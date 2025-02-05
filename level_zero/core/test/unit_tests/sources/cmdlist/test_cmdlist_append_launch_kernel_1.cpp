/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using CommandListAppendLaunchKernelMockModule = Test<ModuleMutableCommandListFixture>;
HWTEST_F(CommandListAppendLaunchKernelMockModule, givenKernelWithIndirectAllocationsAllowedThenCommandListReturnsExpectedIndirectAllocationsAllowed) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DetectIndirectAccessInKernel.set(1);
    mockKernelImmData->kernelDescriptor->kernelAttributes.hasIndirectStatelessAccess = true;
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = false;
    kernel->unifiedMemoryControls.indirectSharedAllocationsAllowed = false;
    kernel->unifiedMemoryControls.indirectHostAllocationsAllowed = true;

    EXPECT_TRUE(kernel->hasIndirectAllocationsAllowed());

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    CmdListKernelLaunchParams launchParams = {};
    {
        returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        EXPECT_TRUE(commandList->hasIndirectAllocationsAllowed());
    }

    {
        returnValue = commandList->reset();
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = false;
        kernel->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;
        kernel->unifiedMemoryControls.indirectHostAllocationsAllowed = false;

        returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        EXPECT_TRUE(commandList->hasIndirectAllocationsAllowed());
    }

    {
        returnValue = commandList->reset();
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = true;
        kernel->unifiedMemoryControls.indirectSharedAllocationsAllowed = false;
        kernel->unifiedMemoryControls.indirectHostAllocationsAllowed = false;

        returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        EXPECT_TRUE(commandList->hasIndirectAllocationsAllowed());
    }
}

