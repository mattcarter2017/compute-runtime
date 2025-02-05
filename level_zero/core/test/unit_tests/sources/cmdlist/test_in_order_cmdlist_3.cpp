/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {
namespace ult {
struct InOrderIpcTests : public InOrderCmdListFixture {
    void enableEventSharing(FixtureMockEvent &event) {
        event.isSharableCounterBased = true;

        if (event.inOrderExecInfo.get()) {
            if (event.inOrderExecInfo->getDeviceCounterAllocation()) {
                static_cast<MemoryAllocation *>(event.inOrderExecInfo->getDeviceCounterAllocation())->internalHandle = 1;
            }
            if (event.inOrderExecInfo->getHostCounterAllocation()) {
                static_cast<MemoryAllocation *>(event.inOrderExecInfo->getHostCounterAllocation())->internalHandle = 2;
            }
        }
    }
};

HWTEST2_F(InOrderIpcTests, givenInvalidCbEventWhenOpenIpcCalledThenReturnError, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto nonTsEvent = createEvents<FamilyType>(1, false);
    auto tsEvent = createEvents<FamilyType>(1, true);

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    enableEventSharing(*events[0]);
    enableEventSharing(*events[1]);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams, false);

    enableEventSharing(*events[0]);
    enableEventSharing(*events[1]);

    auto mockMemoryManager = static_cast<NEO::MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_EQ(0u, mockMemoryManager->registerIpcExportedAllocationCalled);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    EXPECT_EQ(events[0]->inOrderExecInfo->isHostStorageDuplicated() ? 2u : 1u, mockMemoryManager->registerIpcExportedAllocationCalled);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(events[1]->toHandle(), &zexIpcData));

    events[0]->makeCounterBasedImplicitlyDisabled(nonTsEvent->getAllocation());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));
}

HWTEST2_F(InOrderIpcTests, givenCbEventWhenCreatingFromApiThenOpenIpcHandle, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC}; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901
    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE;
    ze_event_handle_t ipcEvent = nullptr;
    ze_event_handle_t nonIpcEvent = nullptr;
    ze_event_handle_t timestampIpcEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &nonIpcEvent));

    counterBasedDesc.flags |= ZEX_COUNTER_BASED_EVENT_FLAG_IPC;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &ipcEvent));

    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_IPC | ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &timestampIpcEvent));

    counterBasedDesc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_IPC | ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_MAPPED_TIMESTAMP;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventCreate2(context, device, &counterBasedDesc, &timestampIpcEvent));

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nonIpcEvent, 0, nullptr, launchParams, false);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, ipcEvent, 0, nullptr, launchParams, false);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zexCounterBasedEventGetIpcHandle(nonIpcEvent, &zexIpcData));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(ipcEvent, &zexIpcData));

    zeEventDestroy(ipcEvent);
    zeEventDestroy(nonIpcEvent);
}

HWTEST2_F(InOrderIpcTests, givenIncorrectInternalHandleWhenGetIsCalledThenReturnError, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);

    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getDeviceCounterAllocation());
    deviceAlloc->internalHandle = std::numeric_limits<uint64_t>::max();

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    if (events[0]->inOrderExecInfo->isHostStorageDuplicated()) {
        deviceAlloc->internalHandle = 1;
        static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getHostCounterAllocation())->internalHandle = std::numeric_limits<uint64_t>::max();
        EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));
    }
}

HWTEST2_F(InOrderIpcTests, givenCounterOffsetWhenOpenIsCalledThenPassCorrectData, MatchAny) {
    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);

    enableEventSharing(*events[0]);

    events[0]->inOrderAllocationOffset = 0x100;

    static_cast<WhiteboxInOrderExecInfo *>(events[0]->inOrderExecInfo.get())->numDevicePartitionsToWait = 2;
    static_cast<WhiteboxInOrderExecInfo *>(events[0]->inOrderExecInfo.get())->numHostPartitionsToWait = 3;

    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getDeviceCounterAllocation());
    auto hostAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getHostCounterAllocation());

    zex_ipc_counter_based_event_handle_t zexIpcData = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    IpcCounterBasedEventData &ipcData = *reinterpret_cast<IpcCounterBasedEventData *>(zexIpcData.data);

    EXPECT_TRUE(deviceAlloc->internalHandle == ipcData.deviceHandle);
    EXPECT_TRUE(events[0]->inOrderExecInfo->isHostStorageDuplicated() ? hostAlloc->internalHandle : 0u == ipcData.hostHandle);
    EXPECT_TRUE(events[0]->counterBasedFlags == ipcData.counterBasedFlags);
    EXPECT_TRUE(device->getRootDeviceIndex() == ipcData.rootDeviceIndex);
    EXPECT_TRUE(events[0]->inOrderExecSignalValue == ipcData.counterValue);
    EXPECT_TRUE(events[0]->signalScope == ipcData.signalScopeFlags);
    EXPECT_TRUE(events[0]->waitScope == ipcData.waitScopeFlags);

    auto expectedOffset = static_cast<uint32_t>(events[0]->inOrderExecInfo->getBaseDeviceAddress() - events[0]->inOrderExecInfo->getDeviceCounterAllocation()->getGpuAddress());
    EXPECT_NE(0u, expectedOffset);
    expectedOffset += events[0]->inOrderAllocationOffset;

    EXPECT_TRUE(expectedOffset == ipcData.counterOffset);
    EXPECT_TRUE(events[0]->inOrderExecInfo->getNumDevicePartitionsToWait() == ipcData.devicePartitions);
    EXPECT_TRUE(events[0]->inOrderExecInfo->isHostStorageDuplicated() ? events[0]->inOrderExecInfo->getNumHostPartitionsToWait() : events[0]->inOrderExecInfo->getNumDevicePartitionsToWait() == ipcData.hostPartitions);
}

