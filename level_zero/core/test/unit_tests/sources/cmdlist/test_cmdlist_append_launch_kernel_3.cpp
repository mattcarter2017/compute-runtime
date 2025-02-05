/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_sync_buffer_handler.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/multi_tile_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using CommandListAppendLaunchKernel = Test<ModuleFixture>;

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandListAppendLaunchKernel, givenFunctionWhenBindingTablePrefetchAllowedThenProgramBindingTableEntryCount) {
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    for (auto debugKey : {-1, 0, 1}) {
        DebugManagerStateRestore restore;
        debugManager.flags.ForceBtpPrefetchMode.set(debugKey);

        createKernel();

        ze_group_count_t groupCount{1, 1, 1};
        ze_result_t returnValue;
        std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
        CmdListKernelLaunchParams launchParams = {};
        commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

        auto commandStream = commandList->getCmdContainer().getCommandStream();

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, commandStream->getCpuBase(), commandStream->getUsed()));

        auto itorMIDL = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(itorMIDL, cmdList.end());

        auto cmd = genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(*itorMIDL);
        ASSERT_NE(cmd, nullptr);

        auto dsh = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::dynamicState);
        auto idd = static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(dsh->getCpuBase(), cmd->getInterfaceDescriptorDataStartAddress()));

        if (NEO::EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
            uint32_t numArgs = kernel->kernelImmData->getDescriptor().payloadMappings.bindingTable.numEntries;
            EXPECT_EQ(numArgs, idd->getBindingTableEntryCount());
        } else {
            EXPECT_EQ(0u, idd->getBindingTableEntryCount());
        }
    }
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandListAppendLaunchKernel, givenEventsWhenAppendingKernelThenPostSyncToEventIsGenerated) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);

    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(
        kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_FALSE(cmd->getDcFlushEnable());
            auto gpuAddress = event->getGpuAddress(device);
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            postSyncFound = true;
        }
    }
    EXPECT_TRUE(postSyncFound);

    {
        auto itorEvent = std::find(std::begin(commandList->getCmdContainer().getResidencyContainer()),
                                   std::end(commandList->getCmdContainer().getResidencyContainer()),
                                   event->getAllocation(device));
        EXPECT_NE(itorEvent, std::end(commandList->getCmdContainer().getResidencyContainer()));
    }
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandListAppendLaunchKernel, givenAppendLaunchMultipleKernelsIndirectThenEnablesPredicate) {
    createKernel();

    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    const ze_kernel_handle_t launchKernels = kernel->toHandle();
    uint32_t *numLaunchArgs;
    const ze_group_count_t launchKernelArgs = {1, 1, 1};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(
        device->toHandle(), &deviceDesc, 16384u, 4096u, reinterpret_cast<void **>(&numLaunchArgs));
    result = commandList->appendLaunchMultipleKernelsIndirect(1, &launchKernels, numLaunchArgs, &launchKernelArgs, nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    *numLaunchArgs = 0;
    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));
    auto itorWalker = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);

    auto cmd = genCmdCast<GPGPU_WALKER *>(*itorWalker);
    EXPECT_TRUE(cmd->getPredicateEnable());
    context->freeMem(reinterpret_cast<void *>(numLaunchArgs));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandListAppendLaunchKernel, givenAppendLaunchMultipleKernelsThenUsesMathAndWalker) {
    createKernel();

    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using MI_MATH = typename FamilyType::MI_MATH;
    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    const ze_kernel_handle_t launchKernels[3] = {kernel->toHandle(), kernel->toHandle(), kernel->toHandle()};
    uint32_t *numLaunchArgs;
    const uint32_t numKernels = 3;
    const ze_group_count_t launchKernelArgs[numKernels] = {{1, 1, 1}, {2, 2, 2}, {1, 1, 1}};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(
        device->toHandle(), &deviceDesc, 16384u, 4096u, reinterpret_cast<void **>(&numLaunchArgs));
    result = commandList->appendLaunchMultipleKernelsIndirect(numKernels, launchKernels, numLaunchArgs, launchKernelArgs, nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    *numLaunchArgs = 2;
    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = cmdList.begin();

    for (uint32_t i = 0; i < numKernels; i++) {
        itor = find<MI_MATH *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        itor = find<GPGPU_WALKER *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
    }

    itor = find<MI_MATH *>(itor, cmdList.end());
    ASSERT_EQ(cmdList.end(), itor);
    context->freeMem(reinterpret_cast<void *>(numLaunchArgs));
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelThenKernelIsExecutedOnImmediateCmdQ, MatchAny) {
    createKernel();

    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    CmdListKernelLaunchParams launchParams = {};
    result = commandList0->appendLaunchKernel(
        kernel->toHandle(),
        groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelWithInvalidEventThenInvalidArgumentErrorIsReturned, MatchAny) {
    createKernel();

    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    CmdListKernelLaunchParams launchParams = {};
    result = commandList0->appendLaunchKernel(
        kernel->toHandle(),
        groupCount, nullptr, 1, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenNonemptyAllocPrintfBufferKernelWhenAppendingLaunchKernelIndirectThenKernelIsStoredOnEvent, MatchAny) {
    Mock<Module> module(this->device, nullptr);
    auto kernel = new Mock<::L0::KernelImp>{};
    static_cast<ModuleImp *>(&module)->getPrintfKernelContainer().push_back(std::shared_ptr<Mock<::L0::KernelImp>>{kernel});

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    kernel->module = &module;
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernelIndirect(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_FALSE(event->getKernelForPrintf().expired());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenEmptyAllocPrintfBufferKernelWhenAppendingLaunchKernelIndirectThenKernelIsNotStoredOnEvent, MatchAny) {
    Mock<Module> module(this->device, nullptr);
    auto kernel = new Mock<::L0::KernelImp>{};
    static_cast<ModuleImp *>(&module)->getPrintfKernelContainer().push_back(std::shared_ptr<Mock<::L0::KernelImp>>{kernel});

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    kernel->module = &module;
    kernel->descriptor.kernelAttributes.flags.usesPrintf = false;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernelIndirect(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(nullptr, event->getKernelForPrintf().lock().get());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenNonemptyAllocPrintfBufferKernelWhenAppendingLaunchKernelWithParamThenKernelIsStoredOnEvent, MatchAny) {
    Mock<Module> module(this->device, nullptr);
    auto kernel = new Mock<::L0::KernelImp>{};
    static_cast<ModuleImp *>(&module)->getPrintfKernelContainer().push_back(std::shared_ptr<Mock<::L0::KernelImp>>{kernel});

    ze_result_t returnValue;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    kernel->module = &module;
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    auto result = pCommandList->appendLaunchKernelWithParams(kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_FALSE(event->getKernelForPrintf().expired());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenEmptyAllocPrintfBufferKernelWhenAppendingLaunchKernelWithParamThenKernelIsNotStoredOnEvent, MatchAny) {
    Mock<Module> module(this->device, nullptr);
    auto kernel = new Mock<::L0::KernelImp>{};
    static_cast<ModuleImp *>(&module)->getPrintfKernelContainer().push_back(std::shared_ptr<Mock<::L0::KernelImp>>{kernel});

    ze_result_t returnValue;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    kernel->module = &module;
    kernel->descriptor.kernelAttributes.flags.usesPrintf = false;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    auto result = pCommandList->appendLaunchKernelWithParams(kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(nullptr, event->getKernelForPrintf().lock().get());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelIndirectThenKernelIsExecutedOnImmediateCmdQ, MatchAny) {
    createKernel();
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    result = commandList0->appendLaunchKernelIndirect(
        kernel->toHandle(),
        groupCount, nullptr, 0, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendingLaunchKernelIndirectWithInvalidEventThenInvalidArgumentErrorIsReturned, MatchAny) {
    createKernel();

    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               result));
    ASSERT_NE(nullptr, commandList0);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList0.get());

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    ze_group_count_t groupCount{1, 1, 1};

    result = commandList0->appendLaunchKernelIndirect(
        kernel->toHandle(),
        groupCount, nullptr, 1, nullptr, false);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenKernelUsingSyncBufferWhenAppendLaunchCooperativeKernelIsCalledThenCorrectValueIsReturned, MatchAny) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();
    EXPECT_EQ(std::numeric_limits<size_t>::max(), kernel.getSyncBufferIndex());
    EXPECT_EQ(nullptr, kernel.getSyncBufferAllocation());

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.usesSyncBuffer = true;
    kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto &productHelper = device->getProductHelper();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto engineGroupType = NEO::EngineGroupType::compute;
    if (productHelper.isCooperativeEngineSupported(*defaultHwInfo)) {
        engineGroupType = gfxCoreHelper.getEngineGroupType(aub_stream::EngineType::ENGINE_CCS, EngineUsage::cooperative, *defaultHwInfo);
    }

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    commandList->initialize(device, engineGroupType, 0u);
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(std::numeric_limits<size_t>::max(), cooperativeParams.syncBufferPatchIndex);

    auto mockSyncBufferHandler = reinterpret_cast<MockSyncBufferHandler *>(device->getNEODevice()->syncBufferHandler.get());
    auto syncBufferAllocation = mockSyncBufferHandler->graphicsAllocation;

    EXPECT_NE(std::numeric_limits<size_t>::max(), kernel.getSyncBufferIndex());
    auto syncBufferAllocationIt = std::find(kernel.internalResidencyContainer.begin(), kernel.internalResidencyContainer.end(), syncBufferAllocation);
    ASSERT_NE(kernel.internalResidencyContainer.end(), syncBufferAllocationIt);
    auto expectedIndex = static_cast<size_t>(std::distance(kernel.internalResidencyContainer.begin(), syncBufferAllocationIt));
    EXPECT_EQ(expectedIndex, kernel.getSyncBufferIndex());

    EXPECT_EQ(syncBufferAllocation, kernel.getSyncBufferAllocation());

    auto &cmdsToPatch = commandList->getCommandsToPatch();
    ASSERT_NE(0u, cmdsToPatch.size());

    auto noopParam = cmdsToPatch[cooperativeParams.syncBufferPatchIndex];
    EXPECT_EQ(CommandToPatch::NoopSpace, noopParam.type);
    EXPECT_NE(0u, noopParam.patchSize);

    commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, engineGroupType, 0u);
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = true;
    result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // sync buffer index once set should not change
    EXPECT_EQ(expectedIndex, kernel.getSyncBufferIndex());
    syncBufferAllocationIt = std::find(kernel.internalResidencyContainer.begin(), kernel.internalResidencyContainer.end(), syncBufferAllocation);
    ASSERT_NE(kernel.internalResidencyContainer.end(), syncBufferAllocationIt);
    // verify syncBufferAllocation is added only once
    auto notFoundIt = std::find(syncBufferAllocationIt + 1, kernel.internalResidencyContainer.end(), syncBufferAllocation);
    EXPECT_EQ(kernel.internalResidencyContainer.end(), notFoundIt);

    {
        VariableBackup<std::array<bool, 4>> usesSyncBuffer{&kernelAttributes.flags.packed};
        usesSyncBuffer = {};
        commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
        result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
    {
        VariableBackup<uint32_t> groupCountX{&groupCount.groupCountX};
        uint32_t maximalNumberOfWorkgroupsAllowed = kernel.suggestMaxCooperativeGroupCount(engineGroupType, false);
        groupCountX = maximalNumberOfWorkgroupsAllowed + 1;
        commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        commandList->initialize(device, engineGroupType, 0u);
        result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }
    {
        VariableBackup<bool> cooperative{&launchParams.isCooperative};
        cooperative = false;
        result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    }

    const ze_command_queue_desc_t desc = {};
    std::unique_ptr<L0::CommandList> commandListImmediate(CommandList::createImmediate(productFamily, device, &desc, false, engineGroupType, result));

    cooperativeParams.isCooperative = true;
    cooperativeParams.syncBufferPatchIndex = std::numeric_limits<size_t>::max();

    result = commandListImmediate->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(std::numeric_limits<size_t>::max(), cooperativeParams.syncBufferPatchIndex);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenKernelUsingSyncBufferWhenAppendLaunchCooperativeKernelWithMakeViewIsCalledThenNoAllocationCreated, IsAtLeastXeHpCore) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();
    EXPECT_EQ(std::numeric_limits<size_t>::max(), kernel.getSyncBufferIndex());
    EXPECT_EQ(nullptr, kernel.getSyncBufferAllocation());

    constexpr uint32_t crossThreadDataSize = 64;
    kernel.crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel.crossThreadDataSize = crossThreadDataSize;
    memset(kernel.crossThreadData.get(), 0, crossThreadDataSize);

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.usesSyncBuffer = true;
    kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;

    auto &syncBufferAddress = kernel.immutableData.kernelDescriptor->payloadMappings.implicitArgs.syncBufferAddress;
    syncBufferAddress.stateless = 0x8;
    syncBufferAddress.pointerSize = 8;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto &productHelper = device->getProductHelper();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto engineGroupType = NEO::EngineGroupType::compute;
    if (productHelper.isCooperativeEngineSupported(*defaultHwInfo)) {
        engineGroupType = gfxCoreHelper.getEngineGroupType(aub_stream::EngineType::ENGINE_CCS, EngineUsage::cooperative, *defaultHwInfo);
    }

    auto commandBufferViewPtr = std::make_unique<uint8_t[]>(512);
    auto heapBufferViewPtr = std::make_unique<uint8_t[]>(512);

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;
    cooperativeParams.makeKernelCommandView = true;
    cooperativeParams.cmdWalkerBuffer = commandBufferViewPtr.get();
    cooperativeParams.hostPayloadBuffer = heapBufferViewPtr.get();

    commandList->initialize(device, engineGroupType, 0u);
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto patchPtr = *reinterpret_cast<uint64_t *>(ptrOffset(kernel.crossThreadData.get(), syncBufferAddress.stateless));
    EXPECT_EQ(0u, patchPtr);

    EXPECT_EQ(std::numeric_limits<size_t>::max(), kernel.getSyncBufferIndex());
    EXPECT_EQ(nullptr, kernel.getSyncBufferAllocation());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenKernelUsingRegionGroupBarrierWhenAppendLaunchKernelIsCalledThenPatchBuffer, IsAtLeastXeHpCore) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->storeMakeResidentAllocations = true;

    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();
    EXPECT_EQ(std::numeric_limits<size_t>::max(), kernel.getRegionGroupBarrierIndex());
    EXPECT_EQ(nullptr, kernel.getRegionGroupBarrierAllocation());

    constexpr uint32_t crossThreadDataSize = 64;
    kernel.crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel.crossThreadDataSize = crossThreadDataSize;
    memset(kernel.crossThreadData.get(), 0, crossThreadDataSize);

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.usesRegionGroupBarrier = true;

    auto &regionGroupBarrier = kernel.immutableData.kernelDescriptor->payloadMappings.implicitArgs.regionGroupBarrierBuffer;
    regionGroupBarrier.stateless = 0x8;
    regionGroupBarrier.pointerSize = 8;

    const ze_command_queue_desc_t desc = {};
    ze_result_t result = ZE_RESULT_SUCCESS;

    std::unique_ptr<L0::CommandList> cmdList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, result));

    CmdListKernelLaunchParams launchParams = {};
    launchParams.localRegionSize = 4;
    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(std::numeric_limits<size_t>::max(), launchParams.regionBarrierPatchIndex);

    auto patchPtr = *reinterpret_cast<uint64_t *>(ptrOffset(kernel.crossThreadData.get(), regionGroupBarrier.stateless));
    EXPECT_NE(0u, patchPtr);

    auto allocIter = std::find_if(ultCsr->makeResidentAllocations.begin(), ultCsr->makeResidentAllocations.end(), [patchPtr](const std::pair<GraphicsAllocation *, uint32_t> &element) {
        return element.first->getGpuAddressToPatch() == patchPtr;
    });
    ASSERT_NE(ultCsr->makeResidentAllocations.end(), allocIter);
    auto regionGroupBarrierAllocation = allocIter->first;

    auto regionGroupBarrierAllocIt = std::find(kernel.internalResidencyContainer.begin(), kernel.internalResidencyContainer.end(), regionGroupBarrierAllocation);
    ASSERT_NE(kernel.internalResidencyContainer.end(), regionGroupBarrierAllocIt);
    auto expectedIndex = static_cast<size_t>(std::distance(kernel.internalResidencyContainer.begin(), regionGroupBarrierAllocIt));
    EXPECT_EQ(expectedIndex, kernel.getRegionGroupBarrierIndex());

    EXPECT_EQ(regionGroupBarrierAllocation, kernel.getRegionGroupBarrierAllocation());

    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));

    // region group barrier index once set should not change
    EXPECT_EQ(expectedIndex, kernel.getRegionGroupBarrierIndex());
    regionGroupBarrierAllocIt = std::find(kernel.internalResidencyContainer.begin(), kernel.internalResidencyContainer.end(), regionGroupBarrierAllocation);
    ASSERT_NE(kernel.internalResidencyContainer.end(), regionGroupBarrierAllocIt);
    // verify regionGroupBarrierAllocation is added only once
    auto notFoundIt = std::find(regionGroupBarrierAllocIt + 1, kernel.internalResidencyContainer.end(), regionGroupBarrierAllocation);
    EXPECT_EQ(kernel.internalResidencyContainer.end(), notFoundIt);

    auto patchPtr2 = *reinterpret_cast<uint64_t *>(ptrOffset(kernel.crossThreadData.get(), regionGroupBarrier.stateless));

    size_t requestedNumberOfWorkgroups = groupCount.groupCountX * groupCount.groupCountY * groupCount.groupCountZ;

    auto offset = alignUp((requestedNumberOfWorkgroups / launchParams.localRegionSize) * (launchParams.localRegionSize + 1) * 2 * sizeof(uint32_t), MemoryConstants::cacheLineSize);

    EXPECT_EQ(patchPtr2, patchPtr + offset);

    std::unique_ptr<L0::CommandList> cmdListRegular(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0, result, false));

    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdListRegular->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_NE(std::numeric_limits<size_t>::max(), launchParams.regionBarrierPatchIndex);

    auto &cmdsToPatch = cmdListRegular->getCommandsToPatch();
    ASSERT_NE(0u, cmdsToPatch.size());

    auto noopParam = cmdsToPatch[launchParams.regionBarrierPatchIndex];
    EXPECT_EQ(CommandToPatch::NoopSpace, noopParam.type);
    EXPECT_NE(0u, noopParam.patchSize);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenKernelUsingRegionGroupBarrierWhenAppendLaunchKernelWithMakeViewIsCalledThenNoPatchBuffer, IsAtLeastXeHpCore) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();
    EXPECT_EQ(std::numeric_limits<size_t>::max(), kernel.getRegionGroupBarrierIndex());
    EXPECT_EQ(nullptr, kernel.getRegionGroupBarrierAllocation());

    constexpr uint32_t crossThreadDataSize = 64;
    kernel.crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel.crossThreadDataSize = crossThreadDataSize;
    memset(kernel.crossThreadData.get(), 0, crossThreadDataSize);

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.usesRegionGroupBarrier = true;

    auto &regionGroupBarrier = kernel.immutableData.kernelDescriptor->payloadMappings.implicitArgs.regionGroupBarrierBuffer;
    regionGroupBarrier.stateless = 0x8;
    regionGroupBarrier.pointerSize = 8;

    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_command_list_flags_t flags = 0;

    std::unique_ptr<L0::CommandList> cmdList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, flags, result, false));

    auto commandBufferViewPtr = std::make_unique<uint8_t[]>(512);
    auto heapBufferViewPtr = std::make_unique<uint8_t[]>(512);

    CmdListKernelLaunchParams launchParams = {};
    launchParams.localRegionSize = 4;
    launchParams.makeKernelCommandView = true;
    launchParams.cmdWalkerBuffer = commandBufferViewPtr.get();
    launchParams.hostPayloadBuffer = heapBufferViewPtr.get();

    EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));

    auto patchPtr = *reinterpret_cast<uint64_t *>(ptrOffset(kernel.crossThreadData.get(), regionGroupBarrier.stateless));
    EXPECT_EQ(0u, patchPtr);

    EXPECT_EQ(std::numeric_limits<size_t>::max(), kernel.getRegionGroupBarrierIndex());
    EXPECT_EQ(nullptr, kernel.getRegionGroupBarrierAllocation());
}

