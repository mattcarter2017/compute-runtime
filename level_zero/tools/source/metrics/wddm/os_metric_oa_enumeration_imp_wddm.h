/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/os_interface_metric.h"

#include "metrics_discovery_api.h"

namespace L0 {
struct Device;
struct MetricEnumeration;

bool getWddmAdapterId(uint32_t &major, uint32_t &minor, Device &device);

MetricsDiscovery::IAdapter_1_13 *getWddmMetricsAdapter(MetricEnumeration *metricEnumeration);

class MetricOAWindowsImp : public MetricOAOsInterface {
  public:
    MetricOAWindowsImp() = default;
    ~MetricOAWindowsImp() override = default;
    ze_result_t getMetricsTimerResolution(uint64_t &timerResolution) override;
};

} // namespace L0
