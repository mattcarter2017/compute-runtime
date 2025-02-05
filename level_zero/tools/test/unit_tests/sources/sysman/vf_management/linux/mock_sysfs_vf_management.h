/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/linux/pmu/pmu_imp.h"
#include "level_zero/tools/source/sysman/vf_management/linux/os_vf_imp.h"

namespace L0 {
namespace ult {

const std::string fileForNumberOfVfs = "device/sriov_numvfs";
const std::string fileForLmemUsed = "iov/vf1/telemetry/lmem_alloc_size";
const std::string fileForLmemQuota = "iov/vf1/gt/lmem_quota";
const std::string pathForVfBdf = "device/virtfn0";
const std::string mockBdfdata = "pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:01.0/0000:4d:00.1";
const std::string mockInvalidBdfdata = "pci0000:4a";
const std::string mockPathWithInvalidTokens = "pci0000:4a/0000:4a";
const int64_t mockPmuFd = 10;
const uint64_t mockActiveTime = 987654321;
const uint64_t mockTimestamp = 87654321;
const uint64_t mockLmemUsed = 65536;
const uint64_t mockLmemQuota = 128000;
const uint32_t mockNumberOfVfs = 1;
const uint32_t numberMockedEngines = 2;

struct MockVfPmuInterfaceImp : public PmuInterfaceImp {
    using PmuInterfaceImp::perfEventOpen;
    MockVfPmuInterfaceImp(LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}
    ~MockVfPmuInterfaceImp() override = default;

    bool mockPmuReadFail = false;
    bool mockPerfEventOpenReadFail = false;
    int32_t mockErrorNumber = -ENOSPC;
    int32_t mockPerfEventOpenFailAtCount = 1;

    int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) override {

        mockPerfEventOpenFailAtCount = std::max<int32_t>(mockPerfEventOpenFailAtCount - 1, 1);
        const bool shouldCheckForError = (mockPerfEventOpenFailAtCount == 1);
        if (shouldCheckForError && mockPerfEventOpenReadFail == true) {
            errno = mockErrorNumber;
            return -1;
        }
        return mockPmuFd;
    }

    int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override {

        if (mockPmuReadFail == true) {
            return -1;
        }

        data[0] = mockActiveTime;
        data[1] = mockTimestamp;
        data[2] = mockActiveTime;
        data[3] = mockTimestamp;
        return 0;
    }
};

struct MockVfNeoDrm : public Drm {
    using Drm::engineInfo;
    using Drm::setupIoctlHelper;
    const int mockFd = 0;
    MockVfNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}
    ~MockVfNeoDrm() override = default;

    bool mockReadSysmanQueryEngineInfo = true;

    bool sysmanQueryEngineInfo() override {

        if (mockReadSysmanQueryEngineInfo == false) {
            return false;
        }

        std::vector<NEO::EngineCapabilities> i915QueryEngineInfo(numberMockedEngines);
        i915QueryEngineInfo[0].engine.engineClass = prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE;
        i915QueryEngineInfo[0].engine.engineInstance = 0;
        i915QueryEngineInfo[1].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY;
        i915QueryEngineInfo[1].engine.engineInstance = 0;

        StackVec<std::vector<NEO::EngineCapabilities>, 2> engineInfos{i915QueryEngineInfo};

        this->engineInfo.reset(new EngineInfo(this, engineInfos));
        return true;
    }
};

struct MockVfSysfsAccess : public L0::SysfsAccess {
    ze_result_t mockError = ZE_RESULT_SUCCESS;
    ze_result_t mockRealPathError = ZE_RESULT_SUCCESS;
    bool mockValidBdfData = true;
    bool mockInvalidTokens = true;
    bool mockLmemValue = true;
    bool mockLmemQuotaValue = true;

    ze_result_t read(const std::string file, uint32_t &val) override {
        if (mockError != ZE_RESULT_SUCCESS) {
            return mockError;
        }

        if (file.compare(fileForNumberOfVfs) == 0) {
            val = mockNumberOfVfs;
            return ZE_RESULT_SUCCESS;
        }

        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t read(const std::string file, uint64_t &val) override {
        return getVal(file, val);
    }

    ze_result_t getVal(const std::string file, uint64_t &val) {
        if (mockError != ZE_RESULT_SUCCESS) {
            return mockError;
        }

        if (file.compare(fileForLmemUsed) == 0) {
            if (mockLmemValue) {
                val = mockLmemUsed;
            } else {
                val = 0;
            }
            return ZE_RESULT_SUCCESS;
        }

        if (file.compare(fileForLmemQuota) == 0) {
            if (mockLmemQuotaValue) {
                val = mockLmemQuota;
            } else {
                val = 0;
            }
            return ZE_RESULT_SUCCESS;
        }

        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t getRealPath(const std::string path, std::string &buf) override {
        if (mockRealPathError != ZE_RESULT_SUCCESS) {
            return mockRealPathError;
        }
        if (path.compare(pathForVfBdf) == 0) {
            if (mockValidBdfData) {
                buf = mockBdfdata;
            } else if (mockInvalidTokens) {
                buf = mockPathWithInvalidTokens;
            } else {
                buf = mockInvalidBdfdata;
            }
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    MockVfSysfsAccess() = default;
    ~MockVfSysfsAccess() override = default;
};

struct MockVfFsAccess : public FsAccess {

    bool mockReadFail = false;

    ze_result_t read(const std::string file, uint32_t &val) override {

        if (mockReadFail == true) {
            val = 0;
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        val = 23;
        return ZE_RESULT_SUCCESS;
    }

    MockVfFsAccess() = default;
    ~MockVfFsAccess() override = default;
};

class PublicLinuxVfImp : public L0::LinuxVfImp {
  public:
    PublicLinuxVfImp(L0::OsSysman *pOsSysman, uint32_t vfId) : L0::LinuxVfImp(pOsSysman, vfId) {}
    using L0::LinuxVfImp::pSysfsAccess;
};

} // namespace ult
} // namespace L0
