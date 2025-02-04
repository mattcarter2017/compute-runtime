/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_base.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {
ScratchSpaceControllerBase::ScratchSpaceControllerBase(uint32_t rootDeviceIndex, ExecutionEnvironment &environment, InternalAllocationStorage &allocationStorage)
    : ScratchSpaceController(rootDeviceIndex, environment, allocationStorage) {
}

void ScratchSpaceControllerBase::setRequiredScratchSpace(void *sshBaseAddress,
                                                         uint32_t scratchSlot,
                                                         uint32_t requiredPerThreadScratchSize,
                                                         uint32_t requiredPerThreadPrivateScratchSize,
                                                         TaskCountType currentTaskCount,
                                                         OsContext &osContext,
                                                         bool &stateBaseAddressDirty,
                                                         bool &vfeStateDirty) {
    size_t requiredScratchSizeInBytes = requiredPerThreadScratchSize * computeUnitsUsedForScratch;
    if (requiredScratchSizeInBytes && (scratchSizeBytes < requiredScratchSizeInBytes)) {
        if (scratchAllocation) {
            scratchAllocation->updateTaskCount(currentTaskCount, osContext.getContextId());
            csrAllocationStorage.storeAllocation(std::unique_ptr<GraphicsAllocation>(scratchAllocation), TEMPORARY_ALLOCATION);
        }
        scratchSizeBytes = requiredScratchSizeInBytes;
        createScratchSpaceAllocation();
        vfeStateDirty = true;
        force32BitAllocation = getMemoryManager()->peekForce32BitAllocations();
        if (is64bit && !force32BitAllocation) {
            stateBaseAddressDirty = true;
        }
    }
}

void ScratchSpaceControllerBase::createScratchSpaceAllocation() {
    scratchAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties({rootDeviceIndex, scratchSizeBytes, AllocationType::scratchSurface, this->csrAllocationStorage.getDeviceBitfield()});
    UNRECOVERABLE_IF(scratchAllocation == nullptr);
}

uint64_t ScratchSpaceControllerBase::calculateNewGSH() {
    uint64_t gsh = 0;
    if (scratchAllocation) {
        gsh = scratchAllocation->getGpuAddress() - ScratchSpaceConstants::scratchSpaceOffsetFor64Bit;
    }
    return gsh;
}
uint64_t ScratchSpaceControllerBase::getScratchPatchAddress() {
    // for 32 bit scratch space pointer is being programmed in Media VFE State and is relative to 0 as General State Base Address
    // for 64 bit, scratch space pointer is being programmed as "General State Base Address - scratchSpaceOffsetFor64bit"
    //             and "0 + scratchSpaceOffsetFor64bit" is being programmed in Media VFE state
    uint64_t scratchAddress = 0;
    if (scratchAllocation) {
        scratchAddress = scratchAllocation->getGpuAddressToPatch();
        if (is64bit && !getMemoryManager()->peekForce32BitAllocations()) {
            // this is to avoid scractch allocation offset "0"
            scratchAddress = ScratchSpaceConstants::scratchSpaceOffsetFor64Bit;
        }
    }
    return scratchAddress;
}

void ScratchSpaceControllerBase::reserveHeap(IndirectHeap::Type heapType, IndirectHeap *&indirectHeap) {
    if (heapType == IndirectHeap::Type::surfaceState) {
        auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHelper<GfxCoreHelper>();
        auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();
        indirectHeap->getSpace(surfaceStateSize);
    }
}

void ScratchSpaceControllerBase::programHeaps(HeapContainer &heapContainer,
                                              uint32_t offset,
                                              uint32_t requiredPerThreadScratchSize,
                                              uint32_t requiredPerThreadPrivateScratchSize,
                                              TaskCountType currentTaskCount,
                                              OsContext &osContext,
                                              bool &stateBaseAddressDirty,
                                              bool &vfeStateDirty) {
}

void ScratchSpaceControllerBase::programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                                                       uint32_t requiredPerThreadScratchSize,
                                                                       uint32_t requiredPerThreadPrivateScratchSize,
                                                                       TaskCountType currentTaskCount,
                                                                       OsContext &osContext,
                                                                       bool &stateBaseAddressDirty,
                                                                       bool &vfeStateDirty,
                                                                       NEO::CommandStreamReceiver *csr) {
}
} // namespace NEO
