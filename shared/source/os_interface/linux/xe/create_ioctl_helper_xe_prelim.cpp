/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe_prelim.h"

namespace NEO {
std::unique_ptr<IoctlHelperXe> IoctlHelperXe::create(Drm &drmArg) {
    return std::make_unique<IoctlHelperXePrelim>(drmArg);
}
} // namespace NEO
