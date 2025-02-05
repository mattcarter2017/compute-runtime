/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_properties.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/surface.h"

namespace NEO {

BlitProperties BlitProperties::constructPropertiesForMemoryFill(GraphicsAllocation *dstAllocation, size_t size, uint32_t *pattern, size_t patternSize, size_t offset) {
    return {
        .blitDirection = BlitterConstants::BlitDirection::fill,
        .dstAllocation = dstAllocation,
        .fillPattern = pattern,
        .copySize = {size, 1, 1},
        .dstOffset = {offset, 0, 0},
        .srcOffset = {0, 0, 0},
        .fillPatternSize = patternSize,
        .isSystemMemoryPoolUsed = MemoryPoolHelper::isSystemMemoryPool(dstAllocation->getMemoryPool())};
}

BlitProperties BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection blitDirection,
                                                               CommandStreamReceiver &commandStreamReceiver,
                                                               GraphicsAllocation *memObjAllocation,
                                                               GraphicsAllocation *preallocatedHostAllocation,
                                                               const void *hostPtr, uint64_t memObjGpuVa,
                                                               uint64_t hostAllocGpuVa, const Vec3<size_t> &hostPtrOffset,
                                                               const Vec3<size_t> &copyOffset, Vec3<size_t> copySize,
                                                               size_t hostRowPitch, size_t hostSlicePitch,
                                                               size_t gpuRowPitch, size_t gpuSlicePitch) {
    GraphicsAllocation *hostAllocation = nullptr;
    auto clearColorAllocation = commandStreamReceiver.getClearColorAllocation();

    copySize.y = copySize.y ? copySize.y : 1;
    copySize.z = copySize.z ? copySize.z : 1;

    if (preallocatedHostAllocation) {
        hostAllocation = preallocatedHostAllocation;
        UNRECOVERABLE_IF(hostAllocGpuVa == 0);
    } else {
        HostPtrSurface hostPtrSurface(hostPtr, static_cast<size_t>(copySize.x * copySize.y * copySize.z), true);
        bool success = commandStreamReceiver.createAllocationForHostSurface(hostPtrSurface, false);
        UNRECOVERABLE_IF(!success);
        hostAllocation = hostPtrSurface.getAllocation();
        hostAllocGpuVa = hostAllocation->getGpuAddress();
    }

    if (BlitterConstants::BlitDirection::hostPtrToBuffer == blitDirection ||
        BlitterConstants::BlitDirection::hostPtrToImage == blitDirection) {
        return {
            .blitSyncProperties = {},
            .csrDependencies = {},
            .multiRootDeviceEventSync = nullptr,
            .blitDirection = blitDirection,
            .auxTranslationDirection = AuxTranslationDirection::none,
            .dstAllocation = memObjAllocation,
            .srcAllocation = hostAllocation,
            .clearColorAllocation = clearColorAllocation,
            .dstGpuAddress = memObjGpuVa,
            .srcGpuAddress = hostAllocGpuVa,
            .copySize = copySize,
            .dstOffset = copyOffset,
            .srcOffset = hostPtrOffset,
            .dstRowPitch = gpuRowPitch,
            .dstSlicePitch = gpuSlicePitch,
            .srcRowPitch = hostRowPitch,
            .srcSlicePitch = hostSlicePitch,
            .dstSize = copySize,
            .srcSize = copySize,
            .isSystemMemoryPoolUsed = true};
    } else {
        return {
            .blitSyncProperties = {},
            .csrDependencies = {},
            .multiRootDeviceEventSync = nullptr,
            .blitDirection = blitDirection,
            .auxTranslationDirection = AuxTranslationDirection::none,
            .dstAllocation = hostAllocation,
            .srcAllocation = memObjAllocation,
            .clearColorAllocation = clearColorAllocation,
            .dstGpuAddress = hostAllocGpuVa,
            .srcGpuAddress = memObjGpuVa,
            .copySize = copySize,
            .dstOffset = hostPtrOffset,
            .srcOffset = copyOffset,
            .dstRowPitch = hostRowPitch,
            .dstSlicePitch = hostSlicePitch,
            .srcRowPitch = gpuRowPitch,
            .srcSlicePitch = gpuSlicePitch,
            .dstSize = copySize,
            .srcSize = copySize,
            .isSystemMemoryPoolUsed = true};
    };
}

