/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_command_encoder.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {

using CommandListAppendSignalEvent = Test<CommandListFixture>;
using CommandListAppendUsedPacketSignalEvent = Test<CommandListEventUsedPacketSignalFixture>;

HWTEST_F(CommandListAppendSignalEvent, WhenAppendingSignalEventWithoutScopeThenMiStoreImmIsGenerated) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    auto result = commandList->appendSignalEvent(event->toHandle(), false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto baseAddr = event->getCompletionFieldGpuAddress(device);
    auto itor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itor, cmdList.end());
    auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itor);
    EXPECT_EQ(baseAddr, cmd->getAddress());
}

HWTEST_F(CommandListAppendSignalEvent, givenCmdlistWhenAppendingSignalEventThenEventPoolGraphicsAllocationIsAddedToResidencyContainer) {
    auto result = commandList->appendSignalEvent(event->toHandle(), false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &residencyContainer = commandList->getCmdContainer().getResidencyContainer();
    auto eventPoolAlloc = &eventPool->getAllocation();
    for (auto alloc : eventPoolAlloc->getGraphicsAllocations()) {
        auto itor =
            std::find(std::begin(residencyContainer), std::end(residencyContainer), alloc);
        EXPECT_NE(itor, std::end(residencyContainer));
    }
}

HWTEST_F(CommandListAppendSignalEvent, givenEventWithScopeFlagDeviceWhenAppendingSignalEventThenPipeControlHasNoDcFlush) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPoolHostVisible = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto eventHostVisible = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPoolHostVisible.get(), &eventDesc, device));

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    result = commandList->appendSignalEvent(eventHostVisible->toHandle(), false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()), cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendSignalEvent, givenCommandListWhenAppendWriteGlobalTimestampCalledWithSignalEventThenPipeControlForTimestampAndSignalEncoded, MatchAny) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    auto &commandContainer = commandList->getCmdContainer();

    uint64_t dstAddress = 0x12345678555500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(dstAddress);

    commandContainer.getResidencyContainer().clear();

    commandList->appendWriteGlobalTimestamp(dstptr, event->toHandle(), 0, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto residencyContainer = commandContainer.getResidencyContainer();
    auto timestampAlloc = residencyContainer[0];

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    while (cmd->getPostSyncOperation() != POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP) {
        itorPC++;
        itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
        EXPECT_NE(cmdList.end(), itorPC);
        cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    }
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_FALSE(cmd->getDcFlushEnable());
    EXPECT_EQ(dstAddress, reinterpret_cast<uint64_t>(timestampAlloc->getUnderlyingBuffer()));
    auto timestampAddress = timestampAlloc->getGpuAddress();
    EXPECT_EQ(timestampAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));

    itorPC++;
    itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    while (cmd->getPostSyncOperation() != POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
        itorPC++;
        itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
        EXPECT_NE(cmdList.end(), itorPC);
        cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    }
    EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_FALSE(cmd->getDcFlushEnable());
}