HWTEST2_F(CommandListAppendLaunchKernel, whenAppendLaunchCooperativeKernelAndQueryKernelTimestampsToTheSameCmdlistThenFronEndStateIsNotChanged, MatchAny) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.usesSyncBuffer = true;
    kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto &productHelper = device->getProductHelper();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto engineGroupType = NEO::EngineGroupType::compute;
    if (productHelper.isCooperativeEngineSupported(*defaultHwInfo)) {
        engineGroupType = gfxCoreHelper.getEngineGroupType(aub_stream::EngineType::ENGINE_CCS, EngineUsage::cooperative, *defaultHwInfo);
    }
    pCommandList->initialize(device, engineGroupType, 0u);
    pCommandList->isFlushTaskSubmissionEnabled = true;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    ze_result_t returnValue = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto eventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    returnValue = pCommandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, cooperativeParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    void *alloc;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device, &deviceDesc, 128, 1, &alloc);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto eventHandle = event->toHandle();

    result = pCommandList->appendQueryKernelTimestamps(1u, &eventHandle, alloc, nullptr, nullptr, 1u, &eventHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    pCommandList->close();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(pCommandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), pCommandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    auto firstWalker = itor;

    itor++;
    itor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    auto secondWalker = itor;
    itor = find<typename FamilyType::FrontEndStateCommand *>(firstWalker, secondWalker);

    EXPECT_EQ(itor, secondWalker);

    context->freeMem(alloc);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenDisableOverdispatchPropertyWhenUpdateStreamPropertiesIsCalledThenRequiredStateAndFinalStateAreCorrectlySet, MatchAny) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const auto &productHelper = device->getProductHelper();
    int32_t expectedDisableOverdispatch = productHelper.isDisableOverdispatchAvailable(*defaultHwInfo) ? 1 : -1;

    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->requiredStreamState.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->finalStreamState.frontEndState.disableOverdispatch.value);

    const ze_group_count_t launchKernelArgs = {1, 1, 1};
    pCommandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->requiredStreamState.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->finalStreamState.frontEndState.disableOverdispatch.value);

    pCommandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->requiredStreamState.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedDisableOverdispatch, pCommandList->finalStreamState.frontEndState.disableOverdispatch.value);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenCooperativeKernelWhenAppendLaunchCooperativeKernelIsCalledThenCommandListTypeIsProperlySet, MatchAny) {
    createKernel();
    kernel->setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_FALSE(pCommandList->containsCooperativeKernelsFlag);

    pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_TRUE(pCommandList->containsCooperativeKernelsFlag);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenAnyCooperativeKernelAndMixingAllowedWhenAppendLaunchCooperativeKernelIsCalledThenCommandListTypeIsProperlySet, MatchAny) {
    DebugManagerStateRestore restorer;

    createKernel();
    kernel->setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_FALSE(pCommandList->containsCooperativeKernelsFlag);

    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_TRUE(pCommandList->containsCooperativeKernelsFlag);

    launchParams.isCooperative = false;
    result = pCommandList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(pCommandList->containsAnyKernel);
    EXPECT_TRUE(pCommandList->containsCooperativeKernelsFlag);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenCooperativeAndNonCooperativeKernelsAndAllowMixingWhenAppendLaunchCooperativeKernelIsCalledThenReturnSuccess, MatchAny) {
    DebugManagerStateRestore restorer;

    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = false;
    auto result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    launchParams.isCooperative = false;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenNotEnoughSpaceInCommandStreamWhenAppendingKernelWithImmediateListWithoutFlushTaskUnrecoverableIsCalled, IsWithinXeGfxFamily) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    createKernel();

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue)));

    auto &commandContainer = commandList->getCmdContainer();
    const auto stream = commandContainer.getCommandStream();

    Vec3<size_t> groupCount{1, 1, 1};
    auto sizeLeftInStream = sizeof(MI_BATCH_BUFFER_END);
    auto available = stream->getAvailableSpace();
    stream->getSpace(available - sizeLeftInStream);

    const uint32_t threadGroupDimensions[3] = {1, 1, 1};

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs{
        0,                                        // eventAddress
        0,                                        // postSyncImmValue
        0,                                        // inOrderCounterValue
        device->getNEODevice(),                   // device
        nullptr,                                  // inOrderExecInfo
        kernel.get(),                             // dispatchInterface
        nullptr,                                  // surfaceStateHeap
        nullptr,                                  // dynamicStateHeap
        threadGroupDimensions,                    // threadGroupDimensions
        nullptr,                                  // outWalkerPtr
        nullptr,                                  // cpuWalkerBuffer
        nullptr,                                  // cpuPayloadBuffer
        nullptr,                                  // outImplicitArgsPtr
        nullptr,                                  // additionalCommands
        PreemptionMode::MidBatch,                 // preemptionMode
        NEO::RequiredPartitionDim::none,          // requiredPartitionDim
        NEO::RequiredDispatchWalkOrder::none,     // requiredDispatchWalkOrder
        NEO::localRegionSizeParamNotSet,          // localRegionSize
        0,                                        // partitionCount
        0,                                        // reserveExtraPayloadSpace
        1,                                        // maxWgCountPerTile
        NEO::ThreadArbitrationPolicy::NotPresent, // defaultPipelinedThreadArbitrationPolicy
        false,                                    // isIndirect
        false,                                    // isPredicate
        false,                                    // isTimestampEvent
        false,                                    // requiresUncachedMocs
        false,                                    // isInternal
        false,                                    // isCooperative
        false,                                    // isHostScopeSignalEvent
        false,                                    // isKernelUsingSystemAllocation
        false,                                    // isKernelDispatchedFromImmediateCmdList
        false,                                    // isRcs
        commandList->getDcFlushRequired(true),    // dcFlushEnable
        false,                                    // isHeaplessModeEnabled
        false,                                    // isHeaplessStateInitEnabled
        false,                                    // interruptEvent
        false,                                    // immediateScratchAddressPatching
        false,                                    // makeCommandView
    };
    EXPECT_THROW(NEO::EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(commandContainer, dispatchKernelArgs), std::exception);
}

