/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/zet_intel_gpu_metric.h"

namespace L0 {

ze_result_t ZE_APICALL zetIntelCommandListAppendMarkerExp(zet_command_list_handle_t hCommandList,
                                                          zet_metric_group_handle_t hMetricGroup,
                                                          uint32_t value) {

    auto metricGroupImp = static_cast<MetricGroupImp *>(L0::MetricGroup::fromHandle(hMetricGroup));
    return metricGroupImp->getMetricSource().appendMarker(hCommandList, hMetricGroup, value);
}

ze_result_t ZE_APICALL zetIntelMetricTracerCreateExp(zet_context_handle_t hContext,
                                                     zet_device_handle_t hDevice,
                                                     uint32_t metricGroupCount,
                                                     zet_metric_group_handle_t *phMetricGroups,
                                                     zet_intel_metric_tracer_exp_desc_t *desc,
                                                     ze_event_handle_t hNotificationEvent,
                                                     zet_intel_metric_tracer_exp_handle_t *phMetricTracer) {
    return L0::metricTracerCreate(hContext, hDevice, metricGroupCount, phMetricGroups, desc, hNotificationEvent, phMetricTracer);
}

ze_result_t ZE_APICALL zetIntelMetricTracerDestroyExp(zet_intel_metric_tracer_exp_handle_t hMetricTracer) {
    return L0::metricTracerDestroy(hMetricTracer);
}

ze_result_t ZE_APICALL zetIntelMetricTracerEnableExp(zet_intel_metric_tracer_exp_handle_t hMetricTracer,
                                                     ze_bool_t synchronous) {
    return L0::metricTracerEnable(hMetricTracer, synchronous);
}

ze_result_t ZE_APICALL zetIntelMetricTracerDisableExp(zet_intel_metric_tracer_exp_handle_t hMetricTracer,
                                                      ze_bool_t synchronous) {
    return L0::metricTracerDisable(hMetricTracer, synchronous);
}

ze_result_t ZE_APICALL zetIntelMetricTracerReadDataExp(zet_intel_metric_tracer_exp_handle_t hMetricTracer,
                                                       size_t *pRawDataSize, uint8_t *pRawData) {
    return L0::metricTracerReadData(hMetricTracer, pRawDataSize, pRawData);
}

ze_result_t ZE_APICALL zetIntelMetricDecoderCreateExp(zet_intel_metric_tracer_exp_handle_t hMetricTracer,
                                                      zet_intel_metric_decoder_exp_handle_t *phMetricDecoder) {
    return L0::metricDecoderCreate(hMetricTracer, phMetricDecoder);
}

ze_result_t ZE_APICALL zetIntelMetricDecoderDestroyExp(zet_intel_metric_decoder_exp_handle_t phMetricDecoder) {
    return L0::metricDecoderDestroy(phMetricDecoder);
}

ze_result_t ZE_APICALL zetIntelMetricDecoderGetDecodableMetricsExp(zet_intel_metric_decoder_exp_handle_t hMetricDecoder,
                                                                   uint32_t *pCount,
                                                                   zet_metric_handle_t *phMetrics) {
    return L0::metricDecoderGetDecodableMetrics(hMetricDecoder, pCount, phMetrics);
}

ze_result_t ZE_APICALL zetIntelMetricTracerDecodeExp(zet_intel_metric_decoder_exp_handle_t phMetricDecoder,
                                                     size_t *pRawDataSize, uint8_t *pRawData, uint32_t metricsCount,
                                                     zet_metric_handle_t *phMetrics, uint32_t *pSetCount, uint32_t *pMetricEntriesCountPerSet,
                                                     uint32_t *pMetricEntriesCount, zet_intel_metric_entry_exp_t *pMetricEntries) {
    return L0::metricTracerDecode(phMetricDecoder, pRawDataSize, pRawData, metricsCount, phMetrics,
                                  pSetCount, pMetricEntriesCountPerSet, pMetricEntriesCount, pMetricEntries);
}

} // namespace L0

extern "C" {

ze_result_t ZE_APICALL
zetIntelCommandListAppendMarkerExp(
    zet_command_list_handle_t hCommandList,
    zet_metric_group_handle_t hMetricGroup,
    uint32_t value) {
    return L0::zetIntelCommandListAppendMarkerExp(hCommandList, hMetricGroup, value);
}

ze_result_t ZE_APICALL zetIntelMetricTracerCreateExp(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    uint32_t metricGroupCount,
    zet_metric_group_handle_t *phMetricGroups,
    zet_intel_metric_tracer_exp_desc_t *desc,
    ze_event_handle_t hNotificationEvent,
    zet_intel_metric_tracer_exp_handle_t *phMetricTracer) {
    return L0::zetIntelMetricTracerCreateExp(hContext, hDevice, metricGroupCount, phMetricGroups, desc, hNotificationEvent, phMetricTracer);
}

ze_result_t ZE_APICALL zetIntelMetricTracerDestroyExp(
    zet_intel_metric_tracer_exp_handle_t hMetricTracer) {
    return L0::zetIntelMetricTracerDestroyExp(hMetricTracer);
}

ze_result_t ZE_APICALL zetIntelMetricTracerEnableExp(
    zet_intel_metric_tracer_exp_handle_t hMetricTracer,
    ze_bool_t synchronous) {
    return L0::zetIntelMetricTracerEnableExp(hMetricTracer, synchronous);
}

ze_result_t ZE_APICALL zetIntelMetricTracerDisableExp(
    zet_intel_metric_tracer_exp_handle_t hMetricTracer,
    ze_bool_t synchronous) {
    return L0::zetIntelMetricTracerDisableExp(hMetricTracer, synchronous);
}

ze_result_t ZE_APICALL zetIntelMetricTracerReadDataExp(
    zet_intel_metric_tracer_exp_handle_t hMetricTracer,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    return L0::zetIntelMetricTracerReadDataExp(hMetricTracer, pRawDataSize, pRawData);
}

ze_result_t ZE_APICALL zetIntelMetricDecoderCreateExp(
    zet_intel_metric_tracer_exp_handle_t hMetricTracer,
    zet_intel_metric_decoder_exp_handle_t *phMetricDecoder) {
    return L0::zetIntelMetricDecoderCreateExp(hMetricTracer, phMetricDecoder);
}

ze_result_t ZE_APICALL zetIntelMetricDecoderDestroyExp(
    zet_intel_metric_decoder_exp_handle_t phMetricDecoder) {
    return L0::zetIntelMetricDecoderDestroyExp(phMetricDecoder);
}

ze_result_t ZE_APICALL zetIntelMetricDecoderGetDecodableMetricsExp(
    zet_intel_metric_decoder_exp_handle_t hMetricDecoder,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    return L0::zetIntelMetricDecoderGetDecodableMetricsExp(hMetricDecoder, pCount, phMetrics);
}

ze_result_t ZE_APICALL zetIntelMetricTracerDecodeExp(
    zet_intel_metric_decoder_exp_handle_t phMetricDecoder,
    size_t *pRawDataSize,
    uint8_t *pRawData,
    uint32_t metricsCount,
    zet_metric_handle_t *phMetrics,
    uint32_t *pSetCount,
    uint32_t *pMetricEntriesCountPerSet,
    uint32_t *pMetricEntriesCount,
    zet_intel_metric_entry_exp_t *pMetricEntries) {
    return L0::zetIntelMetricTracerDecodeExp(phMetricDecoder, pRawDataSize, pRawData, metricsCount, phMetrics, pSetCount,
                                             pMetricEntriesCountPerSet, pMetricEntriesCount, pMetricEntries);
}
}
