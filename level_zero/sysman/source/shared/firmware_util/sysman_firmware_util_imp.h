/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_library.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"

#include <cinttypes>
#include <mutex>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {
typedef int (*pIgscDeviceInitByDevice)(struct igsc_device_handle *handle,
                                       const char *devicePath);
typedef int (*pIgscDeviceGetDeviceInfo)(struct igsc_device_handle *handle,
                                        struct igsc_info_device *info);
typedef int (*pIgscDeviceFwVersion)(struct igsc_device_handle *handle,
                                    struct igsc_fw_version *version);
typedef int (*pIgscDeviceIteratorCreate)(struct igsc_device_iterator **iter);
typedef int (*pIgscDeviceIteratorNext)(struct igsc_device_iterator *iter,
                                       IgscDeviceInfo *info);
typedef void (*pIgscDeviceIteratorDestroy)(struct igsc_device_iterator *iter);
typedef int (*pIgscDeviceFwUpdate)(struct igsc_device_handle *handle,
                                   const uint8_t *buffer,
                                   const uint32_t bufferLen,
                                   igsc_progress_func_t progressFunc,
                                   void *ctx);
typedef int (*pIgscImageOpromInit)(struct igsc_oprom_image **img,
                                   const uint8_t *buffer,
                                   uint32_t bufferLen);
typedef int (*pIgscImageOpromType)(struct igsc_oprom_image *img,
                                   uint32_t *opromType);
typedef int (*pIgscDeviceOpromUpdate)(struct igsc_device_handle *handle,
                                      uint32_t opromType,
                                      struct igsc_oprom_image *img,
                                      igsc_progress_func_t progressFunc,
                                      void *ctx);
typedef int (*pIgscDeviceOpromVersion)(struct igsc_device_handle *handle,
                                       uint32_t opromType,
                                       struct igsc_oprom_version *version);

typedef int (*pIgscDeviceClose)(struct igsc_device_handle *handle);
typedef int (*pIgscIfrGetStatusExt)(struct igsc_device_handle *handle,
                                    uint32_t *supportedTests,
                                    uint32_t *hwCapabilities,
                                    uint32_t *ifrApplied,
                                    uint32_t *prevErrors,
                                    uint32_t *pendingReset);
typedef int (*pIgscIafPscUpdate)(struct igsc_device_handle *handle,
                                 const uint8_t *buffer,
                                 const uint32_t bufferLen,
                                 igsc_progress_func_t progressFunc,
                                 void *ctx);
typedef int (*pIgscGfspMemoryErrors)(struct igsc_device_handle *handle,
                                     struct igsc_gfsp_mem_err *tiles);

typedef int (*pIgscGfspCountTiles)(struct igsc_device_handle *handle,
                                   uint32_t *numOfTiles);

typedef int (*pIgscGfspGetHealthIndicator)(struct igsc_device_handle *handle,
                                           uint8_t *healthIndicator);

typedef int (*pIgscIfrRunMemPPRTest)(struct igsc_device_handle *handle,
                                     uint32_t *status,
                                     uint32_t *pendingReset,
                                     uint32_t *errorCode);

typedef int (*pIgscGetEccConfig)(struct igsc_device_handle *handle,
                                 uint8_t *curEccState,
                                 uint8_t *penEccState);

typedef int (*pIgscSetEccConfig)(struct igsc_device_handle *handle,
                                 uint8_t reqEccState,
                                 uint8_t *curEccState,
                                 uint8_t *penEccState);

