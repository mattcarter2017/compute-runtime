/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_xe_hpg_and_xe_hpc.inl"

#include "aubstream/product_family.h"
#include "platforms.h"

namespace NEO {

template <>
uint64_t ProductHelperHw<gfxProduct>::getHostMemCapabilitiesValue() const {
    return (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess);
}

template <>
bool ProductHelperHw<gfxProduct>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionConstantCacheInvalidationNeeded(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isInitBuiltinAsyncSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::blitEnqueuePreferred(bool isWriteToImageFromBuffer) const {
    return false;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Mtl;
}

template <>
bool ProductHelperHw<gfxProduct>::isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ, TaskCountType queueTaskCount, const CommandStreamReceiver &queueCsr) const {
    const bool enabled = !isOOQ && queueTaskCount == queueCsr.peekTaskCount();
    if (debugManager.flags.ResolveDependenciesViaPipeControls.get() != -1) {
        return debugManager.flags.ResolveDependenciesViaPipeControls.get() == 1;
    }
    return enabled;
}

template <>
bool ProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isUsmPoolAllocatorSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::overrideAllocationCacheable(const AllocationData &allocationData) const {
    return allocationData.type == AllocationType::commandBuffer;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getCommandBuffersPreallocatedPerCommandQueue() const {
    return 2u;
}

template <>
std::optional<GfxMemoryAllocationMethod> ProductHelperHw<gfxProduct>::getPreferredAllocationMethod(AllocationType allocationType) const {
    switch (allocationType) {
    case AllocationType::tagBuffer:
    case AllocationType::timestampPacketTagBuffer:
        return {};
    default:
        return GfxMemoryAllocationMethod::allocateByKmd;
    }
}

template <>
bool ProductHelperHw<gfxProduct>::isCachingOnCpuAvailable() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isNewCoherencyModelSupported() const {
    return true;
}

template <>
std::optional<bool> ProductHelperHw<gfxProduct>::isCoherentAllocation(uint64_t patIndex) const {
    std::array<uint64_t, 2> listOfCoherentPatIndexes = {3, 4};
    if (std::find(listOfCoherentPatIndexes.begin(), listOfCoherentPatIndexes.end(), patIndex) != listOfCoherentPatIndexes.end()) {
        return true;
    }
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceUsmAllocationReuseSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isHostUsmAllocationReuseSupported() const {
    return true;
}
} // namespace NEO
