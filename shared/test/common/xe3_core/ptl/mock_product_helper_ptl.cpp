/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/mock_product_helper_hw.h"

constexpr static auto gfxProduct = IGFX_PTL;

#include "shared/test/common/helpers/mock_product_helper_hw.inl"

template struct NEO::MockProductHelperHw<gfxProduct>;