extern pIgscDeviceInitByDevice deviceInitByDevice;
extern pIgscDeviceGetDeviceInfo deviceGetDeviceInfo;
extern pIgscDeviceFwVersion deviceGetFwVersion;
extern pIgscDeviceIteratorCreate deviceIteratorCreate;
extern pIgscDeviceIteratorNext deviceItreatorNext;
extern pIgscDeviceIteratorDestroy deviceItreatorDestroy;
extern pIgscDeviceFwUpdate deviceFwUpdate;
extern pIgscImageOpromInit imageOpromInit;
extern pIgscImageOpromType imageOpromType;
extern pIgscDeviceOpromUpdate deviceOpromUpdate;
extern pIgscDeviceOpromVersion deviceOpromVersion;
extern pIgscDeviceClose deviceClose;
extern pIgscIfrGetStatusExt deviceIfrGetStatusExt;
extern pIgscIafPscUpdate iafPscUpdate;
extern pIgscGfspMemoryErrors gfspMemoryErrors;
extern pIgscGfspCountTiles gfspCountTiles;
extern pIgscIfrRunMemPPRTest deviceIfrRunMemPPRTest;
extern pIgscGetEccConfig getEccConfig;
extern pIgscSetEccConfig setEccConfig;

extern void firmwareFlashProgressFunc(uint32_t done, uint32_t total, void *ctx);

typedef struct {
    uint32_t completionPercent;
    std::mutex fwProgressLock;
} FlashProgressInfo;

class FirmwareUtilImp : public FirmwareUtil, NEO::NonCopyableOrMovableClass {
  public:
    FirmwareUtilImp(uint16_t domain, uint8_t bus, uint8_t device, uint8_t function);
    ~FirmwareUtilImp() override;
    ze_result_t fwDeviceInit() override;
    ze_result_t getFwVersion(std::string fwType, std::string &firmwareVersion) override;
    ze_result_t flashFirmware(std::string fwType, void *pImage, uint32_t size) override;
    ze_result_t getFlashFirmwareProgress(uint32_t *pCompletionPercent) override;
    ze_result_t fwIfrApplied(bool &ifrStatus) override;
    ze_result_t fwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) override;
    ze_result_t fwRunDiagTests(std::string &osDiagType, zes_diag_result_t *pDiagResult) override;
    ze_result_t fwGetMemoryErrorCount(zes_ras_error_type_t type, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count) override;
    ze_result_t fwGetEccConfig(uint8_t *currentState, uint8_t *pendingState) override;
    ze_result_t fwSetEccConfig(uint8_t newState, uint8_t *currentState, uint8_t *pendingState) override;
    void getDeviceSupportedFwTypes(std::vector<std::string> &fwTypes) override;
    void fwGetMemoryHealthIndicator(zes_mem_health_t *health) override;

    static int fwUtilLoadFlags;
    static std::string fwUtilLibraryName;
    bool loadEntryPoints();
    bool loadEntryPointsExt();
    void updateFirmwareFlashProgress(uint32_t percent);

    NEO::OsLibrary *libraryHandle = nullptr;

  protected:
    ze_result_t getFirstDevice(IgscDeviceInfo *);
    ze_result_t fwGetVersion(std::string &fwVersion);
    ze_result_t opromGetVersion(std::string &fwVersion);
    ze_result_t pscGetVersion(std::string &fwVersion);
    ze_result_t fwFlashGSC(void *pImage, uint32_t size);
    ze_result_t fwFlashOprom(void *pImage, uint32_t size);
    ze_result_t fwFlashIafPsc(void *pImage, uint32_t size);
    ze_result_t fwCallGetstatusExt(uint32_t &supportedTests, uint32_t &ifrApplied, uint32_t &prevErrors, uint32_t &pendingReset);

    std::string fwDevicePath{};
    struct igsc_device_handle fwDeviceHandle = {};
    template <class T>
    bool getSymbolAddr(const std::string name, T &proc) {
        void *addr = libraryHandle->getProcAddress(name);
        proc = reinterpret_cast<T>(addr);
        return nullptr != proc;
    }

  private:
    uint16_t domain = 0;
    uint8_t bus = 0;
    uint8_t device = 0;
    uint8_t function = 0;
    std::mutex fwLock;
    FlashProgressInfo flashProgress{};
};
} // namespace Sysman
} // namespace L0
