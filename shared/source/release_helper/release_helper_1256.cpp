/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1256;

} // namespace NEO
#include "shared/source/release_helper/release_helper_common_xe_hpg.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