HWTEST_F(CommandListAppendLaunchKernel, givenInvalidKernelWhenAppendingThenReturnErrorInvalidArgument) {
    createKernel();
    const_cast<NEO::KernelDescriptor &>(kernel->getKernelDescriptor()).kernelAttributes.flags.isInvalid = true;
    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_group_count_t groupCount{8, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
}

struct CommandListAppendLaunchKernelWithImplicitArgs : CommandListAppendLaunchKernel {
    template <typename FamilyType>
    uint64_t getIndirectHeapOffsetForImplicitArgsBuffer(const Mock<::L0::KernelImp> &kernel) {
        if (FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
            const auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
            auto implicitArgsProgrammingSize = ImplicitArgsHelper::getSizeForImplicitArgsPatching(kernel.pImplicitArgs.get(), kernel.getKernelDescriptor(), !kernel.kernelRequiresGenerationOfLocalIdsByRuntime, rootDeviceEnvironment);
            return implicitArgsProgrammingSize - ImplicitArgs::getSize();
        } else {
            return 0u;
        }
    }
};

HWTEST_F(CommandListAppendLaunchKernelWithImplicitArgs, givenIndirectDispatchWithImplicitArgsWhenAppendingThenMiMathCommandsForWorkGroupCountAndGlobalWorkSizeAndWorkDimAreProgrammed) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();
    kernel.immutableData.crossThreadDataSize = sizeof(uint64_t);
    kernel.pImplicitArgs.reset(new ImplicitArgs());
    UnitTestHelper<FamilyType>::adjustKernelDescriptorForImplicitArgs(*kernel.immutableData.kernelDescriptor);

    kernel.setGroupSize(1, 1, 1);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(),
                                                     *static_cast<ze_group_count_t *>(alloc),
                                                     nullptr, 0, nullptr, false);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto heap = commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject);
    uint64_t pImplicitArgsGPUVA = heap->getGraphicsAllocation()->getGpuAddress() + getIndirectHeapOffsetForImplicitArgsBuffer<FamilyType>(kernel);

    auto workDimStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    workDimStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::csGprR0);
    workDimStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA);

    auto groupCountXStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountXStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::gpgpuDispatchDimX);
    groupCountXStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, groupCountX));

    auto groupCountYStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountYStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::gpgpuDispatchDimY);
    groupCountYStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, groupCountY));

    auto groupCountZStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    groupCountZStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::gpgpuDispatchDimZ);
    groupCountZStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, groupCountZ));

    auto globalSizeXStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    globalSizeXStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::csGprR1);
    globalSizeXStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, globalSizeX));

    auto globalSizeYStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    globalSizeYStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::csGprR1);
    globalSizeYStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, globalSizeY));

    auto globalSizeZStoreRegisterMemCmd = FamilyType::cmdInitStoreRegisterMem;
    globalSizeZStoreRegisterMemCmd.setRegisterAddress(RegisterOffsets::csGprR1);
    globalSizeZStoreRegisterMemCmd.setMemoryAddress(pImplicitArgsGPUVA + offsetof(ImplicitArgs, globalSizeZ));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), groupCountXStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), groupCountXStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), groupCountYStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), groupCountYStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), groupCountZStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), groupCountZStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), globalSizeXStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), globalSizeXStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), globalSizeYStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), globalSizeYStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), globalSizeZStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), globalSizeZStoreRegisterMemCmd.getMemoryAddress());

    itor = find<MI_LOAD_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    auto cmd2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    auto memoryMaskCmd = FamilyType::cmdInitLoadRegisterImm;
    memoryMaskCmd.setDataDword(0xFF00FFFF);

    EXPECT_EQ(cmd2->getDataDword(), memoryMaskCmd.getDataDword());

    itor++; // MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    cmd2 = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    auto offsetCmd = FamilyType::cmdInitLoadRegisterImm;
    offsetCmd.setDataDword(0x0000FFFF);

    EXPECT_EQ(cmd2->getDataDword(), offsetCmd.getDataDword());

    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor++; // MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor++; // MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor++; // MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), workDimStoreRegisterMemCmd.getRegisterAddress());
    EXPECT_EQ(cmd->getMemoryAddress(), workDimStoreRegisterMemCmd.getMemoryAddress());

    context->freeMem(alloc);
}