using CommandListAppendLaunchKernel = Test<ModuleFixture>;
HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithIndirectAllocationsNotAllowedThenCommandListReturnsExpectedIndirectAllocationsAllowed) {
    createKernel();
    kernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed = false;
    kernel->unifiedMemoryControls.indirectSharedAllocationsAllowed = false;
    kernel->unifiedMemoryControls.indirectHostAllocationsAllowed = false;

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_FALSE(commandList->hasIndirectAllocationsAllowed());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithOldestFirstThreadArbitrationPolicySetUsingSchedulingHintExtensionThenCorrectInternalPolicyIsReturned) {
    createKernel();
    ze_scheduling_hint_exp_desc_t pHint{};
    pHint.flags = ZE_SCHEDULING_HINT_EXP_FLAG_OLDEST_FIRST;
    kernel->setSchedulingHintExp(&pHint);
    ASSERT_EQ(kernel->getKernelDescriptor().kernelAttributes.threadArbitrationPolicy, NEO::ThreadArbitrationPolicy::AgeBased);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithRRThreadArbitrationPolicySetUsingSchedulingHintExtensionThenCorrectInternalPolicyIsReturned) {
    createKernel();
    ze_scheduling_hint_exp_desc_t pHint{};
    pHint.flags = ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN;
    kernel->setSchedulingHintExp(&pHint);
    ASSERT_EQ(kernel->getKernelDescriptor().kernelAttributes.threadArbitrationPolicy, NEO::ThreadArbitrationPolicy::RoundRobin);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithStallRRThreadArbitrationPolicySetUsingSchedulingHintExtensionThenCorrectInternalPolicyIsReturned) {
    createKernel();
    ze_scheduling_hint_exp_desc_t pHint{};
    pHint.flags = ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN;
    kernel->setSchedulingHintExp(&pHint);
    ASSERT_EQ(kernel->getKernelDescriptor().kernelAttributes.threadArbitrationPolicy, NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithThreadArbitrationPolicySetUsingSchedulingHintExtensionTheSameFlagIsUsedToSetCmdListThreadArbitrationPolicy) {

    auto &compilerProductHelper = device->getCompilerProductHelper();
    if (compilerProductHelper.isHeaplessModeEnabled()) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);

    createKernel();
    ze_scheduling_hint_exp_desc_t *pHint = new ze_scheduling_hint_exp_desc_t;
    pHint->pNext = nullptr;
    pHint->flags = ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN;
    kernel->setSchedulingHintExp(pHint);

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, commandList->getFinalStreamState().stateComputeMode.threadArbitrationPolicy.value);
    delete (pHint);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithThreadArbitrationPolicySetUsingSchedulingHintExtensionAndOverrideThreadArbitrationPolicyThenTheLatterIsUsedToSetCmdListThreadArbitrationPolicy) {
    createKernel();
    ze_scheduling_hint_exp_desc_t *pHint = new ze_scheduling_hint_exp_desc_t;
    pHint->pNext = nullptr;
    pHint->flags = ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN;
    kernel->setSchedulingHintExp(pHint);

    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideThreadArbitrationPolicy.set(0);
    debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);

    UnitTestSetter::disableHeapless(restorer);

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, commandList->getFinalStreamState().stateComputeMode.threadArbitrationPolicy.value);
    delete (pHint);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenNotEnoughSpaceInCommandStreamWhenAppendingKernelThenBbEndIsAddedAndNewCmdBufferAllocated, MatchAny) {
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restorer;
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);

    createKernel();

    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));

    auto &commandContainer = commandList->getCmdContainer();
    const auto stream = commandContainer.getCommandStream();
    const auto streamCpu = stream->getCpuBase();

    Vec3<size_t> groupCount{1, 1, 1};
    auto sizeLeftInStream = sizeof(MI_BATCH_BUFFER_END);
    auto available = stream->getAvailableSpace();
    stream->getSpace(available - sizeLeftInStream);
    auto bbEndPosition = stream->getSpace(0);

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
    NEO::EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(commandContainer, dispatchKernelArgs);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, 0u);

    const auto streamCpu2 = stream->getCpuBase();

    EXPECT_NE(nullptr, streamCpu2);
    EXPECT_NE(streamCpu, streamCpu2);

    EXPECT_EQ(2u, commandContainer.getCmdBufferAllocations().size());

    GenCmdList cmdList;
    FamilyType::Parse::parseCommandBuffer(cmdList, bbEndPosition, 2 * sizeof(MI_BATCH_BUFFER_END));
    auto itor = find<MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfUsedWhenAppendedToCommandListThenKernelIsStored) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));

    EXPECT_EQ(1u, commandList->getPrintfKernelContainer().size());
    EXPECT_EQ(kernel, commandList->getPrintfKernelContainer()[0].lock().get());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfUsedWhenAppendedToCommandListMultipleTimesThenKernelIsStoredOnce) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));

    EXPECT_EQ(1u, commandList->getPrintfKernelContainer().size());
    EXPECT_EQ(kernel, commandList->getPrintfKernelContainer()[0].lock().get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(1u, commandList->getPrintfKernelContainer().size());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfWhenAppendedToSynchronousImmCommandListThenPrintfBufferIsPrinted) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_FALSE(kernel->hangDetectedPassedToPrintfOutput);
    EXPECT_EQ(0u, commandList->getPrintfKernelContainer().size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(2u, kernel->printPrintfOutputCalledTimes);
    EXPECT_FALSE(kernel->hangDetectedPassedToPrintfOutput);
    EXPECT_EQ(0u, commandList->getPrintfKernelContainer().size());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfWhenAppendedToAsynchronousImmCommandListThenPrintfBufferIsNotPrintedUntilHostSync) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_FALSE(kernel->hangDetectedPassedToPrintfOutput);
    EXPECT_EQ(0u, commandList->getPrintfKernelContainer().size());

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(2u, kernel->printPrintfOutputCalledTimes);
    EXPECT_FALSE(kernel->hangDetectedPassedToPrintfOutput);
    EXPECT_EQ(0u, commandList->getPrintfKernelContainer().size());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfWhenAppendToSynchronousImmCommandListHangsThenPrintfBufferIsPrinted) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    TaskCountType currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;
    csr.callBaseWaitForCompletionWithTimeout = false;
    csr.returnWaitForCompletionWithTimeout = WaitStatus::gpuHang;

    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_TRUE(kernel->hangDetectedPassedToPrintfOutput);
    EXPECT_EQ(0u, commandList->getPrintfKernelContainer().size());

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(2u, kernel->printPrintfOutputCalledTimes);
    EXPECT_TRUE(kernel->hangDetectedPassedToPrintfOutput);
    EXPECT_EQ(0u, commandList->getPrintfKernelContainer().size());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfAppendedToCommandListAndDestroyKernelAfterQueueSyncThenSuccessIsReturned) {
    ze_result_t result;

    ze_command_queue_handle_t commandQueueHandle;
    ze_command_queue_desc_t queueDesc{};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&queueDesc, &commandQueueHandle));
    auto commandQueue = static_cast<L0::ult::CommandQueue *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->close());
    EXPECT_EQ(1u, commandList->getPrintfKernelContainer().size());
    EXPECT_EQ(kernel, commandList->getPrintfKernelContainer()[0].lock().get());

    auto commandListHandle = commandList->toHandle();
    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->synchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->destroy());
    EXPECT_EQ(nullptr, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
    EXPECT_EQ(1u, commandList->getPrintfKernelContainer().size());
    EXPECT_EQ(nullptr, commandList->getPrintfKernelContainer()[0].lock().get());

    commandQueue->destroy();
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfAndEventAppendedToCommandListAndDestroyKernelAfterQueueSyncThenSuccessIsReturnedAndEventSyncDoesNotAccessKernel) {
    ze_result_t result;

    ze_command_queue_handle_t commandQueueHandle;
    ze_command_queue_desc_t queueDesc{};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&queueDesc, &commandQueueHandle));
    auto commandQueue = static_cast<L0::ult::CommandQueue *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    ze_event_pool_desc_t eventPoolDesc{};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.pNext = nullptr;
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    auto eventPool = std::unique_ptr<::L0::EventPool>(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc{};
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.pNext = nullptr;
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<::L0::Event>(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    auto hEventHandle = event->toHandle();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, hEventHandle, 0, nullptr, launchParams, false));
    EXPECT_EQ(kernel, event->getKernelForPrintf().lock().get());
    EXPECT_NE(nullptr, event->getKernelWithPrintfDeviceMutex());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->close());
    EXPECT_EQ(1u, commandList->getPrintfKernelContainer().size());
    EXPECT_EQ(kernel, commandList->getPrintfKernelContainer()[0].lock().get());

    auto commandListHandle = commandList->toHandle();
    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->synchronize(std::numeric_limits<uint64_t>::max()));
    *reinterpret_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->destroy());
    EXPECT_EQ(nullptr, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
    EXPECT_EQ(1u, commandList->getPrintfKernelContainer().size());
    EXPECT_EQ(nullptr, commandList->getPrintfKernelContainer()[0].lock().get());
    EXPECT_EQ(nullptr, event->getKernelForPrintf().lock().get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(nullptr, event->getKernelWithPrintfDeviceMutex());

    commandQueue->destroy();
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfAndEventAppendedToCommandListAndDestroyKernelAfterEventSyncThenSuccessIsReturnedAndQueueSyncDoesNotAccessKernel) {
    ze_result_t result;

    ze_command_queue_handle_t commandQueueHandle;
    ze_command_queue_desc_t queueDesc{};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&queueDesc, &commandQueueHandle));
    auto commandQueue = static_cast<L0::ult::CommandQueue *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    ze_event_pool_desc_t eventPoolDesc{};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.pNext = nullptr;
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    auto eventPool = std::unique_ptr<::L0::EventPool>(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc{};
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.pNext = nullptr;
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<::L0::Event>(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    auto hEventHandle = event->toHandle();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, hEventHandle, 0, nullptr, launchParams, false));
    EXPECT_EQ(kernel, event->getKernelForPrintf().lock().get());
    EXPECT_NE(nullptr, event->getKernelWithPrintfDeviceMutex());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->close());
    EXPECT_EQ(1u, commandList->getPrintfKernelContainer().size());
    EXPECT_EQ(kernel, commandList->getPrintfKernelContainer()[0].lock().get());

    auto commandListHandle = commandList->toHandle();
    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr));
    *reinterpret_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(nullptr, event->getKernelForPrintf().lock().get());
    EXPECT_EQ(nullptr, event->getKernelWithPrintfDeviceMutex());
    EXPECT_EQ(1u, commandList->getPrintfKernelContainer().size());
    EXPECT_EQ(kernel, commandList->getPrintfKernelContainer()[0].lock().get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->destroy());
    EXPECT_EQ(nullptr, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
    EXPECT_EQ(1u, commandList->getPrintfKernelContainer().size());
    EXPECT_EQ(nullptr, commandList->getPrintfKernelContainer()[0].lock().get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->synchronize(std::numeric_limits<uint64_t>::max()));

    commandQueue->destroy();
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfAppendedToImmCommandListWithoutFlushTaskSubmissionAndDestroyKernelAfterListSyncThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_result_t result;
    ze_command_queue_desc_t queueDesc{};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->destroy());
    EXPECT_EQ(nullptr, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfAndEventAppendedToImmCommandListWithoutFlushTaskSubmissionAndDestroyKernelAfterListSyncThenSuccessIsReturnedAndEventSyncDoesNotAccessKernel) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_result_t result;
    ze_command_queue_desc_t queueDesc{};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    ze_event_pool_desc_t eventPoolDesc{};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.pNext = nullptr;
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    auto eventPool = std::unique_ptr<::L0::EventPool>(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc{};
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.pNext = nullptr;
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<::L0::Event>(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    auto hEventHandle = event->toHandle();

    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, hEventHandle, 0, nullptr, launchParams, false));
    EXPECT_EQ(kernel, event->getKernelForPrintf().lock().get());
    EXPECT_NE(nullptr, event->getKernelWithPrintfDeviceMutex());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    *reinterpret_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->destroy());
    EXPECT_EQ(nullptr, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
    EXPECT_EQ(nullptr, event->getKernelForPrintf().lock().get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(nullptr, event->getKernelWithPrintfDeviceMutex());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfAndEventAppendedToImmCommandListWithoutFlushTaskSubmissionAndDestroyKernelAfterEventSyncThenSuccessIsReturnedAndListSyncDoesNotAccessKernel) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_result_t result;
    ze_command_queue_desc_t queueDesc{};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    ze_event_pool_desc_t eventPoolDesc{};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.pNext = nullptr;
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    auto eventPool = std::unique_ptr<::L0::EventPool>(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc{};
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.pNext = nullptr;
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<::L0::Event>(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    auto hEventHandle = event->toHandle();

    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, hEventHandle, 0, nullptr, launchParams, false));
    EXPECT_EQ(kernel, event->getKernelForPrintf().lock().get());
    EXPECT_NE(nullptr, event->getKernelWithPrintfDeviceMutex());
    *reinterpret_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(2u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(nullptr, event->getKernelForPrintf().lock().get());
    EXPECT_EQ(nullptr, event->getKernelWithPrintfDeviceMutex());
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->destroy());
    EXPECT_EQ(nullptr, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfAppendedToImmCommandListWithFlushTaskSubmissionAndDestroyKernelAfterListSyncThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_result_t result;
    ze_command_queue_desc_t queueDesc{};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->destroy());
    EXPECT_EQ(nullptr, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfAndEventAppendedToImmCommandListWithFlushTaskSubmissionAndDestroyKernelAfterListSyncThenSuccessIsReturnedAndEventSyncDoesNotAccessKernel) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_result_t result;
    ze_command_queue_desc_t queueDesc{};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    ze_event_pool_desc_t eventPoolDesc{};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.pNext = nullptr;
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    auto eventPool = std::unique_ptr<::L0::EventPool>(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc{};
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.pNext = nullptr;
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<::L0::Event>(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    auto hEventHandle = event->toHandle();

    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, hEventHandle, 0, nullptr, launchParams, false));
    EXPECT_EQ(kernel, event->getKernelForPrintf().lock().get());
    EXPECT_NE(nullptr, event->getKernelWithPrintfDeviceMutex());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    *reinterpret_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->destroy());
    EXPECT_EQ(nullptr, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
    EXPECT_EQ(nullptr, event->getKernelForPrintf().lock().get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->queryStatus());
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(nullptr, event->getKernelWithPrintfDeviceMutex());
}

