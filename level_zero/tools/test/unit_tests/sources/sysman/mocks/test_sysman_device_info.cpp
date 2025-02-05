/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/mocks/mock_sysman_device_info.h"

namespace L0 {
namespace ult {

TEST_F(SysmanMultiDeviceInfoFixture, givenDeviceWithMultipleTilesWhenOnlyTileOneIsEnabledThenGetSysmanDeviceInfoReturnsExpectedValues) {
    neoDevice->deviceBitfield.reset();
    neoDevice->deviceBitfield.set(1);
    uint32_t subdeviceId = 0;
    ze_bool_t onSubdevice = false;
    SysmanDeviceImp::getSysmanDeviceInfo(device->toHandle(), subdeviceId, onSubdevice, true);
    EXPECT_EQ(subdeviceId, 1u);
    EXPECT_TRUE(onSubdevice);
}

TEST_F(SysmanMultiDeviceInfoFixture, givenDeviceWithMultipleTilesWhenOnlyTileOneIsEnabledAndMultiArchIsDisabledThenGetSysmanDeviceInfoReturnsExpectedValues) {
    neoDevice->deviceBitfield.reset();
    neoDevice->deviceBitfield.set(1);
    uint32_t subdeviceId = 0;
    ze_bool_t onSubdevice = false;
    SysmanDeviceImp::getSysmanDeviceInfo(device->toHandle(), subdeviceId, onSubdevice, false);
    EXPECT_EQ(subdeviceId, 1u);
    EXPECT_FALSE(onSubdevice);
}

TEST_F(SysmanMultiDeviceInfoFixture, givenDeviceWithMultipleTilesEnabledAndCompositeHierarchyThenGetSysmanDeviceInfoReturnsExpectedValues) {
    neoDevice->getExecutionEnvironment()->setDeviceHierarchyMode(NEO::DeviceHierarchyMode::composite);
    uint32_t subDeviceCount = 0;
    std::vector<ze_device_handle_t> deviceHandles;
    Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
    if (subDeviceCount == 0) {
        deviceHandles.resize(1, device->toHandle());
    } else {
        deviceHandles.resize(subDeviceCount, nullptr);
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
    }
    for (auto &device : deviceHandles) {
        NEO::Device *neoDevice = Device::fromHandle(device)->getNEODevice();
        uint32_t subdeviceId = 0;
        ze_bool_t onSubdevice = false;
        SysmanDeviceImp::getSysmanDeviceInfo(device, subdeviceId, onSubdevice, false);
        EXPECT_EQ(subdeviceId, static_cast<NEO::SubDevice *>(neoDevice)->getSubDeviceIndex());
        EXPECT_TRUE(onSubdevice);
    }
}

} // namespace ult
} // namespace L0