using MultiTileImmediateCommandListAppendLaunchKernelXeHpCoreTest = Test<MultiTileImmediateCommandListAppendLaunchKernelFixture>;

HWTEST2_F(MultiTileImmediateCommandListAppendLaunchKernelXeHpCoreTest, givenImplicitScalingWhenUsingImmediateCommandListThenDoNotAddSelfCleanup, IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;

    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.UsePipeControlAfterPartitionedWalker.set(1);

    ze_group_count_t groupCount{128, 1, 1};

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    auto immediateCmdList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    immediateCmdList->cmdListType = ::L0::CommandList::CommandListType::typeImmediate;
    immediateCmdList->isFlushTaskSubmissionEnabled = true;
    immediateCmdList->cmdQImmediate = queue.get();
    auto result = immediateCmdList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = immediateCmdList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto cmdStream = immediateCmdList->getCmdContainer().getCommandStream();

    auto sizeBefore = cmdStream->getUsed();
    result = immediateCmdList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = cmdStream->getUsed();

    uint64_t bbStartGpuAddress = cmdStream->getGraphicsAllocation()->getGpuAddress() + sizeBefore;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), sizeBefore),
        sizeAfter - sizeBefore));

    auto itorWalker = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);
    WalkerVariant walkerCmd = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*itorWalker);

    std::visit([&bbStartGpuAddress](auto &&walker) {
        using WalkerType = std::decay_t<decltype(*walker)>;

        EXPECT_TRUE(walker->getWorkloadPartitionEnable());

        bbStartGpuAddress += sizeof(WalkerType) + sizeof(PIPE_CONTROL) + sizeof(MI_ATOMIC) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait() +
                             sizeof(MI_BATCH_BUFFER_START) + 3 * sizeof(uint32_t);
    },
               walkerCmd);

    auto itorPipeControl = find<PIPE_CONTROL *>(itorWalker, cmdList.end());
    ASSERT_NE(cmdList.end(), itorPipeControl);

    auto itorStoreDataImm = find<MI_STORE_DATA_IMM *>(itorWalker, itorPipeControl);
    EXPECT_EQ(itorPipeControl, itorStoreDataImm);

    auto itorBbStart = find<MI_BATCH_BUFFER_START *>(itorPipeControl, cmdList.end());
    ASSERT_NE(cmdList.end(), itorBbStart);
    auto cmdBbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*itorBbStart);
    EXPECT_EQ(bbStartGpuAddress, cmdBbStart->getBatchBufferStartAddress());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, cmdBbStart->getSecondLevelBatchBuffer());

    auto itorMiAtomic = find<MI_ATOMIC *>(itorBbStart, cmdList.end());
    EXPECT_EQ(cmdList.end(), itorMiAtomic);

    auto itorSemaphoreWait = find<MI_SEMAPHORE_WAIT *>(itorBbStart, cmdList.end());
    EXPECT_EQ(cmdList.end(), itorSemaphoreWait);
}