HWTEST_F(CommandListAppendLaunchKernel, givenKernelWithPrintfAndEventAppendedToImmCommandListWithFlushTaskSubmissionAndDestroyKernelAfterEventSyncThenSuccessIsReturnedAndListSyncDoesNotAccessKernel) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_result_t result;
    ze_command_queue_desc_t queueDesc{};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto kernel = new Mock<KernelImp>{};
    kernel->module = module.get();
    kernel->descriptor.kernelAttributes.flags.usesPrintf = true;
    kernel->createPrintfBuffer();
    static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer().push_back(std::shared_ptr<Kernel>{kernel});

    ze_event_pool_desc_t eventPoolDesc{};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.pNext = nullptr;
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    auto eventPool = std::unique_ptr<::L0::EventPool>(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc{};
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.pNext = nullptr;
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<::L0::Event>(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    auto hEventHandle = event->toHandle();

    EXPECT_EQ(0u, kernel->printPrintfOutputCalledTimes);
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel->toHandle(), groupCount, hEventHandle, 0, nullptr, launchParams, false));
    EXPECT_EQ(kernel, event->getKernelForPrintf().lock().get());
    EXPECT_NE(nullptr, event->getKernelWithPrintfDeviceMutex());
    *reinterpret_cast<uint32_t *>(event->getHostAddress()) = Event::STATE_SIGNALED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, event->hostSynchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_EQ(nullptr, event->getKernelForPrintf().lock().get());
    EXPECT_EQ(nullptr, event->getKernelWithPrintfDeviceMutex());
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->destroy());
    EXPECT_EQ(nullptr, static_cast<ModuleImp *>(module.get())->getPrintfKernelContainer()[0].get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));
}

