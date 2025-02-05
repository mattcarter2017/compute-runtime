/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/api/pci/sysman_pci_imp.h"
#include "level_zero/sysman/source/api/pci/windows/sysman_os_pci_imp.h"
#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint64_t mockRxCounter = 24200000u;
constexpr uint64_t mockTxCounter = 231000000u;
constexpr uint64_t mockRxPacketCounter = 300000u;
constexpr uint64_t mockTxPacketCounter = 200000u;
constexpr uint64_t mockTimestamp = 120000u;

struct PciKmdSysManager : public MockKmdSysManager {
    // PciCurrentDevice, PciParentDevice, PciRootPort
    uint32_t mockDomain[3] = {0, 0, 0};
    uint32_t mockBus[3] = {0, 0, 3};
    uint32_t mockDevice[3] = {2, 0, 0};
    uint32_t mockFunction[3] = {0, 0, 0};
    uint32_t mockMaxLinkSpeed[3] = {1, 0, 4};
    uint32_t mockMaxLinkWidth[3] = {1, 0, 8};
    uint32_t mockCurrentLinkSpeed[3] = {1, 0, 3};
    uint32_t mockCurrentLinkWidth[3] = {1, 0, 1};
    int64_t mockCurrentMaxBandwidth[3] = {250000000, -1, 984615384};
    uint32_t mockResizableBarSupported[3] = {1, 1, 1};
    uint32_t mockResizableBarEnabled[3] = {1, 1, 1};
    uint32_t pciBusReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciDomainReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciDeviceReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciFunctionReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciMaxLinkSpeedReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciMaxLinkWidthReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciCurrentLinkSpeedReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciCurrentLinkWidthReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciResizableBarSupportedReturnCode = KmdSysman::KmdSysmanSuccess;
    uint32_t pciResizableBarEnabledReturnCode = KmdSysman::KmdSysmanSuccess;

    void getPciProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        uint8_t *pBuffer = reinterpret_cast<uint8_t *>(pResponse);
        pBuffer += sizeof(KmdSysman::GfxSysmanReqHeaderOut);

        KmdSysman::PciDomainsType domain = static_cast<KmdSysman::PciDomainsType>(pRequest->inCommandParam);

        switch (pRequest->inRequestId) {
        case KmdSysman::Requests::Pci::Domain: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockDomain[domain];
            pResponse->outReturnCode = pciDomainReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::Bus: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockBus[domain];
            pResponse->outReturnCode = pciBusReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::Device: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockDevice[domain];
            pResponse->outReturnCode = pciDeviceReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::Function: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockFunction[domain];
            pResponse->outReturnCode = pciFunctionReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::MaxLinkSpeed: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxLinkSpeed[domain];
            pResponse->outReturnCode = pciMaxLinkSpeedReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::MaxLinkWidth: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockMaxLinkWidth[domain];
            pResponse->outReturnCode = pciMaxLinkWidthReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::CurrentLinkSpeed: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockCurrentLinkSpeed[domain];
            pResponse->outReturnCode = pciCurrentLinkSpeedReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::CurrentLinkWidth: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockCurrentLinkWidth[domain];
            pResponse->outReturnCode = pciCurrentLinkWidthReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::ResizableBarSupported: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockResizableBarSupported[domain];
            pResponse->outReturnCode = pciResizableBarSupportedReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        case KmdSysman::Requests::Pci::ResizableBarEnabled: {
            uint32_t *pValue = reinterpret_cast<uint32_t *>(pBuffer);
            *pValue = mockResizableBarEnabled[domain];
            pResponse->outReturnCode = pciResizableBarEnabledReturnCode;
            pResponse->outDataSize = sizeof(uint32_t);
        } break;
        default: {
            pResponse->outDataSize = 0;
            pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
        } break;
        }
    }

    void setPciProperty(KmdSysman::GfxSysmanReqHeaderIn *pRequest, KmdSysman::GfxSysmanReqHeaderOut *pResponse) override {
        pResponse->outDataSize = 0;
        pResponse->outReturnCode = KmdSysman::KmdSysmanFail;
    }
};

class PublicPlatformMonitoringTech : public L0::Sysman::PlatformMonitoringTech {
  public:
    PublicPlatformMonitoringTech(std::vector<wchar_t> deviceInterfaceList, SysmanProductHelper *pSysmanProductHelper) : PlatformMonitoringTech(deviceInterfaceList, pSysmanProductHelper) {}
    using PlatformMonitoringTech::keyOffsetMap;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
