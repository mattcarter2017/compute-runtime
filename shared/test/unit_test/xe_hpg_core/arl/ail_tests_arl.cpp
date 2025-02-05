/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

using AILTestsARL = ::testing::Test;

HWTEST2_F(AILTestsARL, givenArlWhenSvchostAppIsDetectedThenDisableDirectSubmission, IsARL) {
    AILWhitebox<productFamily> ail;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &capabilityTable = hwInfo.capabilityTable;
    auto defaultEngineSupportedValue = capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported;

    ail.processName = "UnknownProcess";
    ail.apply(hwInfo);
    EXPECT_EQ(defaultEngineSupportedValue, capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported);

    ail.processName = "svchost";
    ail.apply(hwInfo);
    EXPECT_FALSE(capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported);

    ail.processName = "aomhost64";
    ail.apply(hwInfo);
    EXPECT_FALSE(capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported);

    ail.processName = "Zoom";
    ail.apply(hwInfo);
    EXPECT_FALSE(capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported);
}

} // namespace NEO
