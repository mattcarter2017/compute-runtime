/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include "igfxfmid.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct drm_i915_query_item;
struct drm_i915_gem_context_create_ext;
struct drm_i915_gem_execbuffer2;

namespace NEO {
class Drm;
class IoctlHelper;
enum class CacheRegion : uint16_t;
struct HardwareInfo;

extern IoctlHelper *ioctlHelperFactory[IGFX_MAX_PRODUCT];

struct MemoryClassInstance {
    uint16_t memoryClass;
    uint16_t memoryInstance;
};

struct EngineClassInstance {
    uint16_t engineClass;
    uint16_t engineInstance;
};

struct MemoryRegion {
    MemoryClassInstance region;
    uint64_t probedSize;
    uint64_t unallocatedSize;
};

struct EngineCapabilities {
    EngineClassInstance engine;
    uint64_t capabilities;
};

struct DistanceInfo {
    MemoryClassInstance region;
    EngineClassInstance engine;
    int32_t distance;
};

using MemRegionsVec = StackVec<MemoryClassInstance, 5>;

class IoctlHelper {
  public:
    virtual ~IoctlHelper() {}
    static IoctlHelper *get(const PRODUCT_FAMILY productFamily, const std::string &prelimVersion);
    static uint32_t ioctl(Drm *drm, unsigned long request, void *arg);
    virtual std::string getIoctlString(unsigned long request);
    virtual std::string getIoctlParamString(int param);
    virtual IoctlHelper *clone() = 0;

    virtual bool isVmBindAvailable(Drm *drm) = 0;
    virtual uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle) = 0;
    virtual std::vector<MemoryRegion> translateToMemoryRegions(const std::vector<uint8_t> &regionInfo) = 0;
    virtual CacheRegion closAlloc(Drm *drm) = 0;
    virtual uint16_t closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) = 0;
    virtual CacheRegion closFree(Drm *drm, CacheRegion closIndex) = 0;
    virtual int waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                              uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) = 0;
    virtual uint32_t getHwConfigIoctlVal() = 0;
    virtual uint32_t getAtomicAdvise(bool isNonAtomic) = 0;
    virtual uint32_t getPreferredLocationAdvise() = 0;
    virtual bool setVmBoAdvise(Drm *drm, int32_t handle, uint32_t attribute, void *region) = 0;
    virtual uint32_t getDirectSubmissionFlag() = 0;
    virtual int32_t getMemRegionsIoctlVal() = 0;
    virtual int32_t getEngineInfoIoctlVal() = 0;
    virtual uint32_t getComputeSlicesIoctlVal() = 0;
    virtual std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) = 0;
    virtual uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident) = 0;
    virtual std::vector<EngineCapabilities> translateToEngineCaps(const std::vector<uint8_t> &data) = 0;
    virtual uint32_t queryDistances(Drm *drm, std::vector<drm_i915_query_item> &queryItems, std::vector<DistanceInfo> &distanceInfos) = 0;
    virtual int32_t getComputeEngineClass() = 0;
    virtual uint16_t getWaitUserFenceSoftFlag() = 0;
    virtual int execBuffer(Drm *drm, drm_i915_gem_execbuffer2 *execBuffer, uint64_t completionGpuAddress, uint32_t counterValue) = 0;
    virtual bool completionFenceExtensionSupported(Drm &drm, const HardwareInfo &hwInfo) = 0;
    virtual std::optional<int> getHasPageFaultParamId() = 0;
    virtual uint32_t createContextWithAccessCounters(Drm *drm, drm_i915_gem_context_create_ext &gcc) = 0;
    virtual uint32_t createCooperativeContext(Drm *drm, drm_i915_gem_context_create_ext &gcc) = 0;
    virtual std::unique_ptr<uint8_t[]> createVmBindExtSetPat() = 0;
    virtual void fillVmBindExtSetPat(const std::unique_ptr<uint8_t[]> &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) = 0;
    virtual std::unique_ptr<uint8_t[]> createVmBindExtSyncFence() = 0;
    virtual void fillVmBindExtSyncFence(const std::unique_ptr<uint8_t[]> &vmBindExtSyncFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) = 0;
    virtual std::optional<uint64_t> getCopyClassSaturatePCIECapability() = 0;
    virtual std::optional<uint64_t> getCopyClassSaturateLinkCapability() = 0;
};

class IoctlHelperUpstream : public IoctlHelper {
  public:
    IoctlHelper *clone() override;