HWTEST_F(CommandListAppendLaunchKernel, WhenAppendingMultipleTimesThenSshIsNotDepletedButReallocated) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(0);
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(0);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset();

    createKernel();
    ze_result_t returnValue;

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};

    auto kernelSshSize = alignUp(kernel->getSurfaceStateHeapDataSize(), FamilyType::cacheLineSize);
    auto ssh = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState);
    auto sshHeapSize = ssh->getMaxAvailableSpace();
    auto initialAllocation = ssh->getGraphicsAllocation();
    EXPECT_NE(nullptr, initialAllocation);
    const_cast<KernelDescriptor::AddressingMode &>(kernel->getKernelDescriptor().kernelAttributes.bufferAddressingMode) = KernelDescriptor::BindfulAndStateless;
    CmdListKernelLaunchParams launchParams = {};
    for (size_t i = 0; i < sshHeapSize / kernelSshSize + 1; i++) {
        auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }

    auto reallocatedAllocation = ssh->getGraphicsAllocation();
    EXPECT_NE(nullptr, reallocatedAllocation);
    EXPECT_NE(initialAllocation, reallocatedAllocation);
}

HWTEST_F(CommandListAppendLaunchKernel, WhenAppendingMultipleTimesThenDshIsNotDepletedButReallocated) {
    if (!neoDevice->getDeviceInfo().imageSupport) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(0);
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(0);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset();

    createKernel();
    ze_result_t returnValue;

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};

    size_t size = kernel->getKernelDescriptor().payloadMappings.samplerTable.tableOffset -
                  kernel->getKernelDescriptor().payloadMappings.samplerTable.borderColor;

    constexpr auto samplerStateSize = sizeof(typename FamilyType::SAMPLER_STATE);
    const auto numSamplers = kernel->getKernelDescriptor().payloadMappings.samplerTable.numSamplers;
    size += numSamplers * samplerStateSize;

    auto kernelDshSize = alignUp(size, FamilyType::cacheLineSize);
    auto dsh = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::dynamicState);
    auto dshHeapSize = dsh->getMaxAvailableSpace();
    auto initialAllocation = dsh->getGraphicsAllocation();
    EXPECT_NE(nullptr, initialAllocation);
    const_cast<KernelDescriptor::AddressingMode &>(kernel->getKernelDescriptor().kernelAttributes.bufferAddressingMode) = KernelDescriptor::BindfulAndStateless;
    CmdListKernelLaunchParams launchParams = {};
    for (size_t i = 0; i < dshHeapSize / kernelDshSize + 1; i++) {
        auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }

    auto reallocatedAllocation = dsh->getGraphicsAllocation();
    EXPECT_NE(nullptr, reallocatedAllocation);
    EXPECT_NE(initialAllocation, reallocatedAllocation);
}

using TimestampEventSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(CommandListAppendLaunchKernel, givenTimestampEventsWhenAppendingKernelThenSRMAndPCEncoded, TimestampEventSupport) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);

    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    auto eventPool = std::unique_ptr<::L0::EventPool>(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<::L0::Event>(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

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

    auto itor = find<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(RegisterOffsets::globalTimestampLdw, cmd->getSourceRegisterAddress());
    }
    itor++;

    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(RegisterOffsets::gpThreadTimeRegAddressOffsetLow, cmd->getSourceRegisterAddress());
    }
    itor++;

    itor = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    itor++;

    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*itor);
        EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
        EXPECT_TRUE(cmd->getDcFlushEnable());
    }
    itor++;

    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(RegisterOffsets::globalTimestampLdw, cmd->getSourceRegisterAddress());
    }
    itor++;

    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(RegisterOffsets::gpThreadTimeRegAddressOffsetLow, cmd->getSourceRegisterAddress());
    }
    itor++;

    auto numPCs = findAll<PIPE_CONTROL *>(itor, cmdList.end());
    // we should not have PC when signal scope is device
    ASSERT_EQ(0u, numPCs.size());

    {
        auto itorEvent = std::find(std::begin(commandList->getCmdContainer().getResidencyContainer()),
                                   std::end(commandList->getCmdContainer().getResidencyContainer()),
                                   event->getAllocation(device));
        EXPECT_NE(itorEvent, std::end(commandList->getCmdContainer().getResidencyContainer()));
    }
}

