/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_xe2_and_later.inl"
#include "shared/source/xe3_core/hw_cmds_ptl.h"
#include "shared/source/xe3_core/hw_info_ptl.h"

constexpr static auto gfxProduct = IGFX_PTL;

#include "shared/source/os_interface/linux/product_helper_mtl_and_later.inl"
#include "shared/source/xe3_core/os_agnostic_product_helper_xe3_core.inl"
#include "shared/source/xe3_core/ptl/os_agnostic_product_helper_ptl.inl"

namespace NEO {

#include "shared/source/os_interface/linux/product_helper_xe_hpc_and_later.inl"

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    enableCompression(hwInfo);

    hwInfo->featureTable.flags.ftr57bGPUAddressing = (hwInfo->capabilityTable.gpuAddressSpace == maxNBitValue(57));

    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::isDisableScratchPagesSupported() const {
    return true;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
