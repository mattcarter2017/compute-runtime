/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/firmware/linux/mock_zes_sysman_firmware.h"

namespace L0 {
namespace Sysman {
namespace ult {

class ZesSysmanFirmwareFixture : public SysmanDeviceFixture {

  protected:
    zes_firmware_handle_t hSysmanFirmware = {};
    std::unique_ptr<MockFirmwareInterface> pMockFwInterface;
    L0::Sysman::FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    std::unique_ptr<MockFirmwareFsAccess> pFsAccess;
    L0::Sysman::FsAccessInterface *pFsAccessOriginal = nullptr;
    L0::Sysman::SysFsAccessInterface *pSysFsAccessOriginal = nullptr;
    std::unique_ptr<MockFirmwareSysfsAccess> pMockSysfsAccess;
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockFirmwareFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pSysFsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pMockSysfsAccess = std::make_unique<MockFirmwareSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pMockSysfsAccess.get();

        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pMockFwInterface = std::make_unique<MockFirmwareInterface>();
        pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface.get();

        for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
        device = pSysmanDeviceImp;
    }
    void initFirmware() {
        uint32_t count = 0;
        ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
    void TearDown() override {
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysFsAccessOriginal;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_firmware_handle_t> getFirmwareHandles(uint32_t count) {
        std::vector<zes_firmware_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFirmwares(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

HWTEST2_F(ZesSysmanFirmwareFixture, GivenComponentCountZeroWhenCallingzesFirmwareGetThenZeroCountIsReturnedAndVerifyzesFirmwareGetCallSucceeds, IsXeHpcOrXeHpgCore) {
    std::vector<zes_firmware_handle_t> firmwareHandle{};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockFwHandlesCount);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumFirmwares(device->toHandle(), &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    firmwareHandle.resize(count);
    result = zesDeviceEnumFirmwares(device->toHandle(), &count, firmwareHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockFwHandlesCount);

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);
    result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockFwHandlesCount);

    testCount = count;

    firmwareHandle.resize(testCount);
    result = zesDeviceEnumFirmwares(device->toHandle(), &testCount, firmwareHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, firmwareHandle.data());
    EXPECT_EQ(testCount, mockFwHandlesCount);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesSysmanFirmwareFixture, GivenValidDeviceHandleAndFirmwareUnSupportedWhenCallingEnumerateThenVerifyFirmwareDomainsAreZero) {

    struct MockSysmanProductHelperFirmware : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperFirmware() = default;
        void getDeviceSupportedFwTypes(FirmwareUtil *pFwInterface, std::vector<std::string> &fwTypes) override {
            return;
        }
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperFirmware>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwarePropertiesThenVersionIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    ASSERT_NE(nullptr, handles[0]);
    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[0], &properties));
    EXPECT_STREQ("GFX", properties.name);
    EXPECT_STREQ(mockFwVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenGettingPscPropertiesThenVersionIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[2]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    ASSERT_NE(nullptr, handles[2]);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[2], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypes[2].c_str(), properties.name);
    EXPECT_STREQ(mockPscVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenNullDirEntriesWhenGettingPscPropertiesThenUnknownVersionIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[2]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    pMockSysfsAccess->isNullDirEntries = true;
    auto handles = getFirmwareHandles(mockFwHandlesCount);

    zes_firmware_properties_t properties = {};
    ASSERT_NE(nullptr, handles[2]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[2], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypes[2].c_str(), properties.name);
    EXPECT_STREQ(mockUnknownVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenFailedToReadPSCVersionFromSysfsThenUnknownVersionIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[2]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    pMockSysfsAccess->readResult = ZE_RESULT_ERROR_NOT_AVAILABLE;
    auto handles = getFirmwareHandles(mockFwHandlesCount);
    ASSERT_NE(nullptr, handles[2]);

    zes_firmware_properties_t properties = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[2], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypes[2].c_str(), properties.name);
    EXPECT_STREQ(mockUnknownVersion.c_str(), properties.version);

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenRepeatedFWTypesWhenInitializingFirmwareContextThenexpectNoHandles, IsXeHpcOrXeHpgCore) {
    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    pFsAccess->isReadFwTypes = false;

    pSysmanDeviceImp->pFirmwareHandleContext->init();

    EXPECT_EQ(1u, pSysmanDeviceImp->pFirmwareHandleContext->handleList.size());
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenFlashingGscFirmwareThenSuccessIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareFlash(handle, (void *)testImage, ZES_STRING_PROPERTY_SIZE));
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenFlashingPscFirmwareThenSuccessIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[1]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareFlash(handle, (void *)testImage, ZES_STRING_PROPERTY_SIZE));
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

TEST_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenFlashingUnkownFirmwareThenFailureIsReturned) {
    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockUnsupportedFwTypes[0]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    pMockFwInterface->flashFirmwareResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    uint8_t testImage[ZES_STRING_PROPERTY_SIZE] = {};
    memset(testImage, 0xA, ZES_STRING_PROPERTY_SIZE);
    auto handle = pSysmanDeviceImp->pFirmwareHandleContext->handleList[0]->toHandle();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFirmwareFlash(handle, (void *)testImage, ZES_STRING_PROPERTY_SIZE));