HWTEST2_F(MultiTileImmediateCommandListAppendLaunchKernelXeHpCoreTest, givenImplicitScalingWhenUsingImmediateCommandListWithoutFlushTaskThenUseSecondaryBuffer, IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    debugManager.flags.UsePipeControlAfterPartitionedWalker.set(1);

    ze_group_count_t groupCount{128, 1, 1};

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    auto immediateCmdList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    immediateCmdList->cmdListType = ::L0::CommandList::CommandListType::typeImmediate;
    immediateCmdList->isFlushTaskSubmissionEnabled = false;
    immediateCmdList->cmdQImmediate = queue.get();
    auto result = immediateCmdList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto cmdStream = immediateCmdList->getCmdContainer().getCommandStream();

    auto sizeBefore = cmdStream->getUsed();
    CmdListKernelLaunchParams launchParams = {};
    result = immediateCmdList->appendLaunchKernelWithParams(kernel.get(), groupCount, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = cmdStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), sizeBefore),
        sizeAfter - sizeBefore));

    auto itorWalker = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);

    WalkerVariant walkerCmd = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*itorWalker);
    std::visit([](auto &&walker) {
        EXPECT_TRUE(walker->getWorkloadPartitionEnable());
    },
               walkerCmd);

    auto itorBbStart = find<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorBbStart);
    auto cmdBbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*itorBbStart);
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH, cmdBbStart->getSecondLevelBatchBuffer());
}
} // namespace ult
} // namespace L0
