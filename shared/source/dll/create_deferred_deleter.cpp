/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/deferred_deleter.h"

namespace NEO {
std::unique_ptr<DeferredDeleter> createDeferredDeleter() {
    return std::unique_ptr<DeferredDeleter>(new DeferredDeleter());
}
} // namespace NEO
