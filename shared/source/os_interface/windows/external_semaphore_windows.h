/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"

#include <limits>
#include <memory>
#include <string>

namespace NEO {

typedef wchar_t SharedSyncName[9 + 2 * (sizeof(uint32_t) + sizeof(uint64_t))];

struct SharedMemoryContentHeader {
    alignas(8) uint64_t lastSignaledValue;
    SharedSyncName sharedSyncName;
    uint32_t access;
    uint32_t serializedSecurityDescriptorStringSize;
};

class ExternalSemaphoreWindows : public ExternalSemaphore {
  public:
    static std::unique_ptr<ExternalSemaphoreWindows> create(OSInterface *osInterface);

    ~ExternalSemaphoreWindows() override{};

    bool importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) override;

    bool enqueueWait(uint64_t *fenceValue) override;
    bool enqueueSignal(uint64_t *fenceValue) override;

    SharedMemoryContentHeader *getSharedMemoryContentHeader() {
        return reinterpret_cast<SharedMemoryContentHeader *>(this->pCpuAddress);
    }

  protected:
    D3DKMT_HANDLE syncHandle;
    void *pCpuAddress = nullptr;
    volatile uint64_t *pLastSignaledValue = nullptr;
};

} // namespace NEO
