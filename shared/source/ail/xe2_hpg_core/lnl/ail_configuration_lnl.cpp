/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ail_configuration_lnl.inl"

#include "shared/source/ail/ail_configuration_base.inl"

namespace NEO {
static EnableAIL<IGFX_LUNARLAKE> enableAILLNL;

template <>
bool AILConfigurationHw<IGFX_LUNARLAKE>::isBufferPoolEnabled() {
    auto iterator = applicationsBufferPoolDisabled.find(processName);
    return iterator == applicationsBufferPoolDisabled.end();
}

template <>
bool AILConfigurationHw<IGFX_LUNARLAKE>::is256BPrefetchDisableRequired() {
    auto iterator = applicationsOverfetchDisabled.find(processName);
    return iterator != applicationsOverfetchDisabled.end();
}

template class AILConfigurationHw<IGFX_LUNARLAKE>;
} // namespace NEO
