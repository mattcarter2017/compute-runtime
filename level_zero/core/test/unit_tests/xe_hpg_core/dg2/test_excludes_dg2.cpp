/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

namespace L0 {
namespace ult {
HWTEST_EXCLUDE_PRODUCT(ModuleTranslationUnitTest, givenAtLeastXeHpgCoreWhenGetInternalOptionsThenCorrectBuildOptionIsSet_IsAtLeastXeHpgCore, IGFX_DG2);
}
} // namespace L0
