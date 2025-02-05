/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperPmtTest = ::testing::Test;

HWTEST2_F(SysmanProductHelperPmtTest, GivenSysmanProductHelperInstanceWhenGetGuidToKeyOffsetMapIsCalledThenValidMapIsReturned, IsDG1) {
    const std::map<std::string, std::map<std::string, uint64_t>> mockDg1GuidToKeyOffsetMap = {{"0x490e01",
                                                                                               {{"PACKAGE_ENERGY", 0x420},
                                                                                                {"COMPUTE_TEMPERATURES", 0x68}}}};

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pGuidToKeyOffsetMap = pSysmanProductHelper->getGuidToKeyOffsetMap();
    EXPECT_NE(nullptr, pGuidToKeyOffsetMap);
    EXPECT_EQ(mockDg1GuidToKeyOffsetMap.at("0x490e01").at("PACKAGE_ENERGY"), (*pGuidToKeyOffsetMap).at("0x490e01").at("PACKAGE_ENERGY"));
    EXPECT_EQ(mockDg1GuidToKeyOffsetMap.at("0x490e01").at("COMPUTE_TEMPERATURES"), (*pGuidToKeyOffsetMap).at("0x490e01").at("COMPUTE_TEMPERATURES"));
}

HWTEST2_F(SysmanProductHelperPmtTest, GivenSysmanProductHelperInstanceWhenGetGuidToKeyOffsetMapIsCalledThenValidMapIsReturned, IsDG2) {
    const std::map<std::string, std::map<std::string, uint64_t>> mockDg2GuidToKeyOffsetMap = {{"0x4f95",
                                                                                               {{"PACKAGE_ENERGY", 1032},
                                                                                                {"SOC_TEMPERATURES", 56}}}};

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pGuidToKeyOffsetMap = pSysmanProductHelper->getGuidToKeyOffsetMap();
    EXPECT_NE(nullptr, pGuidToKeyOffsetMap);
    EXPECT_EQ(mockDg2GuidToKeyOffsetMap.at("0x4f95").at("PACKAGE_ENERGY"), (*pGuidToKeyOffsetMap).at("0x4f95").at("PACKAGE_ENERGY"));
    EXPECT_EQ(mockDg2GuidToKeyOffsetMap.at("0x4f95").at("SOC_TEMPERATURES"), (*pGuidToKeyOffsetMap).at("0x4f95").at("SOC_TEMPERATURES"));
}

HWTEST2_F(SysmanProductHelperPmtTest, GivenSysmanProductHelperInstanceWhenGetGuidToKeyOffsetMapIsCalledThenValidMapIsReturned, IsPVC) {
    const std::map<std::string, std::map<std::string, uint64_t>> mockPvcGuidToKeyOffsetMap = {{"0xb15a0edc",
                                                                                               {{"HBM0MaxDeviceTemperature", 28},
                                                                                                {"HBM1MaxDeviceTemperature", 36},
                                                                                                {"TileMinTemperature", 40},
                                                                                                {"TileMaxTemperature", 44}}}};

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pGuidToKeyOffsetMap = pSysmanProductHelper->getGuidToKeyOffsetMap();
    EXPECT_NE(nullptr, pGuidToKeyOffsetMap);
    EXPECT_EQ(mockPvcGuidToKeyOffsetMap.at("0xb15a0edc").at("HBM0MaxDeviceTemperature"), (*pGuidToKeyOffsetMap).at("0xb15a0edc").at("HBM0MaxDeviceTemperature"));
    EXPECT_EQ(mockPvcGuidToKeyOffsetMap.at("0xb15a0edc").at("HBM1MaxDeviceTemperature"), (*pGuidToKeyOffsetMap).at("0xb15a0edc").at("HBM1MaxDeviceTemperature"));
    EXPECT_EQ(mockPvcGuidToKeyOffsetMap.at("0xb15a0edc").at("TileMinTemperature"), (*pGuidToKeyOffsetMap).at("0xb15a0edc").at("TileMinTemperature"));
    EXPECT_EQ(mockPvcGuidToKeyOffsetMap.at("0xb15a0edc").at("TileMaxTemperature"), (*pGuidToKeyOffsetMap).at("0xb15a0edc").at("TileMaxTemperature"));
}

HWTEST2_F(SysmanProductHelperPmtTest, GivenSysmanProductHelperInstanceWhenGetGuidToKeyOffsetMapIsCalledThenValidMapIsReturned, IsBMG) {
    const std::map<std::string, std::map<std::string, uint64_t>> mockBmgGuidToKeyOffsetMap = {{"0x5e2f8210",
                                                                                               {{"reg_PCIESS_rx_bytecount_lsb", 280},
                                                                                                {"reg_PCIESS_tx_bytecount_msb", 284}}}};

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    auto pGuidToKeyOffsetMap = pSysmanProductHelper->getGuidToKeyOffsetMap();
    EXPECT_NE(nullptr, pGuidToKeyOffsetMap);
    EXPECT_EQ(mockBmgGuidToKeyOffsetMap.at("0x5e2f8210").at("reg_PCIESS_rx_bytecount_lsb"), (*pGuidToKeyOffsetMap).at("0x5e2f8210").at("reg_PCIESS_rx_bytecount_lsb"));
    EXPECT_EQ(mockBmgGuidToKeyOffsetMap.at("0x5e2f8210").at("reg_PCIESS_tx_bytecount_msb"), (*pGuidToKeyOffsetMap).at("0x5e2f8210").at("reg_PCIESS_tx_bytecount_msb"));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