    pSysmanDeviceImp->pFirmwareHandleContext->handleList.pop_back();
    delete ptestFirmwareImp;
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleFirmwareLibraryCallFailureWhenGettingFirmwarePropertiesThenUnknownIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    pMockSysfsAccess->scanDirEntriesResult = ZE_RESULT_ERROR_UNKNOWN;
    pMockFwInterface->getFwVersionResult = ZE_RESULT_ERROR_UNINITIALIZED;

    pSysmanDeviceImp->pFirmwareHandleContext->init();
    auto handles = getFirmwareHandles(mockFwHandlesCount);

    zes_firmware_properties_t properties = {};
    ASSERT_NE(nullptr, handles[0]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[0], &properties));
    EXPECT_STREQ("GFX", properties.name);
    EXPECT_STREQ("unknown", properties.version);

    ASSERT_NE(nullptr, handles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[1], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypes[1].c_str(), properties.name);
    EXPECT_STREQ("unknown", properties.version);

    ASSERT_NE(nullptr, handles[2]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetProperties(handles[2], &properties));
    EXPECT_STREQ(mockSupportedFirmwareTypes[2].c_str(), properties.name);
    EXPECT_STREQ("unknown", properties.version);
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenNewFirmwareContextWithHandleSizeZeroWhenFirmwareEnumerateIsCalledThenSuccessResultIsReturned, IsXeHpcOrXeHpgCore) {
    uint32_t count = 0;
    L0::Sysman::OsSysman *pOsSysman = pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman;

    for (const auto &handle : pSysmanDeviceImp->pFirmwareHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.clear();
    delete pSysmanDeviceImp->pFirmwareHandleContext;

    pSysmanDeviceImp->pFirmwareHandleContext = new L0::Sysman::FirmwareHandleContext(pOsSysman);
    EXPECT_EQ(0u, pSysmanDeviceImp->pFirmwareHandleContext->handleList.size());
    ze_result_t result = zesDeviceEnumFirmwares(device->toHandle(), &count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(3u, count);
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwareFlashProgressThenSuccessIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[1]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    uint32_t completionPercent = 0;

    ASSERT_NE(nullptr, handles[1]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesFirmwareGetFlashProgress(handles[1], &completionPercent));
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwareSecurityVersionThenUnSupportedIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[1]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    ASSERT_NE(nullptr, handles[1]);
    char pVersion[100];
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFirmwareGetSecurityVersionExp(handles[1], pVersion));
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenSettingFirmwareSecurityVersionThenUnSupportedIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[1]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    ASSERT_NE(nullptr, handles[1]);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFirmwareSetSecurityVersionExp(handles[1]));
}

HWTEST2_F(ZesSysmanFirmwareFixture, GivenValidFirmwareHandleWhenGettingFirmwareConsoleLogsThenUnSupportedIsReturned, IsXeHpcOrXeHpgCore) {
    initFirmware();

    L0::Sysman::FirmwareImp *ptestFirmwareImp = new L0::Sysman::FirmwareImp(pSysmanDeviceImp->pFirmwareHandleContext->pOsSysman, mockSupportedFirmwareTypes[1]);
    pSysmanDeviceImp->pFirmwareHandleContext->handleList.push_back(ptestFirmwareImp);

    auto handles = getFirmwareHandles(mockFwHandlesCount);
    ASSERT_NE(nullptr, handles[1]);

    size_t pSize;
    char *pFirmwareLog = nullptr;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesFirmwareGetConsoleLogs(handles[1], &pSize, pFirmwareLog));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