HWTEST2_F(CommandListAppendLaunchKernel, givenKernelLaunchWithTSEventAndScopeFlagHostThenPCWithDCFlushEncoded, TimestampEventSupport) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        ZE_EVENT_SCOPE_FLAG_HOST,
        ZE_EVENT_SCOPE_FLAG_HOST};

    auto eventPool = std::unique_ptr<::L0::EventPool>(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<::L0::Event>(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

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

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());

    PIPE_CONTROL *cmd = genCmdCast<PIPE_CONTROL *>(*itorPC[itorPC.size() - 1]);
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(cmd->getDcFlushEnable());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenForcePipeControlPriorToWalkerKeyThenAdditionalPCIsAdded, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandListBase(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto usedSpaceBefore = commandListBase->getCmdContainer().getCommandStream()->getUsed();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandListBase->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandListBase->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdListBase;
    EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdListBase, ptrOffset(commandListBase->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdListBase.begin(), cmdListBase.end());

    size_t numberOfPCsBase = itorPC.size();

    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePipeControlPriorToWalker.set(1);

    std::unique_ptr<L0::CommandList> commandListWithDebugKey(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    usedSpaceBefore = commandListWithDebugKey->getCmdContainer().getCommandStream()->getUsed();

    result = commandListWithDebugKey->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    usedSpaceAfter = commandListWithDebugKey->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdListBaseWithDebugKey;
    EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdListBaseWithDebugKey, ptrOffset(commandListWithDebugKey->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    itorPC = findAll<PIPE_CONTROL *>(cmdListBaseWithDebugKey.begin(), cmdListBaseWithDebugKey.end());

    size_t numberOfPCsWithDebugKey = itorPC.size();

    EXPECT_EQ(numberOfPCsWithDebugKey, numberOfPCsBase + 1);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenForcePipeControlPriorToWalkerKeyAndNoSpaceThenNewBatchBufferAllocationIsUsed, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePipeControlPriorToWalker.set(1);

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    ze_result_t result = ZE_RESULT_SUCCESS;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, result, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto firstBatchBufferAllocation = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation();

    auto useSize = commandList->getCmdContainer().getCommandStream()->getAvailableSpace();
    useSize -= sizeof(PIPE_CONTROL);
    commandList->getCmdContainer().getCommandStream()->getSpace(useSize);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto secondBatchBufferAllocation = commandList->getCmdContainer().getCommandStream()->getGraphicsAllocation();

    EXPECT_NE(firstBatchBufferAllocation, secondBatchBufferAllocation);
}

using SupportedPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_DG1>;
HWTEST2_F(CommandListAppendLaunchKernel, givenCommandListWhenAppendLaunchKernelSeveralTimesThenAlwaysFirstEventPacketIsUsed, SupportedPlatforms) {
    createKernel();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    const ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,
        ZE_EVENT_SCOPE_FLAG_HOST,
        ZE_EVENT_SCOPE_FLAG_HOST};

    auto eventPool = std::unique_ptr<::L0::EventPool>(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<::L0::Event>(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    EXPECT_EQ(1u, event->getPacketsInUse());
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    for (uint32_t i = 0; i < NEO::TimestampPacketConstants::preferredPacketCount + 4; i++) {
        auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
    EXPECT_EQ(1u, event->getPacketsInUse());
}

HWTEST_F(CommandListAppendLaunchKernel, givenIndirectDispatchWhenAppendingThenWorkGroupCountAndGlobalWorkSizeAndWorkDimIsSetInCrossThreadData) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    kernel.groupSize[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.globalWorkSize[0] = 2;
    kernel.descriptor.payloadMappings.dispatchTraits.workDim = 4;
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

    kernel.groupSize[2] = 2;
    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(),
                                                     *static_cast<ze_group_count_t *>(alloc),
                                                     nullptr, 0, nullptr, false);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

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

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor++; // MI_MATH_ALU_INST_INLINE doesn't have tagMI_COMMAND_OPCODE, can't find it in cmdList
    EXPECT_NE(itor, cmdList.end());
    itor++;
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end()); // kernel with groupSize[2] = 2
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_REG *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    itor = find<MI_LOAD_REGISTER_IMM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    itor = find<MI_STORE_REGISTER_MEM *>(++itor, cmdList.end());
    EXPECT_NE(itor, cmdList.end());

    context->freeMem(alloc);
}

HWTEST_F(CommandListAppendLaunchKernel, givenCommandListWhenResetCalledThenStateIsCleaned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableStateBaseAddressTracking.set(0);

    auto bindlessHeapsHelper = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.get();

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    createKernel();

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);
    ASSERT_NE(nullptr, commandList->getCmdContainer().getCommandStream());

    auto commandListControl = std::unique_ptr<CommandList>(CommandList::whiteboxCast(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandListControl);
    ASSERT_NE(nullptr, commandListControl->getCmdContainer().getCommandStream());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(
        kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->reset();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ASSERT_EQ(device, commandList->device);
    ASSERT_NE(nullptr, commandList->getCmdContainer().getCommandStream());
    ASSERT_GE(commandListControl->getCmdContainer().getCmdBufferAllocations()[0]->getUnderlyingBufferSize(), commandList->getCmdContainer().getCmdBufferAllocations()[0]->getUnderlyingBufferSize());
    ASSERT_EQ(commandListControl->getCmdContainer().getResidencyContainer().size(),
              commandList->getCmdContainer().getResidencyContainer().size());
    ASSERT_EQ(commandListControl->getCmdContainer().getDeallocationContainer().size(),
              commandList->getCmdContainer().getDeallocationContainer().size());
    ASSERT_EQ(commandListControl->getPrintfKernelContainer().size(),
              commandList->getPrintfKernelContainer().size());
    ASSERT_EQ(commandListControl->getCmdContainer().getCommandStream()->getUsed(), commandList->getCmdContainer().getCommandStream()->getUsed());
    ASSERT_EQ(commandListControl->getCmdContainer().slmSizeRef(), commandList->getCmdContainer().slmSizeRef());

    for (uint32_t i = 0; i < NEO::HeapType::numTypes; i++) {
        auto heapType = static_cast<NEO::HeapType>(i);
        if (NEO::HeapType::dynamicState == heapType && !device->getHwInfo().capabilityTable.supportsImages) {
            ASSERT_EQ(nullptr, commandListControl->getCmdContainer().getIndirectHeapAllocation(heapType));
            ASSERT_EQ(nullptr, commandListControl->getCmdContainer().getIndirectHeap(heapType));
        } else if (NEO::HeapType::surfaceState == heapType && bindlessHeapsHelper) {
            ASSERT_EQ(nullptr, commandListControl->getCmdContainer().getIndirectHeapAllocation(heapType));
            ASSERT_EQ(nullptr, commandListControl->getCmdContainer().getIndirectHeap(heapType));
        } else {
            ASSERT_NE(nullptr, commandListControl->getCmdContainer().getIndirectHeapAllocation(heapType));
            ASSERT_NE(nullptr, commandList->getCmdContainer().getIndirectHeapAllocation(heapType));
            ASSERT_EQ(commandListControl->getCmdContainer().getIndirectHeapAllocation(heapType)->getUnderlyingBufferSize(),
                      commandList->getCmdContainer().getIndirectHeapAllocation(heapType)->getUnderlyingBufferSize());

            ASSERT_NE(nullptr, commandListControl->getCmdContainer().getIndirectHeap(heapType));
            ASSERT_NE(nullptr, commandList->getCmdContainer().getIndirectHeap(heapType));
            ASSERT_EQ(commandListControl->getCmdContainer().getIndirectHeap(heapType)->getUsed(),
                      commandList->getCmdContainer().getIndirectHeap(heapType)->getUsed());

            ASSERT_EQ(commandListControl->getCmdContainer().isHeapDirty(heapType), commandList->getCmdContainer().isHeapDirty(heapType));
        }
    }

    auto &compilerProductHelper = device->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled();
    auto heaplessStateInitEnabled = compilerProductHelper.isHeaplessStateInitEnabled(heaplessEnabled);

    if (!heaplessStateInitEnabled) {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

        auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), itor);
    }
}

HWTEST_F(CommandListAppendLaunchKernel, WhenAddingKernelsThenResidencyContainerDoesNotContainDuplicatesAfterClosingCommandList) {
    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    for (int i = 0; i < 4; ++i) {
        auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }

    commandList->close();

    uint32_t it = 0;
    const auto &residencyCont = commandList->getCmdContainer().getResidencyContainer();
    for (auto alloc : residencyCont) {
        auto occurences = std::count(residencyCont.begin(), residencyCont.end(), alloc);
        EXPECT_EQ(1U, static_cast<uint32_t>(occurences)) << it;
        ++it;
    }
}

HWTEST_F(CommandListAppendLaunchKernel, givenSingleValidWaitEventsThenAddSemaphoreToCommandStream) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList->getCmdContainer().getCommandStream());
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    std::unique_ptr<::L0::EventPool> eventPool(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    std::unique_ptr<::L0::Event> event(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    ze_event_handle_t hEventHandle = event->toHandle();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 1, &hEventHandle, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    {
        auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itor);
        EXPECT_EQ(cmd->getCompareOperation(),
                  MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(static_cast<uint32_t>(-1), cmd->getSemaphoreDataDword());
        auto addressSpace = device->getHwInfo().capabilityTable.gpuAddressSpace;

        uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);

        EXPECT_EQ(gpuAddress & addressSpace, cmd->getSemaphoreGraphicsAddress() & addressSpace);
    }
}

HWTEST_F(CommandListAppendLaunchKernel, givenMultipleValidWaitEventsThenAddSemaphoreCommands) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<L0::CommandList>(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList->getCmdContainer().getCommandStream());
    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc1 = {};
    eventDesc1.index = 0;

    ze_event_desc_t eventDesc2 = {};
    eventDesc2.index = 1;

    std::unique_ptr<::L0::EventPool> eventPool(::L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    std::unique_ptr<::L0::Event> event1(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc1, device));
    std::unique_ptr<::L0::Event> event2(::L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc2, device));
    ze_event_handle_t hEventHandle1 = event1->toHandle();
    ze_event_handle_t hEventHandle2 = event2->toHandle();

    ze_event_handle_t waitEvents[2];
    waitEvents[0] = hEventHandle1;
    waitEvents[1] = hEventHandle2;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 2, waitEvents, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto itor = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_FALSE(itor.empty());
    ASSERT_EQ(2, static_cast<int>(itor.size()));
}

