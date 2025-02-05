/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/gen12lp/hw_info_dg1.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_tgllp_and_later.inl"

constexpr static auto gfxProduct = IGFX_DG1;

#include "shared/source/gen12lp/dg1/os_agnostic_product_helper_dg1.inl"
#include "shared/source/gen12lp/os_agnostic_product_helper_gen12lp.inl"

namespace NEO {

template <>
void ProductHelperHw<gfxProduct>::setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) const {
    coherencyFlag = false;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