BlitProperties BlitProperties::constructPropertiesForCopy(GraphicsAllocation *dstAllocation, GraphicsAllocation *srcAllocation,
                                                          const Vec3<size_t> &dstOffset, const Vec3<size_t> &srcOffset, Vec3<size_t> copySize,
                                                          size_t srcRowPitch, size_t srcSlicePitch,
                                                          size_t dstRowPitch, size_t dstSlicePitch, GraphicsAllocation *clearColorAllocation) {
    copySize.y = copySize.y ? copySize.y : 1;
    copySize.z = copySize.z ? copySize.z : 1;

    return {
        .blitSyncProperties = {},
        .csrDependencies = {},
        .multiRootDeviceEventSync = nullptr,
        .blitDirection = BlitterConstants::BlitDirection::bufferToBuffer,
        .auxTranslationDirection = AuxTranslationDirection::none,
        .dstAllocation = dstAllocation,
        .srcAllocation = srcAllocation,
        .clearColorAllocation = clearColorAllocation,
        .dstGpuAddress = dstAllocation->getGpuAddress(),
        .srcGpuAddress = srcAllocation->getGpuAddress(),
        .copySize = copySize,
        .dstOffset = dstOffset,
        .srcOffset = srcOffset,
        .dstRowPitch = dstRowPitch,
        .dstSlicePitch = dstSlicePitch,
        .srcRowPitch = srcRowPitch,
        .srcSlicePitch = srcSlicePitch,
        .isSystemMemoryPoolUsed = MemoryPoolHelper::isSystemMemoryPool(dstAllocation->getMemoryPool(), srcAllocation->getMemoryPool())};
}

BlitProperties BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection auxTranslationDirection,
                                                                    GraphicsAllocation *allocation, GraphicsAllocation *clearColorAllocation) {

    auto allocationSize = allocation->getUnderlyingBufferSize();
    return {
        .blitSyncProperties = {},
        .csrDependencies = {},
        .multiRootDeviceEventSync = nullptr,
        .blitDirection = BlitterConstants::BlitDirection::bufferToBuffer,
        .auxTranslationDirection = auxTranslationDirection,
        .dstAllocation = allocation,
        .srcAllocation = allocation,
        .clearColorAllocation = clearColorAllocation,
        .dstGpuAddress = allocation->getGpuAddress(),
        .srcGpuAddress = allocation->getGpuAddress(),
        .copySize = {allocationSize, 1, 1},
        .isSystemMemoryPoolUsed = MemoryPoolHelper::isSystemMemoryPool(allocation->getMemoryPool())};
}

void BlitProperties::setupDependenciesForAuxTranslation(BlitPropertiesContainer &blitPropertiesContainer, TimestampPacketDependencies &timestampPacketDependencies,
                                                        TimestampPacketContainer &kernelTimestamps, const CsrDependencies &depsFromEvents,
                                                        CommandStreamReceiver &gpguCsr, CommandStreamReceiver &bcsCsr) {
    auto numObjects = blitPropertiesContainer.size() / 2;

    for (size_t i = 0; i < numObjects; i++) {
        blitPropertiesContainer[i].blitSyncProperties.outputTimestampPacket = timestampPacketDependencies.auxToNonAuxNodes.peekNodes()[i];
        blitPropertiesContainer[i].blitSyncProperties.syncMode = BlitSyncMode::immediate;

        blitPropertiesContainer[i + numObjects].blitSyncProperties.outputTimestampPacket = timestampPacketDependencies.nonAuxToAuxNodes.peekNodes()[i];
        blitPropertiesContainer[i + numObjects].blitSyncProperties.syncMode = BlitSyncMode::immediate;
    }

    auto nodesAllocator = gpguCsr.getTimestampPacketAllocator();
    timestampPacketDependencies.barrierNodes.add(nodesAllocator->getTag());

    // wait for barrier and events before AuxToNonAux
    blitPropertiesContainer[0].csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies.barrierNodes);

    for (auto dep : depsFromEvents.timestampPacketContainer) {
        blitPropertiesContainer[0].csrDependencies.timestampPacketContainer.push_back(dep);
    }

    // wait for NDR before NonAuxToAux
    blitPropertiesContainer[numObjects].csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies.cacheFlushNodes);
    blitPropertiesContainer[numObjects].csrDependencies.timestampPacketContainer.push_back(&kernelTimestamps);
}

bool BlitProperties::isImageOperation() const {
    return blitDirection == BlitterConstants::BlitDirection::hostPtrToImage ||
           blitDirection == BlitterConstants::BlitDirection::imageToHostPtr ||
           blitDirection == BlitterConstants::BlitDirection::imageToImage;
}

} // namespace NEO