HWTEST2_F(CommandListAppendSignalEvent, givenImmediateCmdListAndAppendingRegularCommandlistWithWaitOnEventsAndSignalEventThenUseSemaphoreAndPipeControl, IsAtLeastXeHpcCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPoolHostVisible = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto eventHostVisible = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPoolHostVisible.get(), &eventDesc, device));

    auto waitEventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto waitEvent = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(waitEventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);

    ze_event_handle_t hSignalEventHandle = eventHostVisible->toHandle();
    ze_event_handle_t hWaitEventHandle = waitEvent->toHandle();
    std::unique_ptr<L0::CommandList> commandListRegular(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandListRegular->close();
    auto commandListHandle = commandListRegular->toHandle();
    auto usedSpaceBefore = immCommandList->getCmdContainer().getCommandStream()->getUsed();
    result = immCommandList->appendCommandLists(1u, &commandListHandle, hSignalEventHandle, 1u, &hWaitEventHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      immCommandList->getCmdContainer().getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itorSemaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorSemaphore);

    auto itorBBStart = find<MI_BATCH_BUFFER_START *>(itorSemaphore, cmdList.end());
    ASSERT_NE(cmdList.end(), itorBBStart);

    auto itorPC = findAll<PIPE_CONTROL *>(itorBBStart, cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_NE(cmd->getImmediateData(), Event::STATE_CLEARED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()), cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendSignalEvent, givenImmediateCmdListWithComputeQueueAndAppendingRegularCommandlistThenCsrMakeNonTesidentSkippedFromCmdQueue, IsAtLeastXeHpcCore) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_result_t returnValue;

    using cmdListImmediateHwType = typename L0::CommandListCoreFamilyImmediate<static_cast<GFXCORE_FAMILY>(NEO::HwMapper<productFamily>::gfxFamily)>;

    std::unique_ptr<cmdListImmediateHwType> commandList0(static_cast<cmdListImmediateHwType *>(CommandList::createImmediate(productFamily,
                                                                                                                            device,
                                                                                                                            &desc,
                                                                                                                            false,
                                                                                                                            NEO::EngineGroupType::compute,
                                                                                                                            returnValue)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList0);

    auto &commandStreamReceiver = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heaplessStateInit = commandStreamReceiver.heaplessStateInitialized;

    std::unique_ptr<L0::CommandList> commandListRegular(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandListRegular->close();
    auto commandListHandle = commandListRegular->toHandle();

    ze_result_t result = ZE_RESULT_SUCCESS;
    result = commandList0->appendCommandLists(1u, &commandListHandle, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(heaplessStateInit ? 2u : 1u, commandStreamReceiver.makeSurfacePackNonResidentCalled);
}

HWTEST2_F(CommandListAppendSignalEvent, givenCopyOnlyImmediateCmdListAndAppendingRegularCommandlistWithWaitOnEventsAndSignalEventThenUseSemaphoreAndFlushDw, IsAtLeastXeHpcCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPoolHostVisible = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto eventHostVisible = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPoolHostVisible.get(), &eventDesc, device));

    auto waitEventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto waitEvent = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(waitEventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, immCommandList);

    ze_event_handle_t hSignalEventHandle = eventHostVisible->toHandle();
    ze_event_handle_t hWaitEventHandle = waitEvent->toHandle();
    std::unique_ptr<L0::CommandList> commandListRegular(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    commandListRegular->close();
    auto commandListHandle = commandListRegular->toHandle();
    auto usedSpaceBefore = immCommandList->getCmdContainer().getCommandStream()->getUsed();
    result = immCommandList->appendCommandLists(1u, &commandListHandle, hSignalEventHandle, 1u, &hWaitEventHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      immCommandList->getCmdContainer().getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itorSemaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorSemaphore);

    auto itorBBStart = find<MI_BATCH_BUFFER_START *>(itorSemaphore, cmdList.end());
    ASSERT_NE(cmdList.end(), itorBBStart);

    uint32_t expectedMiFlushCount = 1;
    NEO::EncodeDummyBlitWaArgs waArgs{false, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
    if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs) > 0) {
        expectedMiFlushCount = 2;
    }
    // Add pair of MIFlush for task count update
    expectedMiFlushCount += 2;
    auto itorMiFlush = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(expectedMiFlushCount, static_cast<uint32_t>(itorMiFlush.size()));
}

HWTEST2_F(CommandListAppendSignalEvent, givenImmediateCmdListWithCopyQueueAndAppendingRegularCommandlistThenCsrMakeNonTesidentSkippedFromCmdQueue, IsAtLeastXeHpcCore) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_result_t returnValue;

    using cmdListImmediateHwType = typename L0::CommandListCoreFamilyImmediate<static_cast<GFXCORE_FAMILY>(NEO::HwMapper<productFamily>::gfxFamily)>;

    std::unique_ptr<cmdListImmediateHwType> commandList0(static_cast<cmdListImmediateHwType *>(CommandList::createImmediate(productFamily,
                                                                                                                            device,
                                                                                                                            &desc,
                                                                                                                            false,
                                                                                                                            NEO::EngineGroupType::copy,
                                                                                                                            returnValue)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList0);

    auto &commandStreamReceiver = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto heaplessStateInit = commandStreamReceiver.heaplessStateInitialized;

    std::unique_ptr<L0::CommandList> commandListRegular(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    commandListRegular->close();
    auto commandListHandle = commandListRegular->toHandle();

    ze_result_t result = ZE_RESULT_SUCCESS;
    result = commandList0->appendCommandLists(1u, &commandListHandle, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(heaplessStateInit ? 2u : 1u, commandStreamReceiver.makeSurfacePackNonResidentCalled);
}

HWTEST2_F(CommandListAppendSignalEvent, givenTimestampEventUsedInSignalThenPipeControlAppendedCorrectly, MatchAny) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    auto &commandContainer = commandList->getCmdContainer();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    commandList->appendSignalEvent(event->toHandle(), false);
    auto contextOffset = event->getContextEndOffset();
    auto baseAddr = event->getGpuAddress(device);
    auto gpuAddress = ptrOffset(baseAddr, contextOffset);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(cmd->getImmediateData(), Event::STATE_SIGNALED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_FALSE(cmd->getDcFlushEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent,
          givenMultiTileCommandListWhenAppendingScopeEventSignalThenExpectPartitionedPipeControl, IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    size_t useSize = cmdStream->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    cmdStream->getSpace(useSize);

    constexpr uint32_t packets = 2u;

    event->setEventTimestampFlag(false);
    event->setUsingContextEndOffset(true);
    event->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    commandList->partitionCount = packets;
    ze_result_t returnValue = commandList->appendSignalEvent(event->toHandle(), false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(packets, event->getPacketsInUse());

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    size_t expectedSize = NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(device->getNEODevice()->getRootDeviceEnvironment(), false);
    size_t usedSize = cmdStream->getUsed();
    EXPECT_EQ(expectedSize, usedSize);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedSize));

    auto pipeControlList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControlList.size());
    uint32_t postSyncFound = 0;
    for (auto &it : pipeControlList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()), cmd->getDcFlushEnable());
            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
            postSyncFound++;
            gpuAddress += event->getSinglePacketSize();
        }
    }
    EXPECT_EQ(1u, postSyncFound);
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent, givenMultiTileAndDynamicPostSyncLayoutWhenAppendingSignalingTimestampEventThenExpectOffsetRegisters, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    event->setEventTimestampFlag(true);

    commandList->partitionCount = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendSignalEvent(event->toHandle(), false));

    size_t expectedSize = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(device->getNEODevice()->getRootDeviceEnvironment(), false);

    auto unifiedPostSyncLayout = device->getL0GfxCoreHelper().hasUnifiedPostSyncAllocationLayout();

    if (!unifiedPostSyncLayout) {
        expectedSize += (2 * sizeof(MI_LOAD_REGISTER_IMM));
    }

    size_t usedSize = cmdStream->getUsed() - offset;
    EXPECT_EQ(expectedSize, usedSize);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), usedSize));

        auto lriItor = cmdList.begin();
        auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*lriItor);

        std::vector<GenCmdList::iterator> pipeControlList;

        if (unifiedPostSyncLayout) {
            EXPECT_EQ(nullptr, lriCmd);
            pipeControlList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

        } else {
            ASSERT_NE(nullptr, lriCmd);

            EXPECT_EQ(NEO::PartitionRegisters<FamilyType>::addressOffsetCCSOffset, lriCmd->getRegisterOffset());
            EXPECT_EQ(NEO::ImplicitScalingDispatch<FamilyType>::getTimeStampPostSyncOffset(), lriCmd->getDataDword());

            pipeControlList = findAll<PIPE_CONTROL *>(++lriItor, cmdList.end());
        }

        ASSERT_NE(0u, pipeControlList.size());

        auto endLriItor = cmdList.rbegin();

        lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*endLriItor);

        if (unifiedPostSyncLayout) {
            EXPECT_EQ(nullptr, lriCmd);
        } else {
            ASSERT_NE(nullptr, lriCmd);

            EXPECT_EQ(NEO::PartitionRegisters<FamilyType>::addressOffsetCCSOffset, lriCmd->getRegisterOffset());
            EXPECT_EQ(NEO::ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset(), lriCmd->getDataDword());
        }
    }

    event->setEventTimestampFlag(false);

    offset = cmdStream->getUsed();

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendSignalEvent(event->toHandle(), false));

    expectedSize = sizeof(MI_STORE_DATA_IMM);
    usedSize = cmdStream->getUsed() - offset;
    EXPECT_EQ(expectedSize, usedSize);

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), usedSize));

        auto lriList = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, lriList.size());

        auto sdiList = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(1u, sdiList.size());
    }
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent,
          givenMultiTileCommandListWhenAppendingNonScopeEventSignalThenExpectPartitionedStoreDataImm, IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    size_t useSize = cmdStream->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    cmdStream->getSpace(useSize);

    constexpr uint32_t packets = 2u;

    event->setEventTimestampFlag(false);
    event->setUsingContextEndOffset(true);
    event->signalScope = 0;

    commandList->partitionCount = packets;
    ze_result_t returnValue = commandList->appendSignalEvent(event->toHandle(), false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(packets, event->getPacketsInUse());

    auto gpuAddress = event->getGpuAddress(device) + event->getContextEndOffset();

    size_t expectedSize = NEO::EncodeStoreMemory<GfxFamily>::getStoreDataImmSize();
    size_t usedSize = cmdStream->getUsed();
    EXPECT_EQ(expectedSize, usedSize);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedSize));

    auto storeDataImmList = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, storeDataImmList.size());
    uint32_t postSyncFound = 0;
    for (auto &it : storeDataImmList) {
        auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*it);
        EXPECT_EQ(gpuAddress, cmd->getAddress());
        EXPECT_FALSE(cmd->getStoreQword());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
        EXPECT_EQ(0u, cmd->getDataDword1());
        EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, cmd->getDwordLength());
        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
        postSyncFound++;
        gpuAddress += event->getSinglePacketSize();
    }
    EXPECT_EQ(1u, postSyncFound);
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent,
          givenMultiTileCommandListWhenAppendingScopeEventSignalAfterWalkerThenExpectPartitionedPipeControl, IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    size_t useSize = cmdStream->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    cmdStream->getSpace(useSize);

    constexpr uint32_t packets = 2u;

    event->setEventTimestampFlag(false);
    event->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    commandList->partitionCount = packets;
    commandList->appendSignalEventPostWalker(event.get(), nullptr, nullptr, false, false, false);
    EXPECT_EQ(packets, event->getPacketsInUse());

    auto gpuAddress = event->getCompletionFieldGpuAddress(device);

    size_t expectedSize = NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(device->getNEODevice()->getRootDeviceEnvironment(), false);
    size_t usedSize = cmdStream->getUsed();
    EXPECT_EQ(expectedSize, usedSize);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedSize));

    auto pipeControlList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControlList.size());
    uint32_t postSyncFound = 0;
    for (auto &it : pipeControlList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()), cmd->getDcFlushEnable());
            EXPECT_EQ(device->getNEODevice()->isAnyDirectSubmissionEnabled(), cmd->getConstantCacheInvalidationEnable());
            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
            postSyncFound++;
            gpuAddress += event->getSinglePacketSize();
        }
    }
    EXPECT_EQ(1u, postSyncFound);
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent,
          givenMultiTileImmediateCommandListWhenAppendingScopeEventSignalAfterWalkerThenExpectPartitionedPipeControl, IsAtLeastXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    ze_command_queue_desc_t desc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);

    auto commandList = std::make_unique<::L0::ult::MockCommandListImmediateHw<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    commandList->cmdQImmediate = queue.get();
    commandList->cmdListType = CommandList::CommandListType::typeImmediate;
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    size_t useSize = cmdStream->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    cmdStream->getSpace(useSize);

    constexpr uint32_t packets = 2u;

    event->setEventTimestampFlag(false);
    event->signalScope = ZE_EVENT_SCOPE_FLAG_HOST;

    commandList->partitionCount = packets;
    commandList->checkAvailableSpace(0, false, commonImmediateCommandSize);
    commandList->appendSignalEventPostWalker(event.get(), nullptr, nullptr, false, false, false);
    EXPECT_EQ(packets, event->getPacketsInUse());

    auto gpuAddress = event->getCompletionFieldGpuAddress(device);

    size_t expectedSize = NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(device->getNEODevice()->getRootDeviceEnvironment(), false);
    size_t usedSize = cmdStream->getUsed();
    EXPECT_EQ(expectedSize, usedSize);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedSize));

    auto pipeControlList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControlList.size());
    uint32_t postSyncFound = 0;
    for (auto &it : pipeControlList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()), cmd->getDcFlushEnable());
            auto &productHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
            EXPECT_EQ(productHelper.isDirectSubmissionConstantCacheInvalidationNeeded(device->getHwInfo()) && commandList->getCsr(false)->isDirectSubmissionEnabled(), cmd->getConstantCacheInvalidationEnable());
            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
            postSyncFound++;
            gpuAddress += event->getSinglePacketSize();
        }
    }
    EXPECT_EQ(1u, postSyncFound);
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent,
          givenMultiTileCommandListWhenAppendWriteGlobalTimestampCalledWithSignalEventThenWorkPartitionedRegistersAreUsed, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    auto &commandContainer = commandList->getCmdContainer();

    auto memoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;

    uint64_t dstAddress = 0x123456785500;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(dstAddress);
    commandContainer.getResidencyContainer().clear();

    constexpr uint32_t packets = 2u;

    event->setEventTimestampFlag(true);
    commandList->partitionCount = packets;

    commandList->appendWriteGlobalTimestamp(dstptr, event->toHandle(), 0, nullptr);
    EXPECT_EQ(packets, event->getPacketsInUse());

    auto residencyContainer = commandContainer.getResidencyContainer();
    auto timestampAlloc = residencyContainer[1];
    EXPECT_EQ(dstAddress, reinterpret_cast<uint64_t>(timestampAlloc->getUnderlyingBuffer()));
    auto timestampAddress = timestampAlloc->getGpuAddress();

    auto eventGpuAddress = event->getGpuAddress(device);
    uint64_t contextStartAddress = eventGpuAddress + event->getContextStartOffset();
    uint64_t globalStartAddress = eventGpuAddress + event->getGlobalStartOffset();
    uint64_t contextEndAddress = eventGpuAddress + event->getContextEndOffset();
    uint64_t globalEndAddress = eventGpuAddress + event->getGlobalEndOffset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    auto cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    while (cmd->getPostSyncOperation() != POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP) {
        itorPC++;
        itorPC = find<PIPE_CONTROL *>(itorPC, cmdList.end());
        EXPECT_NE(cmdList.end(), itorPC);
        cmd = genCmdCast<PIPE_CONTROL *>(*itorPC);
    }
    EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
    EXPECT_FALSE(cmd->getDcFlushEnable());
    EXPECT_EQ(timestampAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));

    auto startCmdList = cmdList.begin();
    validateTimestampRegisters<FamilyType>(cmdList,
                                           startCmdList,
                                           RegisterOffsets::globalTimestampLdw, globalStartAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextStartAddress,
                                           true,
                                           true);

    if (UnitTestHelper<FamilyType>::timestampRegisterHighAddress()) {
        uint64_t globalStartAddressHigh = globalStartAddress + sizeof(uint32_t);
        uint64_t contextStartAddressHigh = contextStartAddress + sizeof(uint32_t);
        validateTimestampRegisters<FamilyType>(cmdList,
                                               startCmdList,
                                               RegisterOffsets::globalTimestampUn, globalStartAddressHigh,
                                               RegisterOffsets::gpThreadTimeRegAddressOffsetHigh, contextStartAddressHigh,
                                               true,
                                               false);
    }

    validateTimestampRegisters<FamilyType>(cmdList,
                                           startCmdList,
                                           RegisterOffsets::globalTimestampLdw, globalEndAddress,
                                           RegisterOffsets::gpThreadTimeRegAddressOffsetLow, contextEndAddress,
                                           true,
                                           true);

    if (UnitTestHelper<FamilyType>::timestampRegisterHighAddress()) {
        uint64_t globalEndAddressHigh = globalEndAddress + sizeof(uint32_t);
        uint64_t contextEndAddressHigh = contextEndAddress + sizeof(uint32_t);
        validateTimestampRegisters<FamilyType>(cmdList,
                                               startCmdList,
                                               RegisterOffsets::globalTimestampUn, globalEndAddressHigh,
                                               RegisterOffsets::gpThreadTimeRegAddressOffsetHigh, contextEndAddressHigh,
                                               true,
                                               false);
    }
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent,
          givenCopyCommandListWhenAppendingTimestampEventPacketThenExpectCorrectNumberOfMiFlushCommands, IsAtLeastXeHpCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    event->setEventTimestampFlag(true);

    commandList->appendEventForProfilingCopyCommand(event.get(), false);
    size_t usedAfterSize = cmdStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedAfterSize));

    uint32_t expectedMiFlushCount = 1;
    NEO::EncodeDummyBlitWaArgs waArgs{false, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
    if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs) > 0) {
        expectedMiFlushCount = 2;
    }

    auto itorMiFlush = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedMiFlushCount, static_cast<uint32_t>(itorMiFlush.size()));

    for (uint32_t i = 0; i < expectedMiFlushCount; i++) {
        if ((expectedMiFlushCount == 2) && (i % 2 == 0)) {
            continue;
        }
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*itorMiFlush[i]);
        EXPECT_EQ(0u, cmd->getDestinationAddress());
        EXPECT_EQ(0u, cmd->getImmediateData());
        EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE, cmd->getPostSyncOperation());
    }
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent,
          givenCopyCommandListWhenAppendingImmediateEventPacketPostWalkerThenExpectCorrectNumberOfMiFlushCommands, IsAtLeastXeHpCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    event->setEventTimestampFlag(false);

    commandList->appendSignalEventPostWalker(event.get(), nullptr, nullptr, false, false, true);
    size_t usedAfterSize = cmdStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedAfterSize));

    uint32_t expectedMiFlushCount = 1;
    NEO::EncodeDummyBlitWaArgs waArgs{false, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
    if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs) > 0) {
        expectedMiFlushCount = 2;
    }

    auto itorMiFlush = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedMiFlushCount, static_cast<uint32_t>(itorMiFlush.size()));

    auto gpuAddress = event->getCompletionFieldGpuAddress(device);

    for (uint32_t i = 0; i < expectedMiFlushCount; i++) {
        if ((expectedMiFlushCount == 2) && (i % 2 == 0)) {
            continue;
        }
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*itorMiFlush[i]);
        EXPECT_EQ(gpuAddress, cmd->getDestinationAddress());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
        EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, cmd->getPostSyncOperation());
    }
}