HWTEST_F(CommandListAppendLaunchKernel, givenInvalidEventListWhenAppendLaunchCooperativeKernelIsCalledThenErrorIsReturned) {
    createKernel();

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 1, nullptr, cooperativeParams, false);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendLaunchCooperativeKernelUsingFlushTaskThenExpectCorrectExecuteCall, MatchAny) {
    createKernel();
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.isFlushTaskSubmissionEnabled = true;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();

    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    returnValue = cmdList.appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);
    EXPECT_EQ(0u, cmdList.executeCommandListImmediateCalledCount);
    EXPECT_EQ(1u, cmdList.executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenImmediateCommandListWhenAppendLaunchCooperativeKernelNotUsingFlushTaskThenExpectCorrectExecuteCall, MatchAny) {
    createKernel();
    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.isFlushTaskSubmissionEnabled = false;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;

    CmdListKernelLaunchParams cooperativeParams = {};
    cooperativeParams.isCooperative = true;

    returnValue = cmdList.appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, cooperativeParams, false);
    EXPECT_EQ(1u, cmdList.executeCommandListImmediateCalledCount);
    EXPECT_EQ(0u, cmdList.executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

HWTEST2_F(CommandListAppendLaunchKernel, whenUpdateStreamPropertiesIsCalledThenCorrectThreadArbitrationPolicyIsSet, MatchAny) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto expectedThreadArbitrationPolicy = gfxCoreHelper.getDefaultThreadArbitrationPolicy();
    int32_t threadArbitrationPolicyValues[] = {
        ThreadArbitrationPolicy::AgeBased, ThreadArbitrationPolicy::RoundRobin,
        ThreadArbitrationPolicy::RoundRobinAfterDependency};

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(-1, commandList->requiredStreamState.stateComputeMode.threadArbitrationPolicy.value);
    EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.threadArbitrationPolicy.value);

    const ze_group_count_t launchKernelArgs = {};
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_EQ(expectedThreadArbitrationPolicy, commandList->finalStreamState.stateComputeMode.threadArbitrationPolicy.value);

    for (auto threadArbitrationPolicy : threadArbitrationPolicyValues) {
        debugManager.flags.OverrideThreadArbitrationPolicy.set(threadArbitrationPolicy);
        commandList->reset();
        commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
        EXPECT_EQ(threadArbitrationPolicy, commandList->finalStreamState.stateComputeMode.threadArbitrationPolicy.value);
    }
}

