/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_streamer.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/zet_intel_gpu_metric.h"
#include "level_zero/zet_intel_gpu_metric_export.h"
#include <level_zero/zet_api.h>

#include <cstring>

namespace L0 {
constexpr uint32_t ipSamplinDomainId = 100u;

std::unique_ptr<IpSamplingMetricSourceImp> IpSamplingMetricSourceImp::create(const MetricDeviceContext &metricDeviceContext) {
    return std::unique_ptr<IpSamplingMetricSourceImp>(new (std::nothrow) IpSamplingMetricSourceImp(metricDeviceContext));
}

IpSamplingMetricSourceImp::IpSamplingMetricSourceImp(const MetricDeviceContext &metricDeviceContext) : metricDeviceContext(metricDeviceContext) {
    metricIPSamplingpOsInterface = MetricIpSamplingOsInterface::create(metricDeviceContext.getDevice());
    activationTracker = std::make_unique<MultiDomainDeferredActivationTracker>(metricDeviceContext.getSubDeviceIndex());
    type = MetricSource::metricSourceTypeIpSampling;
}

ze_result_t IpSamplingMetricSourceImp::getTimerResolution(uint64_t &resolution) {
    resolution = metricDeviceContext.getDevice().getNEODevice()->getDeviceInfo().outProfilingTimerClock;
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::getTimestampValidBits(uint64_t &validBits) {
    validBits = metricDeviceContext.getDevice().getNEODevice()->getHardwareInfo().capabilityTable.timestampValidBits;
    return ZE_RESULT_SUCCESS;
}

void IpSamplingMetricSourceImp::enable() {
    isEnabled = metricIPSamplingpOsInterface->isDependencyAvailable();
}

bool IpSamplingMetricSourceImp::isAvailable() {
    return isEnabled;
}

void IpSamplingMetricSourceImp::cacheMetricGroup() {

    const auto deviceImp = static_cast<DeviceImp *>(&metricDeviceContext.getDevice());
    if (metricDeviceContext.isImplicitScalingCapable()) {
        std::vector<IpSamplingMetricGroupImp *> subDeviceMetricGroup = {};
        subDeviceMetricGroup.reserve(deviceImp->subDevices.size());

        // Prepare cached metric group for sub-devices
        for (auto &subDevice : deviceImp->subDevices) {
            IpSamplingMetricSourceImp &source = subDevice->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
            // 1 metric group available for IP Sampling
            uint32_t count = 1;
            zet_metric_group_handle_t hMetricGroup = {};
            const auto result = source.metricGroupGet(&count, &hMetricGroup);
            // Getting MetricGroup from sub-device cannot fail, since RootDevice is successful
            UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);
            subDeviceMetricGroup.push_back(static_cast<IpSamplingMetricGroupImp *>(MetricGroup::fromHandle(hMetricGroup)));
        }

        IpSamplingMetricSourceImp &source = deviceImp->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        cachedMetricGroup = MultiDeviceIpSamplingMetricGroupImp::create(source, subDeviceMetricGroup);
        return;
    }

    std::vector<IpSamplingMetricImp> metrics = {};
    auto &l0GfxCoreHelper = deviceImp->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    metrics.reserve(l0GfxCoreHelper.getIpSamplingMetricCount());
    metricSourceCount = l0GfxCoreHelper.getIpSamplingMetricCount();

    zet_metric_properties_t metricProperties = {};

    metricProperties.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
    metricProperties.pNext = nullptr;
    strcpy_s(metricProperties.component, ZET_MAX_METRIC_COMPONENT, "XVE");
    metricProperties.tierNumber = 4;
    metricProperties.resultType = ZET_VALUE_TYPE_UINT64;

    // Preparing properties for IP seperately because of unique values
    strcpy_s(metricProperties.name, ZET_MAX_METRIC_NAME, "IP");
    strcpy_s(metricProperties.description, ZET_MAX_METRIC_DESCRIPTION, "IP address");
    metricProperties.metricType = ZET_METRIC_TYPE_IP;
    strcpy_s(metricProperties.resultUnits, ZET_MAX_METRIC_RESULT_UNITS, "Address");
    metrics.push_back(IpSamplingMetricImp(*this, metricProperties));

    std::vector<std::pair<const char *, const char *>> stallSamplingReportList = l0GfxCoreHelper.getStallSamplingReportMetrics();

    // Preparing properties for others because of common values
    metricProperties.metricType = ZET_METRIC_TYPE_EVENT;
    strcpy_s(metricProperties.resultUnits, ZET_MAX_METRIC_RESULT_UNITS, "Events");

    for (auto &property : stallSamplingReportList) {
        strcpy_s(metricProperties.name, ZET_MAX_METRIC_NAME, property.first);
        strcpy_s(metricProperties.description, ZET_MAX_METRIC_DESCRIPTION, property.second);
        metrics.push_back(IpSamplingMetricImp(*this, metricProperties));
    }

    cachedMetricGroup = IpSamplingMetricGroupImp::create(*this, metrics);
    DEBUG_BREAK_IF(cachedMetricGroup == nullptr);
}

ze_result_t IpSamplingMetricSourceImp::metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {

    if (!isEnabled) {
        *pCount = 0;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (*pCount == 0) {
        *pCount = 1;
        return ZE_RESULT_SUCCESS;
    }

    if (cachedMetricGroup == nullptr) {
        cacheMetricGroup();
    }

    DEBUG_BREAK_IF(phMetricGroups == nullptr);
    phMetricGroups[0] = cachedMetricGroup->toHandle();
    *pCount = 1;

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::appendMetricMemoryBarrier(CommandList &commandList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t IpSamplingMetricSourceImp::activateMetricGroupsPreferDeferred(uint32_t count,
                                                                          zet_metric_group_handle_t *phMetricGroups) {
    auto status = activationTracker->activateMetricGroupsDeferred(count, phMetricGroups);
    if (!status) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::activateMetricGroupsAlreadyDeferred() {
    return activationTracker->activateMetricGroupsAlreadyDeferred();
}

bool IpSamplingMetricSourceImp::isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const {
    return activationTracker->isMetricGroupActivated(hMetricGroup);
}

void IpSamplingMetricSourceImp::setMetricOsInterface(std::unique_ptr<MetricIpSamplingOsInterface> &metricIPSamplingpOsInterface) {
    this->metricIPSamplingpOsInterface = std::move(metricIPSamplingpOsInterface);
}

ze_result_t IpSamplingMetricGroupBase::getExportData(const uint8_t *pRawData, size_t rawDataSize, size_t *pExportDataSize,
                                                     uint8_t *pExportData) {
    const auto expectedExportDataSize = sizeof(zet_intel_metric_df_gpu_export_data_format_t) + rawDataSize;

    if (*pExportDataSize == 0u) {
        *pExportDataSize = expectedExportDataSize;
        return ZE_RESULT_SUCCESS;
    }

    if (*pExportDataSize < expectedExportDataSize) {
        METRICS_LOG_ERR("Incorrect Size Passed. Returning 0x%x", ZE_RESULT_ERROR_INVALID_SIZE);
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    zet_intel_metric_df_gpu_export_data_format_t *exportData = reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(pExportData);
    exportData->header.type = ZET_INTEL_METRIC_DF_SOURCE_TYPE_IPSAMPLING;
    exportData->header.version.major = ZET_INTEL_GPU_METRIC_EXPORT_VERSION_MAJOR;
    exportData->header.version.minor = ZET_INTEL_GPU_METRIC_EXPORT_VERSION_MINOR;
    exportData->header.rawDataOffset = sizeof(zet_intel_metric_df_gpu_export_data_format_t);
    exportData->header.rawDataSize = rawDataSize;

    // Append the rawData
    memcpy_s(reinterpret_cast<void *>(pExportData + exportData->header.rawDataOffset), rawDataSize, pRawData, rawDataSize);

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                                                 uint32_t *pConcurrentGroupCount,
                                                                 uint32_t *pCountPerConcurrentGroup) {

    if (*pConcurrentGroupCount == 0) {
        *pConcurrentGroupCount = static_cast<uint32_t>(hMetricGroups.size());
        return ZE_RESULT_SUCCESS;
    }

    *pConcurrentGroupCount = std::min(*pConcurrentGroupCount, static_cast<uint32_t>(hMetricGroups.size()));
    // Each metric group is in unique container
    for (uint32_t index = 0; index < *pConcurrentGroupCount; index++) {
        pCountPerConcurrentGroup[index] = 1;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::handleMetricGroupExtendedProperties(zet_metric_group_handle_t hMetricGroup, void *pNext) {
    ze_result_t retVal = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    while (pNext) {
        auto extendedProperties = reinterpret_cast<zet_base_properties_t *>(pNext);

        if (extendedProperties->stype == ZET_STRUCTURE_TYPE_METRIC_GLOBAL_TIMESTAMPS_RESOLUTION_EXP) {

            zet_metric_global_timestamps_resolution_exp_t *metricsTimestampProperties =
                reinterpret_cast<zet_metric_global_timestamps_resolution_exp_t *>(extendedProperties);

            getTimerResolution(metricsTimestampProperties->timerResolution);
            getTimestampValidBits(metricsTimestampProperties->timestampValidBits);
            retVal = ZE_RESULT_SUCCESS;
        }

        if (extendedProperties->stype == ZET_STRUCTURE_TYPE_METRIC_GROUP_TYPE_EXP) {
            zet_metric_group_type_exp_t *groupType = reinterpret_cast<zet_metric_group_type_exp_t *>(extendedProperties);
            groupType->type = ZET_METRIC_GROUP_TYPE_EXP_FLAG_OTHER;
            retVal = ZE_RESULT_SUCCESS;
        }

        pNext = extendedProperties->pNext;
    }

    return retVal;
}

IpSamplingMetricGroupImp::IpSamplingMetricGroupImp(IpSamplingMetricSourceImp &metricSource,
                                                   std::vector<IpSamplingMetricImp> &metrics) : IpSamplingMetricGroupBase(metricSource) {
    this->metrics.reserve(metrics.size());
    for (const auto &metric : metrics) {
        this->metrics.push_back(std::make_unique<IpSamplingMetricImp>(metric));
    }

    properties.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    properties.pNext = nullptr;
    strcpy_s(properties.name, ZET_MAX_METRIC_GROUP_NAME, "EuStallSampling");
    strcpy_s(properties.description, ZET_MAX_METRIC_GROUP_DESCRIPTION, "EU stall sampling");
    properties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED;
    properties.domain = ipSamplinDomainId;
    properties.metricCount = this->getMetricSource().metricSourceCount;
}

ze_result_t IpSamplingMetricGroupImp::getProperties(zet_metric_group_properties_t *pProperties) {
    void *pNext = pProperties->pNext;
    *pProperties = properties;
    pProperties->pNext = pNext;

    if (pNext) {
        return metricSource.handleMetricGroupExtendedProperties(toHandle(), pNext);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricGroupImp::metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) {

    if (*pCount == 0) {
        *pCount = static_cast<uint32_t>(metrics.size());
        return ZE_RESULT_SUCCESS;
    }
    // User is expected to allocate space.
    DEBUG_BREAK_IF(phMetrics == nullptr);

    *pCount = std::min(*pCount, static_cast<uint32_t>(metrics.size()));

    for (uint32_t i = 0; i < *pCount; i++) {
        phMetrics[i] = metrics[i]->toHandle();
    }

    return ZE_RESULT_SUCCESS;
}

bool IpSamplingMetricGroupImp::isMultiDeviceCaptureData(const size_t rawDataSize, const uint8_t *pRawData) {
    if (rawDataSize >= sizeof(IpSamplingMetricDataHeader)) {
        const auto header = reinterpret_cast<const IpSamplingMetricDataHeader *>(pRawData);
        return header->magic == IpSamplingMetricDataHeader::magicValue;
    }
    return false;
}

ze_result_t IpSamplingMetricGroupImp::calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                            const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                                            zet_typed_value_t *pMetricValues) {
    const bool calculateCountOnly = *pMetricValueCount == 0;

    if (isMultiDeviceCaptureData(rawDataSize, pRawData)) {
        METRICS_LOG_INFO("%s", "The call is not supported for multiple devices");
        METRICS_LOG_INFO("%s", "Please use zetMetricGroupCalculateMultipleMetricValuesExp instead");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (calculateCountOnly) {
        return getCalculatedMetricCount(rawDataSize, *pMetricValueCount);
    } else {
        return getCalculatedMetricValues(type, rawDataSize, pRawData, *pMetricValueCount, pMetricValues);
    }
}

ze_result_t IpSamplingMetricGroupImp::calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                               const uint8_t *pRawData, uint32_t *pSetCount,
                                                               uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                               zet_typed_value_t *pMetricValues) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    const bool calculateCountOnly = (*pTotalMetricValueCount == 0) || (*pSetCount == 0);
    if (calculateCountOnly) {
        *pTotalMetricValueCount = 0;
        *pSetCount = 0;
    }

    if (!isMultiDeviceCaptureData(rawDataSize, pRawData)) {
        result = this->calculateMetricValues(type, rawDataSize, pRawData, pTotalMetricValueCount, pMetricValues);
    } else {
        if (calculateCountOnly) {
            result = getCalculatedMetricCount(pRawData, rawDataSize, *pTotalMetricValueCount, 0);
        } else {
            result = getCalculatedMetricValues(type, rawDataSize, pRawData, *pTotalMetricValueCount, pMetricValues, 0);
        }
    }

    if ((result == ZE_RESULT_SUCCESS) || (result == ZE_RESULT_WARNING_DROPPED_DATA)) {
        *pSetCount = 1;
        if (!calculateCountOnly) {
            pMetricCounts[0] = *pTotalMetricValueCount;
        }
    } else {
        if (!calculateCountOnly) {
            pMetricCounts[0] = 0;
        }
    }

    return result;
}

ze_result_t getDeviceTimestamps(DeviceImp *deviceImp, const ze_bool_t synchronizedWithHost,
                                uint64_t *globalTimestamp, uint64_t *metricTimestamp) {

    ze_result_t result;
    uint64_t hostTimestamp;
    uint64_t deviceTimestamp;

    result = deviceImp->getGlobalTimestamps(&hostTimestamp, &deviceTimestamp);
    if (result != ZE_RESULT_SUCCESS) {
        *globalTimestamp = 0;
        *metricTimestamp = 0;
    } else {
        if (synchronizedWithHost) {
            *globalTimestamp = hostTimestamp;
        } else {
            *globalTimestamp = deviceTimestamp;
        }
        *metricTimestamp = deviceTimestamp;
        result = ZE_RESULT_SUCCESS;
    }

    return result;
}

ze_result_t IpSamplingMetricGroupImp::getMetricTimestampsExp(const ze_bool_t synchronizedWithHost,
                                                             uint64_t *globalTimestamp,
                                                             uint64_t *metricTimestamp) {
    DeviceImp *deviceImp = static_cast<DeviceImp *>(&getMetricSource().getMetricDeviceContext().getDevice());
    return getDeviceTimestamps(deviceImp, synchronizedWithHost, globalTimestamp, metricTimestamp);
}

ze_result_t IpSamplingMetricGroupImp::getCalculatedMetricCount(const size_t rawDataSize,
                                                               uint32_t &metricValueCount) {

    if ((rawDataSize % IpSamplingMetricGroupBase::rawReportSize) != 0) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    const uint32_t rawReportCount = static_cast<uint32_t>(rawDataSize) / IpSamplingMetricGroupBase::rawReportSize;
    metricValueCount = rawReportCount * properties.metricCount;
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricGroupImp::getCalculatedMetricCount(const uint8_t *pMultiMetricData, const size_t rawDataSize, uint32_t &metricValueCount, const uint32_t setIndex) {

    // Iterate through headers and assign required sizes
    auto processedSize = 0u;
    while (processedSize < rawDataSize) {
        auto processMetricData = pMultiMetricData + processedSize;
        if (!isMultiDeviceCaptureData(rawDataSize - processedSize, processMetricData)) {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }

        auto header = reinterpret_cast<const IpSamplingMetricDataHeader *>(processMetricData);
        processedSize += sizeof(IpSamplingMetricDataHeader) + header->rawDataSize;
        if (header->setIndex != setIndex) {
            continue;
        }

        auto currTotalMetricValueCount = 0u;
        auto result = this->getCalculatedMetricCount(header->rawDataSize, currTotalMetricValueCount);
        if (result != ZE_RESULT_SUCCESS) {
            metricValueCount = 0;
            return result;
        }
        metricValueCount += currTotalMetricValueCount;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricGroupImp::getCalculatedMetricValues(const zet_metric_group_calculation_type_t type, const size_t rawDataSize, const uint8_t *pMultiMetricData,
                                                                uint32_t &metricValueCount,
                                                                zet_typed_value_t *pCalculatedData, const uint32_t setIndex) {

    auto processedSize = 0u;
    auto isDataDropped = false;
    auto requestTotalMetricValueCount = metricValueCount;

    while (processedSize < rawDataSize && requestTotalMetricValueCount > 0) {
        auto processMetricData = pMultiMetricData + processedSize;
        if (!isMultiDeviceCaptureData(rawDataSize - processedSize, processMetricData)) {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }

        auto header = reinterpret_cast<const IpSamplingMetricDataHeader *>(processMetricData);
        processedSize += header->rawDataSize + sizeof(IpSamplingMetricDataHeader);
        if (header->setIndex != setIndex) {
            continue;
        }

        auto processMetricRawData = processMetricData + sizeof(IpSamplingMetricDataHeader);
        auto currTotalMetricValueCount = requestTotalMetricValueCount;
        auto result = this->calculateMetricValues(type, header->rawDataSize, processMetricRawData, &currTotalMetricValueCount, pCalculatedData);
        if (result != ZE_RESULT_SUCCESS) {
            if (result == ZE_RESULT_WARNING_DROPPED_DATA) {
                isDataDropped = true;
            } else {
                metricValueCount = 0;
                return result;
            }
        }
        pCalculatedData += currTotalMetricValueCount;
        requestTotalMetricValueCount -= currTotalMetricValueCount;
    }

    metricValueCount -= requestTotalMetricValueCount;
    return isDataDropped ? ZE_RESULT_WARNING_DROPPED_DATA : ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricGroupImp::getCalculatedMetricValues(const zet_metric_group_calculation_type_t type, const size_t rawDataSize, const uint8_t *pRawData,
                                                                uint32_t &metricValueCount,
                                                                zet_typed_value_t *pCalculatedData) {
    bool dataOverflow = false;
    std::map<uint64_t, void *> stallReportDataMap;

    // MAX_METRIC_VALUES is not supported yet.
    if (type != ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    DEBUG_BREAK_IF(pCalculatedData == nullptr);

    const uint32_t rawReportSize = IpSamplingMetricGroupBase::rawReportSize;

    if ((rawDataSize % rawReportSize) != 0) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    const uint32_t rawReportCount = static_cast<uint32_t>(rawDataSize) / rawReportSize;

    DeviceImp *deviceImp = static_cast<DeviceImp *>(&this->getMetricSource().getMetricDeviceContext().getDevice());
    auto &l0GfxCoreHelper = deviceImp->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    for (const uint8_t *pRawIpData = pRawData; pRawIpData < pRawData + (rawReportCount * rawReportSize); pRawIpData += rawReportSize) {
        dataOverflow |= l0GfxCoreHelper.stallIpDataMapUpdate(stallReportDataMap, pRawIpData);
    }

    metricValueCount = std::min<uint32_t>(metricValueCount, static_cast<uint32_t>(stallReportDataMap.size()) * properties.metricCount);
    std::vector<zet_typed_value_t> ipDataValues;
    uint32_t i = 0;
    for (auto it = stallReportDataMap.begin(); it != stallReportDataMap.end(); ++it) {
        l0GfxCoreHelper.stallSumIpDataToTypedValues(it->first, it->second, ipDataValues);
        for (auto jt = ipDataValues.begin(); (jt != ipDataValues.end()) && (i < metricValueCount); jt++, i++) {
            *(pCalculatedData + i) = *jt;
        }
        ipDataValues.clear();
    }
    l0GfxCoreHelper.stallIpDataMapDelete(stallReportDataMap);
    stallReportDataMap.clear();

    return dataOverflow ? ZE_RESULT_WARNING_DROPPED_DATA : ZE_RESULT_SUCCESS;
}

zet_metric_group_handle_t IpSamplingMetricGroupImp::getMetricGroupForSubDevice(const uint32_t subDeviceIndex) {
    return toHandle();
}

std::unique_ptr<IpSamplingMetricGroupImp> IpSamplingMetricGroupImp::create(IpSamplingMetricSourceImp &metricSource,
                                                                           std::vector<IpSamplingMetricImp> &ipSamplingMetrics) {
    return std::unique_ptr<IpSamplingMetricGroupImp>(new (std::nothrow) IpSamplingMetricGroupImp(metricSource, ipSamplingMetrics));
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::getProperties(zet_metric_group_properties_t *pProperties) {
    return subDeviceMetricGroup[0]->getProperties(pProperties);
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) {
    return subDeviceMetricGroup[0]->metricGet(pCount, phMetrics);
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                                       const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                                                       zet_typed_value_t *pMetricValues) {
    return subDeviceMetricGroup[0]->calculateMetricValues(type, rawDataSize, pRawData, pMetricValueCount, pMetricValues);
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                                          const uint8_t *pRawData, uint32_t *pSetCount,
                                                                          uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                                          zet_typed_value_t *pMetricValues) {

    const bool calculateCountOnly = *pSetCount == 0 || *pTotalMetricValueCount == 0;
    bool isDroppedData = false;
    ze_result_t result = ZE_RESULT_SUCCESS;

    if (calculateCountOnly) {
        *pSetCount = 0;
        *pTotalMetricValueCount = 0;
        for (uint32_t setIndex = 0; setIndex < subDeviceMetricGroup.size(); setIndex++) {
            uint32_t currTotalMetricValueCount = 0;
            result = subDeviceMetricGroup[setIndex]->getCalculatedMetricCount(pRawData, rawDataSize, currTotalMetricValueCount, setIndex);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            *pTotalMetricValueCount += currTotalMetricValueCount;
        }
        *pSetCount = static_cast<uint32_t>(subDeviceMetricGroup.size());
    } else {
        memset(pMetricCounts, 0, *pSetCount);
        const auto maxSets = std::min<uint32_t>(static_cast<uint32_t>(subDeviceMetricGroup.size()), *pSetCount);

        auto tempTotalMetricValueCount = *pTotalMetricValueCount;
        for (uint32_t setIndex = 0; setIndex < maxSets; setIndex++) {
            uint32_t currTotalMetricValueCount = tempTotalMetricValueCount;
            result = subDeviceMetricGroup[setIndex]->getCalculatedMetricValues(type, rawDataSize, pRawData, currTotalMetricValueCount, pMetricValues, setIndex);
            if (result != ZE_RESULT_SUCCESS) {
                if (result == ZE_RESULT_WARNING_DROPPED_DATA) {
                    isDroppedData = true;
                } else {
                    memset(pMetricCounts, 0, *pSetCount);
                    return result;
                }
            }

            pMetricCounts[setIndex] = currTotalMetricValueCount;
            pMetricValues += currTotalMetricValueCount;
            tempTotalMetricValueCount -= currTotalMetricValueCount;
        }
        *pTotalMetricValueCount -= tempTotalMetricValueCount;
    }

    return isDroppedData ? ZE_RESULT_WARNING_DROPPED_DATA : ZE_RESULT_SUCCESS;
}

zet_metric_group_handle_t MultiDeviceIpSamplingMetricGroupImp::getMetricGroupForSubDevice(const uint32_t subDeviceIndex) {
    return subDeviceMetricGroup[subDeviceIndex]->toHandle();
}

void MultiDeviceIpSamplingMetricGroupImp::closeSubDeviceStreamers(std::vector<IpSamplingMetricStreamerImp *> &subDeviceStreamers) {
    for (auto streamer : subDeviceStreamers) {
        streamer->close();
    }
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::getMetricTimestampsExp(const ze_bool_t synchronizedWithHost,
                                                                        uint64_t *globalTimestamp,
                                                                        uint64_t *metricTimestamp) {
    DeviceImp *deviceImp = static_cast<DeviceImp *>(&subDeviceMetricGroup[0]->getMetricSource().getMetricDeviceContext().getDevice());
    return getDeviceTimestamps(deviceImp, synchronizedWithHost, globalTimestamp, metricTimestamp);
}

std::unique_ptr<MultiDeviceIpSamplingMetricGroupImp> MultiDeviceIpSamplingMetricGroupImp::create(
    MetricSource &metricSource,
    std::vector<IpSamplingMetricGroupImp *> &subDeviceMetricGroup) {
    UNRECOVERABLE_IF(subDeviceMetricGroup.size() == 0);
    return std::unique_ptr<MultiDeviceIpSamplingMetricGroupImp>(new (std::nothrow) MultiDeviceIpSamplingMetricGroupImp(metricSource, subDeviceMetricGroup));
}

IpSamplingMetricImp::IpSamplingMetricImp(MetricSource &metricSource, zet_metric_properties_t &properties) : MetricImp(metricSource), properties(properties) {
}

ze_result_t IpSamplingMetricImp::getProperties(zet_metric_properties_t *pProperties) {
    *pProperties = properties;
    return ZE_RESULT_SUCCESS;
}

template <>
IpSamplingMetricSourceImp &MetricDeviceContext::getMetricSource<IpSamplingMetricSourceImp>() const {
    return static_cast<IpSamplingMetricSourceImp &>(*metricSources.at(MetricSource::metricSourceTypeIpSampling));
}

} // namespace L0
