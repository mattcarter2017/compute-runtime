/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"

using namespace NEO;

void StateComputeModeProperties::setPropertiesExtraPerContext() {
}

void StateComputeModeProperties::copyPropertiesExtra(const StateComputeModeProperties &properties) {
}

bool StateComputeModeProperties::isDirtyExtra() const {
    return false;
}

void StateComputeModeProperties::clearIsDirtyExtraPerContext() {
}

void StateComputeModeProperties::resetStateExtra() {
}
