/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/memory/windows/sysman_os_memory_imp.h"

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/windows/sysman_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {

template <typename I>
std::string intToHex(I w, size_t hexLength = sizeof(I) << 1) {
    static const char *digits = "0123456789ABCDEF";
    std::string retString(hexLength, '0');
    constexpr uint32_t intSize = sizeof(uint32_t);
    for (size_t i = 0, j = (hexLength - 1) * intSize; i < hexLength; ++i, j -= intSize)
        retString[i] = digits[(w >> j) & 0x0f];
    return (std::string("0x") + retString);
}

std::wstring toWString(std::string str) {
    std::wstring wsTmp(str.begin(), str.end());
    return wsTmp;
}

std::wstring constructCounterStr(std::wstring object, std::wstring counter, LUID luid, uint32_t index) {
    std::wstring fstr = L"\\";
    fstr += object;
    fstr += L"(luid_";
    fstr += toWString(intToHex((long)luid.HighPart));
    fstr += L"_";
    fstr += toWString(intToHex((unsigned long)luid.LowPart));
    fstr += L"_phys_";
    fstr += std::to_wstring(index);
    fstr += L")\\";
    fstr += counter;
    return fstr;
}

std::unique_ptr<OsMemory> OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<WddmMemoryImp> pWddmMemoryImp = std::make_unique<WddmMemoryImp>(pOsSysman, onSubdevice, subdeviceId);
    return pWddmMemoryImp;
}

bool WddmMemoryImp::isMemoryModuleSupported() {
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::MemoryComponent;
    request.requestId = KmdSysman::Requests::Memory::NumMemoryDomains;

    if (pKmdSysManager->requestSingle(request, response) != ZE_RESULT_SUCCESS) {
        return false;
    }

    memcpy_s(&value, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    return (value > 0);
}

ze_result_t WddmMemoryImp::getProperties(zes_mem_properties_t *pProperties) {
    uint32_t retValu32 = 0;
    uint64_t retValu64 = 0;
    std::vector<KmdSysman::RequestProperty> vRequests = {};
    std::vector<KmdSysman::ResponseProperty> vResponses = {};
    KmdSysman::RequestProperty request = {};

    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subdeviceId;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::MemoryComponent;

    request.requestId = KmdSysman::Requests::Memory::MemoryType;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Memory::PhysicalSize;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Memory::NumChannels;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Memory::MemoryLocation;
    vRequests.push_back(request);

    request.requestId = KmdSysman::Requests::Memory::MemoryWidth;
    vRequests.push_back(request);

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);

    if ((status != ZE_RESULT_SUCCESS) || (vResponses.size() != vRequests.size())) {
        return status;
    }

    pProperties->type = ZES_MEM_TYPE_FORCE_UINT32;
    if (vResponses[0].returnCode == KmdSysman::Success) {
        memcpy_s(&retValu32, sizeof(uint32_t), vResponses[0].dataBuffer, sizeof(uint32_t));
        switch (retValu32) {
        case KmdSysman::MemoryType::DDR4: {
            pProperties->type = ZES_MEM_TYPE_DDR4;
        } break;
        case KmdSysman::MemoryType::DDR5: {
            pProperties->type = ZES_MEM_TYPE_DDR5;
        } break;
        case KmdSysman::MemoryType::LPDDR5: {
            pProperties->type = ZES_MEM_TYPE_LPDDR5;
        } break;
        case KmdSysman::MemoryType::LPDDR4: {
            pProperties->type = ZES_MEM_TYPE_LPDDR4;
        } break;
        case KmdSysman::MemoryType::DDR3: {
            pProperties->type = ZES_MEM_TYPE_DDR3;
        } break;
        case KmdSysman::MemoryType::LPDDR3: {
            pProperties->type = ZES_MEM_TYPE_LPDDR3;
        } break;
        case KmdSysman::MemoryType::GDDR4: {
            pProperties->type = ZES_MEM_TYPE_GDDR4;
        } break;
        case KmdSysman::MemoryType::GDDR5: {
            pProperties->type = ZES_MEM_TYPE_GDDR5;
        } break;
        case KmdSysman::MemoryType::GDDR5X: {
            pProperties->type = ZES_MEM_TYPE_GDDR5X;
        } break;
        case KmdSysman::MemoryType::GDDR6: {
            pProperties->type = ZES_MEM_TYPE_GDDR6;
        } break;
        case KmdSysman::MemoryType::GDDR6X: {
            pProperties->type = ZES_MEM_TYPE_GDDR6X;
        } break;
        case KmdSysman::MemoryType::GDDR7: {
            pProperties->type = ZES_MEM_TYPE_GDDR7;
        } break;
        default: {
            pProperties->type = ZES_MEM_TYPE_FORCE_UINT32;
        } break;
        }
    }

    pProperties->physicalSize = 0;
    if (vResponses[1].returnCode == KmdSysman::Success) {
        memcpy_s(&retValu64, sizeof(uint64_t), vResponses[1].dataBuffer, sizeof(uint64_t));
        pProperties->physicalSize = retValu64;
    }

    pProperties->numChannels = -1;
    if (vResponses[2].returnCode == KmdSysman::Success) {
        memcpy_s(&retValu32, sizeof(uint32_t), vResponses[2].dataBuffer, sizeof(uint32_t));
        pProperties->numChannels = retValu32;
    }

    pProperties->location = ZES_MEM_LOC_FORCE_UINT32;
    if (vResponses[3].returnCode == KmdSysman::Success) {
        memcpy_s(&retValu32, sizeof(uint32_t), vResponses[3].dataBuffer, sizeof(uint32_t));
        pProperties->location = static_cast<zes_mem_loc_t>(retValu32);
    }

    pProperties->busWidth = -1;
    if (vResponses[4].returnCode == KmdSysman::Success) {
        memcpy_s(&retValu32, sizeof(uint32_t), vResponses[4].dataBuffer, sizeof(uint32_t));
        pProperties->busWidth = retValu32;
    }

    pProperties->subdeviceId = 0;
    pProperties->onSubdevice = false;

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmMemoryImp::getBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    auto pSysmanProductHelper = pWddmSysmanImp->getSysmanProductHelper();
    return pSysmanProductHelper->getMemoryBandWidth(pBandwidth, pWddmSysmanImp);
}

