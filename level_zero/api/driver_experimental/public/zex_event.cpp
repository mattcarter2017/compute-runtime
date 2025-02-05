/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_event.h"

#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexEventGetDeviceAddress(ze_event_handle_t event, uint64_t *completionValue, uint64_t *address) {
    auto eventObj = Event::fromHandle(toInternalType(event));

    if (!eventObj || !completionValue || !address) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (eventObj->isCounterBased()) {
        if (!eventObj->getInOrderExecInfo()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        *completionValue = eventObj->getInOrderExecSignalValueWithSubmissionCounter();
        *address = eventObj->getInOrderExecInfo()->getBaseDeviceAddress() + eventObj->getInOrderAllocationOffset();
    } else if (eventObj->isEventTimestampFlagSet()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    } else {
        *completionValue = Event::State::STATE_SIGNALED;
        *address = eventObj->getCompletionFieldGpuAddress(eventObj->peekEventPool()->getDevice());
    }

    return ZE_RESULT_SUCCESS;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCounterBasedEventCreate2(ze_context_handle_t hContext, ze_device_handle_t hDevice, const zex_counter_based_event_desc_t *desc, ze_event_handle_t *phEvent) {
    constexpr uint32_t supportedBasedFlags = (ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE);

    auto device = Device::fromHandle(toInternalType(hDevice));

    if (!hDevice || !desc || !phEvent) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    const bool ipcFlag = !!(desc->flags & ZEX_COUNTER_BASED_EVENT_FLAG_IPC);
    const bool timestampFlag = !!(desc->flags & ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP);
    const bool mappedTimestampFlag = !!(desc->flags & ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_MAPPED_TIMESTAMP);

    uint32_t inputCbFlags = desc->flags & supportedBasedFlags;
    if (inputCbFlags == 0) {
        inputCbFlags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE;
    }

    if (ipcFlag && (timestampFlag || mappedTimestampFlag)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto signalScope = desc->signalScope;

    if (NEO::debugManager.flags.MitigateHostVisibleSignal.get()) {
        signalScope &= ~ZE_EVENT_SCOPE_FLAG_HOST;
    }

    EventDescriptor eventDescriptor = {
        nullptr,                           // eventPoolAllocation
        desc->pNext,                       // extensions
        0,                                 // totalEventSize
        EventPacketsCount::maxKernelSplit, // maxKernelCount
        1,                                 // maxPacketsCount
        inputCbFlags,                      // counterBasedFlags
        0,                                 // index
        signalScope,                       // signalScope
        desc->waitScope,                   // waitScope
        timestampFlag,                     // timestampPool
        mappedTimestampFlag,               // kernelMappedTsPoolFlag
        false,                             // importedIpcPool
        ipcFlag,                           // ipcPool
    };

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto l0Event = device->getL0GfxCoreHelper().createStandaloneEvent(eventDescriptor, device, result);

    if (signalScope ^ desc->signalScope) {
        l0Event->setMitigateHostVisibleSignal();
    }

    *phEvent = l0Event;

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCounterBasedEventCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, uint64_t *deviceAddress, uint64_t *hostAddress, uint64_t completionValue, const ze_event_desc_t *desc, ze_event_handle_t *phEvent) {
    constexpr uint32_t counterBasedFlags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_NON_IMMEDIATE;

    if (!desc) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    zex_counter_based_event_external_sync_alloc_properties_t externalSyncAllocProperties = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES}; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901
    externalSyncAllocProperties.completionValue = completionValue;
    externalSyncAllocProperties.deviceAddress = deviceAddress;
    externalSyncAllocProperties.hostAddress = hostAddress;

    zex_counter_based_event_desc_t counterBasedDesc = {ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC}; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901
    counterBasedDesc.flags = counterBasedFlags;
    counterBasedDesc.signalScope = desc->signal;
    counterBasedDesc.waitScope = desc->wait;
    counterBasedDesc.pNext = desc->pNext;

    if (deviceAddress && hostAddress) {
        externalSyncAllocProperties.pNext = desc->pNext;
        counterBasedDesc.pNext = &externalSyncAllocProperties;
    }

    return zexCounterBasedEventCreate2(hContext, hDevice, &counterBasedDesc, phEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zexIntelAllocateNetworkInterrupt(ze_context_handle_t hContext, uint32_t &networkInterruptId) {
    auto context = static_cast<ContextImp *>(L0::Context::fromHandle(toInternalType(hContext)));

    if (!context) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (!context->getDriverHandle()->getMemoryManager()->allocateInterrupt(networkInterruptId, context->rootDeviceIndices[0])) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return ZE_RESULT_SUCCESS;
}

ZE_APIEXPORT ze_result_t ZE_APICALL zexIntelReleaseNetworkInterrupt(ze_context_handle_t hContext, uint32_t networkInterruptId) {
    auto context = static_cast<ContextImp *>(L0::Context::fromHandle(toInternalType(hContext)));

    if (!context || !context->getDriverHandle()->getMemoryManager()->releaseInterrupt(networkInterruptId, context->rootDeviceIndices[0])) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return ZE_RESULT_SUCCESS;
}

ZE_APIEXPORT ze_result_t ZE_APICALL zexCounterBasedEventGetIpcHandle(ze_event_handle_t hEvent, zex_ipc_counter_based_event_handle_t *phIpc) {
    auto event = Event::fromHandle(hEvent);
    if (!event || !phIpc || !event->isCounterBasedExplicitlyEnabled()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto ipcData = reinterpret_cast<IpcCounterBasedEventData *>(phIpc->data);

    return event->getCounterBasedIpcHandle(*ipcData);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zexCounterBasedEventOpenIpcHandle(ze_context_handle_t hContext, zex_ipc_counter_based_event_handle_t hIpc, ze_event_handle_t *phEvent) {
    auto context = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    if (!context || !phEvent) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto ipcData = reinterpret_cast<IpcCounterBasedEventData *>(hIpc.data);

    return context->openCounterBasedIpcHandle(*ipcData, phEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zexCounterBasedEventCloseIpcHandle(ze_event_handle_t hEvent) {
    return Event::fromHandle(hEvent)->destroy();
}

} // namespace L0
