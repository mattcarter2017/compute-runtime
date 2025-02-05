/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {
struct MetricEnumeration;
struct MetricsLibrary;
struct MetricsStreamer;

class OaMetricSourceImp : public MetricSource {

  public:
    OaMetricSourceImp(const MetricDeviceContext &metricDeviceContext);
    ~OaMetricSourceImp() override;
    void enable() override;
    bool isAvailable() override;
    ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) override;
    ze_result_t appendMetricMemoryBarrier(CommandList &commandList) override;
    bool loadDependencies();
    bool isInitialized();
    void setInitializationState(const ze_result_t state);
    Device &getDevice();
    MetricsLibrary &getMetricsLibrary();
    MetricEnumeration &getMetricEnumeration();
    MetricStreamer *getMetricStreamer();
    void setMetricStreamer(MetricStreamer *pMetricStreamer);
    std::unique_ptr<MetricsLibrary> &getMetricsLibraryObject() { return metricsLibrary; }
    std::unique_ptr<MetricEnumeration> &getMetricEnumerationObject() { return metricEnumeration; }

    ze_result_t activateMetricGroupsAlreadyDeferred() override;
    ze_result_t activateMetricGroupsPreferDeferred(const uint32_t count,
                                                   zet_metric_group_handle_t *phMetricGroups) override;
    ze_result_t metricProgrammableGet(uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) override;
    ze_result_t getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                          uint32_t *pConcurrentGroupCount, uint32_t *pCountPerConcurrentGroup) override;
    ze_result_t handleMetricGroupExtendedProperties(zet_metric_group_handle_t hMetricGroup, void *pNext) override;
    bool isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const;
    bool isMetricGroupActivatedInHw() const;
    void setUseCompute(const bool useCompute);
    bool isComputeUsed() const;
    uint32_t getSubDeviceIndex();
    bool isImplicitScalingCapable() const;
    const MetricDeviceContext &getMetricDeviceContext() const { return metricDeviceContext; }
    static std::unique_ptr<OaMetricSourceImp> create(const MetricDeviceContext &metricDeviceContext);
    void setMetricOsInterface(std::unique_ptr<MetricOAOsInterface> &metricOAOsInterface);
    MetricOAOsInterface *getMetricOsInterface() { return metricOAOsInterface.get(); }
    ze_result_t metricGroupCreateFromMetric(const char *pName, const char *pDescription,
                                            zet_metric_group_sampling_type_flags_t samplingType, zet_metric_handle_t hMetric,
                                            zet_metric_group_handle_t *phMetricGroup);
    ze_result_t createMetricGroupsFromMetrics(std::vector<zet_metric_handle_t> &metricList,
                                              const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
                                              const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                              uint32_t *maxMetricGroupCount,
                                              std::vector<zet_metric_group_handle_t> &metricGroupList) override;
    ze_result_t appendMarker(zet_command_list_handle_t hCommandList, zet_metric_group_handle_t hMetricGroup, uint32_t value) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    void metricGroupCreate(const char name[ZET_MAX_METRIC_GROUP_NAME],
                           const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                           zet_metric_group_sampling_type_flag_t samplingType,
                           zet_metric_group_handle_t *pMetricGroupHandle);

  protected:
    ze_result_t initializationState = ZE_RESULT_ERROR_UNINITIALIZED;
    const MetricDeviceContext &metricDeviceContext;
    std::unique_ptr<MetricEnumeration> metricEnumeration;
    std::unique_ptr<MetricsLibrary> metricsLibrary;
    MetricStreamer *pMetricStreamer = nullptr;
    bool useCompute = false;
    std::unique_ptr<MetricOAOsInterface> metricOAOsInterface = nullptr;
    std::unique_ptr<MultiDomainDeferredActivationTracker> activationTracker{};
    ze_result_t getTimerResolution(uint64_t &resolution);
    ze_result_t getTimestampValidBits(uint64_t &validBits);
};

template <>
OaMetricSourceImp &MetricDeviceContext::getMetricSource<OaMetricSourceImp>() const;

} // namespace L0
