/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned, IGFX_BMG);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, whenAdjustScratchSizeThenSizeIsNotChanged, IGFX_BMG);