HWTEST2_F(InOrderIpcTests, givenIpcHandleWhenCreatingNewEventThenSetCorrectData, MatchAny) {
    auto immCmdList1 = createImmCmdList<gfxCoreFamily>();
    auto immCmdList2 = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList2->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);
    events[0]->inOrderAllocationOffset = 0x100;
    auto event0InOrderInfo = static_cast<WhiteboxInOrderExecInfo *>(events[0]->inOrderExecInfo.get());
    event0InOrderInfo->numDevicePartitionsToWait = 2;
    event0InOrderInfo->numHostPartitionsToWait = 3;

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));

    EXPECT_NE(nullptr, newEvent);

    auto newEventMock = static_cast<FixtureMockEvent *>(Event::fromHandle(newEvent));
    auto inOrderInfo = newEventMock->inOrderExecInfo.get();

    EXPECT_FALSE(inOrderInfo->isRegularCmdList());
    EXPECT_EQ(inOrderInfo->getDeviceCounterAllocation()->getGpuAddress(), inOrderInfo->getBaseDeviceAddress());
    EXPECT_EQ(event0InOrderInfo->getNumDevicePartitionsToWait(), inOrderInfo->getNumDevicePartitionsToWait());
    EXPECT_EQ(event0InOrderInfo->isHostStorageDuplicated() ? event0InOrderInfo->getNumHostPartitionsToWait() : event0InOrderInfo->getNumDevicePartitionsToWait(), inOrderInfo->getNumHostPartitionsToWait());

    EXPECT_NE(nullptr, inOrderInfo->getExternalDeviceAllocation());
    EXPECT_NE(nullptr, inOrderInfo->getExternalHostAllocation());

    if (event0InOrderInfo->isHostStorageDuplicated()) {
        EXPECT_NE(inOrderInfo->getExternalDeviceAllocation(), inOrderInfo->getExternalHostAllocation());
        EXPECT_TRUE(inOrderInfo->isHostStorageDuplicated());
    } else {
        EXPECT_EQ(inOrderInfo->getExternalDeviceAllocation(), inOrderInfo->getExternalHostAllocation());
        EXPECT_FALSE(inOrderInfo->isHostStorageDuplicated());
    }

    EXPECT_TRUE(newEventMock->isFromIpcPool);
    EXPECT_EQ(newEventMock->signalScope, events[0]->signalScope);
    EXPECT_EQ(newEventMock->waitScope, events[0]->waitScope);
    EXPECT_EQ(newEventMock->inOrderExecSignalValue, events[0]->inOrderExecSignalValue);

    auto expectedOffset = static_cast<uint32_t>(event0InOrderInfo->getBaseDeviceAddress() - event0InOrderInfo->getDeviceCounterAllocation()->getGpuAddress()) + events[0]->inOrderAllocationOffset;
    EXPECT_EQ(expectedOffset, newEventMock->inOrderAllocationOffset);

    zexCounterBasedEventCloseIpcHandle(newEvent);
}

HWTEST2_F(InOrderIpcTests, givenInvalidInternalHandleWhenOpenCalledThenReturnError, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);

    auto deviceAlloc = static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getDeviceCounterAllocation());
    deviceAlloc->internalHandle = NEO::MockMemoryManager::invalidSharedHandle;

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));

    if (events[0]->inOrderExecInfo->isHostStorageDuplicated()) {
        deviceAlloc->internalHandle = 1;
        static_cast<MemoryAllocation *>(events[0]->inOrderExecInfo->getHostCounterAllocation())->internalHandle = NEO::MockMemoryManager::invalidSharedHandle;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

        EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));
    }
}

HWTEST2_F(InOrderIpcTests, givenTbxModeWhenOpenIsCalledThenSetAllocationParams, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    ultCsr->commandStreamReceiverType = CommandStreamReceiverType::tbx;

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));

    auto newEventMock = static_cast<FixtureMockEvent *>(Event::fromHandle(newEvent));

    EXPECT_TRUE(newEventMock->inOrderExecInfo->getExternalDeviceAllocation()->getAubInfo().writeMemoryOnly);

    if (newEventMock->inOrderExecInfo->isHostStorageDuplicated()) {
        EXPECT_TRUE(newEventMock->inOrderExecInfo->getExternalHostAllocation()->getAubInfo().writeMemoryOnly);
    }

    zexCounterBasedEventCloseIpcHandle(newEvent);
}

HWTEST2_F(InOrderIpcTests, givenIpcImportedEventWhenSignalingThenReturnError, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_event_handle_t newEvent = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, &newEvent));

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, newEvent, 0, nullptr, launchParams, false));

    zexCounterBasedEventCloseIpcHandle(newEvent);
}

HWTEST2_F(InOrderIpcTests, givenIncorrectParamsWhenUsingIpcApisThenReturnError, MatchAny) {
    auto immCmdList = createImmCmdList<gfxCoreFamily>();

    auto pool = createEvents<FamilyType>(1, false);

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams, false);
    enableEventSharing(*events[0]);

    zex_ipc_counter_based_event_handle_t zexIpcData = {};

    ze_event_handle_t nullEvent = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(nullEvent, &zexIpcData));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), nullptr));

    events[0]->makeCounterBasedInitiallyDisabled(pool->getAllocation());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventGetIpcHandle(events[0]->toHandle(), &zexIpcData));

    ze_context_handle_t nullContext = nullptr;
    ze_event_handle_t newEvent = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventOpenIpcHandle(nullContext, zexIpcData, &newEvent));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zexCounterBasedEventOpenIpcHandle(context->toHandle(), zexIpcData, nullptr));
}

} // namespace ult
} // namespace L0