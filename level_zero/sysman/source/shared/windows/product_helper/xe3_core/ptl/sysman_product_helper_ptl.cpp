/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.inl"

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_PTL;

template <>
bool SysmanProductHelperHw<gfxProduct>::isZesInitSupported() {
    return true;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0
