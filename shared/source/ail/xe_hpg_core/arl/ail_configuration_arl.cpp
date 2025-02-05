/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"
#include "shared/source/helpers/hw_info.h"

#include "aubstream/engine_node.h"

#include <map>
#include <vector>

namespace NEO {

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapARL = {{"svchost", {AILEnumeration::disableDirectSubmission}},
                                                                             {"aomhost64", {AILEnumeration::disableDirectSubmission}},
                                                                             {"Zoom", {AILEnumeration::disableDirectSubmission}}};

static EnableAIL<IGFX_ARROWLAKE> enableAILARL;

template <>
void AILConfigurationHw<IGFX_ARROWLAKE>::applyExt(HardwareInfo &hwInfo) {
    auto search = applicationMapARL.find(processName);
    if (search != applicationMapARL.end()) {
        for (size_t i = 0; i < search->second.size(); ++i) {
            switch (search->second[i]) {
            case AILEnumeration::disableDirectSubmission:
                hwInfo.capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported = false;
            default:
                break;
            }
        }
    }
}

template <>
bool AILConfigurationHw<IGFX_ARROWLAKE>::isBufferPoolEnabled() {
    auto iterator = applicationsBufferPoolDisabledXe.find(processName);
    return iterator == applicationsBufferPoolDisabledXe.end();
}

template <>
bool AILConfigurationHw<IGFX_ARROWLAKE>::limitAmountOfDeviceMemoryForRecycling() {
    auto iterator = applicationsDeviceUSMRecyclingLimited.find(processName);
    return iterator != applicationsDeviceUSMRecyclingLimited.end();
}

template class AILConfigurationHw<IGFX_ARROWLAKE>;

} // namespace NEO