HWTEST2_F(CommandListAppendLaunchKernelMockModule,
          givenFlagOmitKernelArgumentResourcePassToCmdlistResidencyWhenAppendingKernelThenExpectNoKernelArgumentsInCmdlistResidency,
          IsAtLeastXeHpCore) {
    NEO::MockGraphicsAllocation mockAllocation;
    NEO::GraphicsAllocation *allocation = &mockAllocation;
    kernel->argumentsResidencyContainer.push_back(allocation);

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    CmdListKernelLaunchParams launchParams = {};
    launchParams.omitAddingKernelArgumentResidency = true;
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto &cmdlistResidency = commandList->getCmdContainer().getResidencyContainer();

    auto kernelAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), allocation);
    EXPECT_EQ(kernelAllocationIt, cmdlistResidency.end());

    launchParams.omitAddingKernelArgumentResidency = false;
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    kernelAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), allocation);
    EXPECT_NE(kernelAllocationIt, cmdlistResidency.end());
}

HWTEST2_F(CommandListAppendLaunchKernelMockModule,
          givenFlagOmitKernelInternalResourcePassToCmdlistResidencyWhenAppendingKernelThenExpectNoKernelInternalsInCmdlistResidency,
          IsAtLeastXeHpCore) {
    NEO::MockGraphicsAllocation mockAllocation;
    NEO::GraphicsAllocation *allocation = &mockAllocation;
    kernel->internalResidencyContainer.push_back(allocation);

    auto kernelIsaAllocation = kernel->getImmutableData()->getIsaGraphicsAllocation();

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    CmdListKernelLaunchParams launchParams = {};
    launchParams.omitAddingKernelInternalResidency = true;
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto &cmdlistResidency = commandList->getCmdContainer().getResidencyContainer();

    auto kernelAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), allocation);
    EXPECT_EQ(kernelAllocationIt, cmdlistResidency.end());
    kernelAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), kernelIsaAllocation);
    EXPECT_EQ(kernelAllocationIt, cmdlistResidency.end());

    launchParams.omitAddingKernelInternalResidency = false;
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    kernelAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), allocation);
    EXPECT_NE(kernelAllocationIt, cmdlistResidency.end());
    kernelAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), kernelIsaAllocation);
    EXPECT_NE(kernelAllocationIt, cmdlistResidency.end());
}

HWTEST2_F(CommandListAppendLaunchKernelMockModule,
          givenOutWalkerPtrDispatchParamWhenAppendingKernelThenSetPtrToWalkerCmd,
          IsAtLeastXeHpCore) {

    ze_group_count_t groupCount{1, 1, 1};
    ze_result_t returnValue;
    CmdListKernelLaunchParams launchParams = {};
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();

    ASSERT_NE(nullptr, launchParams.outWalker);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        usedSpaceAfter));

    auto itorWalker = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);

    EXPECT_EQ(*itorWalker, launchParams.outWalker);
}

HWTEST2_F(CommandListAppendLaunchKernelMockModule,
          givenFlagOmitEventResourcePassToCmdlistResidencyWhenAppendingKernelThenExpectNoEventInCmdlistResidency,
          IsAtLeastXeHpCore) {
    ze_result_t returnValue;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    ze_event_handle_t event = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event));
    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));

    auto eventAllocation = eventObject->getAllocation(device);
    ASSERT_NE(nullptr, eventAllocation);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.omitAddingEventResidency = true;
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, event, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto &cmdlistResidency = commandList->getCmdContainer().getResidencyContainer();

    auto eventAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), eventAllocation);
    EXPECT_EQ(eventAllocationIt, cmdlistResidency.end());

    launchParams.omitAddingEventResidency = false;
    returnValue = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, event, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    eventAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), eventAllocation);
    EXPECT_NE(eventAllocationIt, cmdlistResidency.end());
}

} // namespace ult
} // namespace L0
