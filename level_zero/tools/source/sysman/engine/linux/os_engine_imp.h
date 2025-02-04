/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "level_zero/tools/source/sysman/engine/os_engine.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/sysman_const.h"

#include <unistd.h>

namespace L0 {
class PmuInterface;
struct Device;
class LinuxEngineImp : public OsEngine, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getActivity(zes_engine_stats_t *pStats) override;
    ze_result_t getActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) override;
    ze_result_t getProperties(zes_engine_properties_t &properties) override;
    bool isEngineModuleSupported() override;
    static zes_engine_group_t getGroupFromEngineType(zes_engine_group_t type);
    LinuxEngineImp() = default;
    LinuxEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubDevice);
    ~LinuxEngineImp() override;

  protected:
    zes_engine_group_t engineGroup = ZES_ENGINE_GROUP_ALL;
    uint32_t engineInstance = 0;
    PmuInterface *pPmuInterface = nullptr;
    NEO::Drm *pDrm = nullptr;
    Device *pDevice = nullptr;
    uint32_t subDeviceId = 0;
    ze_bool_t onSubDevice = false;
    uint32_t numberOfVfs = 0;
    SysfsAccess *pSysfsAccess = nullptr;

  private:
    void init();
    std::vector<std::pair<int64_t, int64_t>> fdList{};
};

} // namespace L0