HWTEST2_F(CommandListAppendUsedPacketSignalEvent,
          givenCopyCommandListWhenAppendingSignalImmediateEventPacketThenExpectCorrectNumberOfMiFlushCommands, IsAtLeastXeHpCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdStream = commandList->getCmdContainer().getCommandStream();

    event->setEventTimestampFlag(false);

    commandList->appendSignalEvent(event->toHandle(), false);
    size_t usedAfterSize = cmdStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        cmdStream->getCpuBase(),
        usedAfterSize));

    uint32_t expectedMiFlushCount = 1;
    NEO::EncodeDummyBlitWaArgs waArgs{false, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
    if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs) > 0) {
        expectedMiFlushCount = 2;
    }

    auto itorMiFlush = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedMiFlushCount, static_cast<uint32_t>(itorMiFlush.size()));

    auto gpuAddress = event->getCompletionFieldGpuAddress(device);

    for (uint32_t i = 0; i < expectedMiFlushCount; i++) {
        if ((expectedMiFlushCount == 2) && (i % 2 == 0)) {
            continue;
        }
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*itorMiFlush[i]);
        EXPECT_EQ(gpuAddress, cmd->getDestinationAddress());
        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
        EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, cmd->getPostSyncOperation());
    }
}

} // namespace ult
} // namespace L0
