/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_interface.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/os_library.h"

#include <memory>

namespace NEO {

static std::unique_ptr<OsLibrary> gmmLib;

namespace GmmInterface {

GMM_STATUS initialize(GMM_INIT_IN_ARGS *pInArgs, GMM_INIT_OUT_ARGS *pOutArgs) {
    if (!gmmLib) {
        gmmLib.reset(OsLibrary::loadFunc({GMM_UMD_DLL}));
        UNRECOVERABLE_IF(!gmmLib);
    }
    auto initGmmFunc = reinterpret_cast<decltype(&InitializeGmm)>(gmmLib->getProcAddress(GMM_ADAPTER_INIT_NAME));
    UNRECOVERABLE_IF(!initGmmFunc);
    return initGmmFunc(pInArgs, pOutArgs);
}

void destroy(GMM_INIT_OUT_ARGS *pInArgs) {
    auto destroyGmmFunc = reinterpret_cast<decltype(&GmmAdapterDestroy)>(gmmLib->getProcAddress(GMM_ADAPTER_DESTROY_NAME));
    UNRECOVERABLE_IF(!destroyGmmFunc);
    destroyGmmFunc(pInArgs);
}
} // namespace GmmInterface
} // namespace NEO