ze_result_t WddmMemoryImp::getState(zes_mem_state_t *pState) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint64_t retValu64 = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    pState->health = ZES_MEM_HEALTH_OK;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::MemoryComponent;
    request.requestId = KmdSysman::Requests::Memory::CurrentTotalAllocableMem;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&retValu64, sizeof(uint64_t), response.dataBuffer, sizeof(uint64_t));
    pState->size = retValu64;

    if (!pdhInitialized) {
        if (pdhOpenQuery && pdhOpenQuery(NULL, NULL, &gpuQuery) == ERROR_SUCCESS) {
            pdhInitialized = true;
        }
    }

    if (!pdhCounterAdded && pdhAddEnglishCounterW && pKmdSysManager->GetWddmAccess()) {
        std::wstring counterStr = constructCounterStr(L"GPU Adapter Memory", L"Dedicated Usage", pKmdSysManager->GetWddmAccess()->getAdapterLuid(), 0);
        pdhCounterAdded = (pdhAddEnglishCounterW(gpuQuery, counterStr.c_str(), NULL, &dedicatedUsage) == ERROR_SUCCESS);
    }

    if (pdhCounterAdded && pdhCollectQueryData && pdhGetFormattedCounterValue) {
        PDH_FMT_COUNTERVALUE counterVal;
        pdhCollectQueryData(gpuQuery);
        pdhGetFormattedCounterValue(dedicatedUsage, PDH_FMT_LARGE, NULL, &counterVal);
        retValu64 = counterVal.largeValue;
        pState->free = pState->size - retValu64;
    }

    return ZE_RESULT_SUCCESS;
}

WddmMemoryImp::WddmMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();

    hGetProcPDH = LoadLibrary(L"C:\\Windows\\System32\\pdh.dll");

    if (hGetProcPDH) {
        pdhOpenQuery = reinterpret_cast<fn_PdhOpenQueryW>(GetProcAddress(hGetProcPDH, "PdhOpenQueryW"));
        pdhAddEnglishCounterW = reinterpret_cast<fn_PdhAddEnglishCounterW>(GetProcAddress(hGetProcPDH, "PdhAddEnglishCounterW"));
        pdhCollectQueryData = reinterpret_cast<fn_PdhCollectQueryData>(GetProcAddress(hGetProcPDH, "PdhCollectQueryData"));
        pdhGetFormattedCounterValue = reinterpret_cast<fn_PdhGetFormattedCounterValue>(GetProcAddress(hGetProcPDH, "PdhGetFormattedCounterValue"));
        pdhCloseQuery = reinterpret_cast<fn_PdhCloseQuery>(GetProcAddress(hGetProcPDH, "PdhCloseQuery"));
    }
}

WddmMemoryImp::~WddmMemoryImp() {
    if (pdhInitialized && pdhCloseQuery) {
        pdhCloseQuery(gpuQuery);
    }
    if (hGetProcPDH) {
        FreeLibrary(hGetProcPDH);
    }
}

} // namespace Sysman
} // namespace L0
