/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeCommandListGetNextCommandIdExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_command_id_exp_desc_t *desc,
    uint64_t *pCommandId) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zeCommandListUpdateMutableCommandsExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_commands_exp_desc_t *desc) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zeCommandListUpdateMutableCommandSignalEventExp(
    ze_command_list_handle_t hCommandList,
    uint64_t commandId,
    ze_event_handle_t hSignalEvent) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zeCommandListUpdateMutableCommandWaitEventsExp(
    ze_command_list_handle_t hCommandList,
    uint64_t commandId,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zeCommandListGetNextCommandIdWithKernelsExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_command_id_exp_desc_t *desc,
    uint32_t numKernels,
    ze_kernel_handle_t *phKernels,
    uint64_t *pCommandId) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zeCommandListUpdateMutableCommandKernelsExp(
    ze_command_list_handle_t hCommandList,
    uint32_t numKernels,
    uint64_t *pCommandId,
    ze_kernel_handle_t *phKernels) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListGetNextCommandIdExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_command_id_exp_desc_t *desc,
    uint64_t *pCommandId) {
    return L0::zeCommandListGetNextCommandIdExp(hCommandList, desc, pCommandId);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListUpdateMutableCommandsExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_commands_exp_desc_t *desc) {
    return L0::zeCommandListUpdateMutableCommandsExp(hCommandList, desc);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListUpdateMutableCommandSignalEventExp(
    ze_command_list_handle_t hCommandList,
    uint64_t commandId,
    ze_event_handle_t hSignalEvent) {
    return L0::zeCommandListUpdateMutableCommandSignalEventExp(hCommandList, commandId, hSignalEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListUpdateMutableCommandWaitEventsExp(
    ze_command_list_handle_t hCommandList,
    uint64_t commandId,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListUpdateMutableCommandWaitEventsExp(hCommandList, commandId, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListGetNextCommandIdWithKernelsExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_command_id_exp_desc_t *desc,
    uint32_t numKernels,
    ze_kernel_handle_t *phKernels,
    uint64_t *pCommandId) {
    return L0::zeCommandListGetNextCommandIdWithKernelsExp(hCommandList, desc, numKernels, phKernels, pCommandId);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListUpdateMutableCommandKernelsExp(
    ze_command_list_handle_t hCommandList,
    uint32_t numKernels,
    uint64_t *pCommandId,
    ze_kernel_handle_t *phKernels) {
    return L0::zeCommandListUpdateMutableCommandKernelsExp(hCommandList, numKernels, pCommandId, phKernels);
}

} // extern "C"