    bool isVmBindAvailable(Drm *drm) override;
    uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle) override;
    std::vector<MemoryRegion> translateToMemoryRegions(const std::vector<uint8_t> &regionInfo) override;
    CacheRegion closAlloc(Drm *drm) override;
    uint16_t closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override;
    CacheRegion closFree(Drm *drm, CacheRegion closIndex) override;
    int waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                      uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) override;
    uint32_t getHwConfigIoctlVal() override;
    uint32_t getAtomicAdvise(bool isNonAtomic) override;
    uint32_t getPreferredLocationAdvise() override;
    bool setVmBoAdvise(Drm *drm, int32_t handle, uint32_t attribute, void *region) override;
    uint32_t getDirectSubmissionFlag() override;
    int32_t getMemRegionsIoctlVal() override;
    int32_t getEngineInfoIoctlVal() override;
    uint32_t getComputeSlicesIoctlVal() override;
    std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) override;
    uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident) override;
    std::vector<EngineCapabilities> translateToEngineCaps(const std::vector<uint8_t> &data) override;
    uint32_t queryDistances(Drm *drm, std::vector<drm_i915_query_item> &queryItems, std::vector<DistanceInfo> &distanceInfos) override;
    int32_t getComputeEngineClass() override;
    uint16_t getWaitUserFenceSoftFlag() override;
    int execBuffer(Drm *drm, drm_i915_gem_execbuffer2 *execBuffer, uint64_t completionGpuAddress, uint32_t counterValue) override;
    bool completionFenceExtensionSupported(Drm &drm, const HardwareInfo &hwInfo) override;
    std::optional<int> getHasPageFaultParamId() override;
    uint32_t createContextWithAccessCounters(Drm *drm, drm_i915_gem_context_create_ext &gcc) override;
    uint32_t createCooperativeContext(Drm *drm, drm_i915_gem_context_create_ext &gcc) override;
    std::unique_ptr<uint8_t[]> createVmBindExtSetPat() override;
    void fillVmBindExtSetPat(const std::unique_ptr<uint8_t[]> &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) override;
    std::unique_ptr<uint8_t[]> createVmBindExtSyncFence() override;
    void fillVmBindExtSyncFence(const std::unique_ptr<uint8_t[]> &vmBindExtSyncFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) override;
    std::optional<uint64_t> getCopyClassSaturatePCIECapability() override;
    std::optional<uint64_t> getCopyClassSaturateLinkCapability() override;
};

template <PRODUCT_FAMILY gfxProduct>
class IoctlHelperImpl : public IoctlHelperUpstream {
  public:
    static IoctlHelper *get() {
        static IoctlHelperImpl<gfxProduct> instance;
        return &instance;
    }
    IoctlHelper *clone() override;

    uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle) override;
    std::vector<MemoryRegion> translateToMemoryRegions(const std::vector<uint8_t> &regionInfo) override;
};

class IoctlHelperPrelim20 : public IoctlHelper {
  public:
    std::string getIoctlString(unsigned long request) override;
    std::string getIoctlParamString(int param) override;
    IoctlHelper *clone() override;

    bool isVmBindAvailable(Drm *drm) override;
    uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle) override;
    std::vector<MemoryRegion> translateToMemoryRegions(const std::vector<uint8_t> &regionInfo) override;
    CacheRegion closAlloc(Drm *drm) override;
    uint16_t closAllocWays(Drm *drm, CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override;
    CacheRegion closFree(Drm *drm, CacheRegion closIndex) override;
    int waitUserFence(Drm *drm, uint32_t ctxId, uint64_t address,
                      uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) override;
    uint32_t getHwConfigIoctlVal() override;
    uint32_t getAtomicAdvise(bool isNonAtomic) override;
    uint32_t getPreferredLocationAdvise() override;
    bool setVmBoAdvise(Drm *drm, int32_t handle, uint32_t attribute, void *region) override;
    uint32_t getDirectSubmissionFlag() override;
    int32_t getMemRegionsIoctlVal() override;
    int32_t getEngineInfoIoctlVal() override;
    uint32_t getComputeSlicesIoctlVal() override;
    std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) override;
    uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident) override;
    std::vector<EngineCapabilities> translateToEngineCaps(const std::vector<uint8_t> &data) override;
    uint32_t queryDistances(Drm *drm, std::vector<drm_i915_query_item> &queryItems, std::vector<DistanceInfo> &distanceInfos) override;
    int32_t getComputeEngineClass() override;
    uint16_t getWaitUserFenceSoftFlag() override;
    int execBuffer(Drm *drm, drm_i915_gem_execbuffer2 *execBuffer, uint64_t completionGpuAddress, uint32_t counterValue) override;
    bool completionFenceExtensionSupported(Drm &drm, const HardwareInfo &hwInfo) override;
    std::optional<int> getHasPageFaultParamId() override;
    uint32_t createContextWithAccessCounters(Drm *drm, drm_i915_gem_context_create_ext &gcc) override;
    uint32_t createCooperativeContext(Drm *drm, drm_i915_gem_context_create_ext &gcc) override;
    std::unique_ptr<uint8_t[]> createVmBindExtSetPat() override;
    void fillVmBindExtSetPat(const std::unique_ptr<uint8_t[]> &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) override;
    std::unique_ptr<uint8_t[]> createVmBindExtSyncFence() override;
    void fillVmBindExtSyncFence(const std::unique_ptr<uint8_t[]> &vmBindExtSyncFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) override;
    std::optional<uint64_t> getCopyClassSaturatePCIECapability() override;
    std::optional<uint64_t> getCopyClassSaturateLinkCapability() override;
};

} // namespace NEO
