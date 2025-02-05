/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {
class MockReleaseHelper : public ReleaseHelper {
  public:
    MockReleaseHelper() : ReleaseHelper(0) {}
    ADDMETHOD_CONST_NOBASE(isAdjustWalkOrderAvailable, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isMatrixMultiplyAccumulateSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isDotProductAccumulateSystolicSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isPipeControlPriorToNonPipelinedStateCommandsWARequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isPipeControlPriorToPipelineSelectWaRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isProgramAllStateComputeCommandFieldsWARequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isSplitMatrixMultiplyAccumulateSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isBFloat16ConversionSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isAuxSurfaceModeOverrideRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isResolvingSubDeviceIDNeeded, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isDirectSubmissionSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isRcsExposureDisabled, bool, false, ());
    ADDMETHOD_CONST_NOBASE(getSupportedNumGrfs, std::vector<uint32_t>, {128}, ());
    ADDMETHOD_CONST_NOBASE(isBindlessAddressingDisabled, bool, true, ());
    ADDMETHOD_CONST_NOBASE(isGlobalBindlessAllocatorEnabled, bool, false, ());
    ADDMETHOD_CONST_NOBASE(getNumThreadsPerEu, uint32_t, 8u, ());
    ADDMETHOD_CONST_NOBASE(getTotalMemBankSize, uint64_t, 32ull * MemoryConstants::gigaByte, ());
    ADDMETHOD_CONST_NOBASE(getThreadsPerEUConfigs, const ThreadsPerEUConfigs, {}, (uint32_t numThreadsPerEu));
    ADDMETHOD_CONST_NOBASE(getDeviceConfigString, const std::string, {}, (uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount));
    ADDMETHOD_CONST_NOBASE(isRayTracingSupported, bool, true, ());
    ADDMETHOD_CONST_NOBASE(getAdditionalFp16Caps, uint32_t, {}, ());
    ADDMETHOD_CONST_NOBASE(getAdditionalExtraCaps, uint32_t, {}, ());
    ADDMETHOD_CONST_NOBASE(getStackSizePerRay, uint32_t, {}, ());
    ADDMETHOD_CONST_NOBASE(isLocalOnlyAllowed, bool, {}, ());
    ADDMETHOD_CONST_NOBASE(isDummyBlitWaRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isNumRtStacksPerDssFixedValue, bool, true, ());
    ADDMETHOD_CONST_NOBASE(getFtrXe2Compression, bool, false, ());

    const SizeToPreferredSlmValueArray &getSizeToPreferredSlmValue(bool isHeapless) const override {
        static SizeToPreferredSlmValueArray sizeToPreferredSlmValue = {};
        return sizeToPreferredSlmValue;
    }
};
} // namespace NEO
