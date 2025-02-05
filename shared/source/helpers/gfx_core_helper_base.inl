/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/tag_allocator.h"

#include "encode_surface_state_args.h"

namespace NEO {

template <typename Family>
const AuxTranslationMode GfxCoreHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::builtin;

template <typename Family>
bool GfxCoreHelperHw<Family>::isBufferSizeSuitableForCompression(const size_t size) const {
    if (debugManager.flags.OverrideBufferSuitableForRenderCompression.get() != -1) {
        return !!debugManager.flags.OverrideBufferSuitableForRenderCompression.get();
    }
    return size > MemoryConstants::kiloByte;
}

template <typename Family>
size_t GfxCoreHelperHw<Family>::getMax3dImageWidthOrHeight() const {
    return 16384;
}

template <typename Family>
uint64_t GfxCoreHelperHw<Family>::getMaxMemAllocSize() const {
    // With stateful messages we have an allocation cap of 4GB
    // Reason to subtract 8KB is that driver may pad the buffer with addition pages for over fetching
    return (4ULL * MemoryConstants::gigaByte) - (8ULL * MemoryConstants::kiloByte);
}

template <typename Family>
bool GfxCoreHelperHw<Family>::isStatelessToStatefulWithOffsetSupported() const {
    return true;
}

template <typename Family>
SipKernelType GfxCoreHelperHw<Family>::getSipKernelType(bool debuggingActive) const {
    if (!debuggingActive) {
        return SipKernelType::csr;
    }
    return debugManager.flags.UseBindlessDebugSip.get() ? SipKernelType::dbgBindless : SipKernelType::dbgCsr;
}

template <typename Family>
size_t GfxCoreHelperHw<Family>::getMaxBarrierRegisterPerSlice() const {
    return 32;
}

template <typename Family>
uint32_t GfxCoreHelperHw<Family>::getPitchAlignmentForImage(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return 4u;
}

template <typename Family>
const AubMemDump::LrcaHelper &GfxCoreHelperHw<Family>::getCsTraits(aub_stream::EngineType engineType) const {
    return *AUBFamilyMapper<Family>::csTraits[engineType];
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) const {
    return true;
}

template <typename Family>
void GfxCoreHelperHw<Family>::setRenderSurfaceStateForScratchResource(const RootDeviceEnvironment &rootDeviceEnvironment,
                                                                      void *surfaceStateBuffer,
                                                                      size_t bufferSize,
                                                                      uint64_t gpuVa,
                                                                      size_t offset,
                                                                      uint32_t pitch,
                                                                      GraphicsAllocation *gfxAlloc,
                                                                      bool isReadOnly,
                                                                      uint32_t surfaceType,
                                                                      bool forceNonAuxMode,
                                                                      bool useL1Cache) const {
    using RENDER_SURFACE_STATE = typename Family::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto gmmHelper = rootDeviceEnvironment.getGmmHelper();
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuffer);
    RENDER_SURFACE_STATE state = Family::cmdInitRenderSurfaceState;
    auto surfaceSize = alignUp(bufferSize, 4);

    SurfaceStateBufferLength length = {0};
    length.length = static_cast<uint32_t>(surfaceSize - 1);

    state.setWidth(length.surfaceState.width + 1);
    state.setHeight(length.surfaceState.height + 1);
    state.setDepth(length.surfaceState.depth + 1);
    if (pitch) {
        EncodeSurfaceState<Family>::setPitchForScratch(&state, pitch, rootDeviceEnvironment.getProductHelper());
    }

    // The graphics allocation for Host Ptr surface will be created in makeResident call and GPU address is expected to be the same as CPU address
    auto bufferStateAddress = (gfxAlloc != nullptr) ? gfxAlloc->getGpuAddress() : gpuVa;
    bufferStateAddress += offset;

    auto bufferStateSize = (gfxAlloc != nullptr) ? gfxAlloc->getUnderlyingBufferSize() : bufferSize;

    state.setSurfaceType(static_cast<typename RENDER_SURFACE_STATE::SURFACE_TYPE>(surfaceType));

    state.setSurfaceFormat(SURFACE_FORMAT::SURFACE_FORMAT_RAW);
    state.setSurfaceVerticalAlignment(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4);
    state.setSurfaceHorizontalAlignment(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT);

    state.setTileMode(RENDER_SURFACE_STATE::TILE_MODE_LINEAR);
    state.setVerticalLineStride(0);
    state.setVerticalLineStrideOffset(0);
    if ((isAligned<MemoryConstants::cacheLineSize>(bufferStateAddress) && isAligned<MemoryConstants::cacheLineSize>(bufferStateSize)) ||
        isReadOnly) {
        state.setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    } else {
        state.setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    }
    if (debugManager.flags.OverrideMocsIndexForScratchSpace.get() != -1) {
        auto mocsIndex = static_cast<uint32_t>(debugManager.flags.OverrideMocsIndexForScratchSpace.get()) << 1;
        state.setMemoryObjectControlState(mocsIndex);
    }

    state.setSurfaceBaseAddress(bufferStateAddress);

    bool isCompressionEnabled = gfxAlloc ? gfxAlloc->isCompressionEnabled() : false;
    if (isCompressionEnabled && !forceNonAuxMode) {
        // Its expected to not program pitch/qpitch/baseAddress for Aux surface in CCS scenarios
        EncodeSurfaceState<Family>::setCoherencyType(&state, RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
        EncodeSurfaceState<Family>::setBufferAuxParamsForCCS(&state);
    } else {
        EncodeSurfaceState<Family>::setCoherencyType(&state, RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT);
        state.setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    }
    setL1CachePolicy(useL1Cache, &state, rootDeviceEnvironment.getHardwareInfo());

    *surfaceState = state;
}

template <typename GfxFamily>
void NEO::GfxCoreHelperHw<GfxFamily>::setL1CachePolicy(bool useL1Cache, typename GfxFamily::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) const {}

template <typename Family>
bool GfxCoreHelperHw<Family>::getEnableLocalMemory(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.EnableLocalMemory.get() != -1) {
        return debugManager.flags.EnableLocalMemory.get();
    } else if (debugManager.flags.AUBDumpForceAllToLocalMemory.get()) {
        return true;
    }

    return OSInterface::osEnableLocalMemory && isLocalMemoryEnabled(hwInfo);
}

template <typename Family>
bool GfxCoreHelperHw<Family>::is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const {
    return false;
}

template <typename Family>
AuxTranslationMode GfxCoreHelperHw<Family>::getAuxTranslationMode(const HardwareInfo &hwInfo) {
    auto mode = GfxCoreHelperHw<Family>::defaultAuxTranslationMode;
    if (debugManager.flags.ForceAuxTranslationMode.get() != -1) {
        mode = static_cast<AuxTranslationMode>(debugManager.flags.ForceAuxTranslationMode.get());
    }

    if (mode == AuxTranslationMode::blit && !hwInfo.capabilityTable.blitterOperationsSupported) {
        DEBUG_BREAK_IF(true);
        mode = AuxTranslationMode::builtin;
    }

    return mode;
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(LinearStream &commandStream, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData,
                                                                               const RootDeviceEnvironment &rootDeviceEnvironment, PipeControlArgs &args) {

    void *commandBuffer = commandStream.getSpace(MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, args.tlbInvalidation));

    MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(commandBuffer, postSyncMode, gpuAddress, immediateData, rootDeviceEnvironment, args);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
    void *&commandsBuffer,
    PostSyncMode postSyncMode,
    uint64_t gpuAddress,
    uint64_t immediateData,
    const RootDeviceEnvironment &rootDeviceEnvironment,
    PipeControlArgs &args) {

    MemorySynchronizationCommands<GfxFamily>::setBarrierWa(commandsBuffer, gpuAddress, rootDeviceEnvironment);

    if (!args.blockSettingPostSyncProperties) {
        setPostSyncExtraProperties(args);
    }
    MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(commandsBuffer, postSyncMode, gpuAddress, immediateData, args);
    commandsBuffer = ptrOffset(commandsBuffer, getSizeForSingleBarrier(args.tlbInvalidation));

    MemorySynchronizationCommands<GfxFamily>::setAdditionalSynchronization(commandsBuffer, gpuAddress, false, rootDeviceEnvironment);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(LinearStream &commandStream, PipeControlArgs &args) {
    addSingleBarrier(commandStream, PostSyncMode::noWrite, 0, 0, args);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(void *commandsBuffer, PipeControlArgs &args) {
    setSingleBarrier(commandsBuffer, PostSyncMode::noWrite, 0, 0, args);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(LinearStream &commandStream, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, PipeControlArgs &args) {
    auto barrier = commandStream.getSpace(MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(args.tlbInvalidation));

    setSingleBarrier(barrier, postSyncMode, gpuAddress, immediateData, args);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(void *commandsBuffer, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, PipeControlArgs &args) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    PIPE_CONTROL pipeControl = GfxFamily::cmdInitPipeControl;

    pipeControl.setCommandStreamerStallEnable(true);
    setBarrierExtraProperties(&pipeControl, args);

    if (args.csStallOnly) {
        *reinterpret_cast<PIPE_CONTROL *>(commandsBuffer) = pipeControl;
        return;
    }

    pipeControl.setConstantCacheInvalidationEnable(args.constantCacheInvalidationEnable);
    pipeControl.setInstructionCacheInvalidateEnable(args.instructionCacheInvalidateEnable);
    pipeControl.setPipeControlFlushEnable(args.pipeControlFlushEnable);
    pipeControl.setRenderTargetCacheFlushEnable(args.renderTargetCacheFlushEnable);
    pipeControl.setStateCacheInvalidationEnable(args.stateCacheInvalidationEnable);
    pipeControl.setTextureCacheInvalidationEnable(args.textureCacheInvalidationEnable);
    pipeControl.setVfCacheInvalidationEnable(args.vfCacheInvalidationEnable);
    pipeControl.setTlbInvalidate(args.tlbInvalidation);
    pipeControl.setNotifyEnable(args.notifyEnable);
    pipeControl.setDcFlushEnable(args.dcFlushEnable);
    pipeControl.setDepthCacheFlushEnable(args.depthCacheFlushEnable);
    pipeControl.setDepthStallEnable(args.depthStallEnable);
    pipeControl.setProtectedMemoryDisable(args.protectedMemoryDisable);

    if constexpr (GfxFamily::isUsingGenericMediaStateClear) {
        pipeControl.setGenericMediaStateClear(args.genericMediaStateClear);
    }

    if (debugManager.flags.FlushAllCaches.get()) {
        pipeControl.setDcFlushEnable(true);
        pipeControl.setRenderTargetCacheFlushEnable(true);
        pipeControl.setInstructionCacheInvalidateEnable(true);
        pipeControl.setTextureCacheInvalidationEnable(true);
        pipeControl.setPipeControlFlushEnable(true);
        pipeControl.setVfCacheInvalidationEnable(true);
        pipeControl.setConstantCacheInvalidationEnable(true);
        pipeControl.setStateCacheInvalidationEnable(true);
        pipeControl.setTlbInvalidate(true);
    }
    if (debugManager.flags.DoNotFlushCaches.get()) {
        pipeControl.setDcFlushEnable(false);
        pipeControl.setRenderTargetCacheFlushEnable(false);
        pipeControl.setInstructionCacheInvalidateEnable(false);
        pipeControl.setTextureCacheInvalidationEnable(false);
        pipeControl.setPipeControlFlushEnable(false);
        pipeControl.setVfCacheInvalidationEnable(false);
        pipeControl.setConstantCacheInvalidationEnable(false);
        pipeControl.setStateCacheInvalidationEnable(false);
    }

    if (postSyncMode != PostSyncMode::noWrite) {
        args.postSyncCmd = commandsBuffer;
        pipeControl.setAddress(static_cast<uint32_t>(gpuAddress & 0x0000FFFFFFFFULL));
        pipeControl.setAddressHigh(static_cast<uint32_t>(gpuAddress >> 32));
    }

    if (postSyncMode == PostSyncMode::timestamp) {
        pipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    } else if (postSyncMode == PostSyncMode::immediateData) {
        pipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
        pipeControl.setImmediateData(immediateData);
    }

    *reinterpret_cast<PIPE_CONTROL *>(commandsBuffer) = pipeControl;
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addBarrierWa(LinearStream &commandStream, uint64_t gpuAddress, const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t requiredSize = MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWa(rootDeviceEnvironment);
    void *commandBuffer = commandStream.getSpace(requiredSize);
    setBarrierWa(commandBuffer, gpuAddress, rootDeviceEnvironment);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setBarrierWa(void *&commandsBuffer, uint64_t gpuAddress, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    if (MemorySynchronizationCommands<GfxFamily>::isBarrierWaRequired(rootDeviceEnvironment)) {
        PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl;
        MemorySynchronizationCommands<GfxFamily>::setBarrierWaFlags(&cmd);
        *reinterpret_cast<PIPE_CONTROL *>(commandsBuffer) = cmd;
        commandsBuffer = ptrOffset(commandsBuffer, sizeof(PIPE_CONTROL));

        MemorySynchronizationCommands<GfxFamily>::setAdditionalSynchronization(commandsBuffer, gpuAddress, false, rootDeviceEnvironment);
    }
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t requiredSize = MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronization(rootDeviceEnvironment);
    void *commandBuffer = commandStream.getSpace(requiredSize);
    setAdditionalSynchronization(commandBuffer, gpuAddress, acquire, rootDeviceEnvironment);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronizationForDirectSubmission(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const RootDeviceEnvironment &rootDeviceEnvironment) {
    MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, gpuAddress, acquire, rootDeviceEnvironment);
}

template <typename GfxFamily>
bool MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(bool isFlushPreferred, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (isFlushPreferred) {
        const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        return productHelper.isDcFlushAllowed();
    }
    return false;
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(bool tlbInvalidationRequired) {
    return sizeof(typename GfxFamily::PIPE_CONTROL);
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(const RootDeviceEnvironment &rootDeviceEnvironment, bool tlbInvalidationRequired) {

    size_t size = getSizeForSingleBarrier(tlbInvalidationRequired);
    size += getSizeForBarrierWa(rootDeviceEnvironment);
    size += getSizeForSingleAdditionalSynchronization(rootDeviceEnvironment);
    return size;
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWa(const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t size = 0;
    if (MemorySynchronizationCommands<GfxFamily>::isBarrierWaRequired(rootDeviceEnvironment)) {
        size = getSizeForSingleBarrier(false) +
               getSizeForSingleAdditionalSynchronization(rootDeviceEnvironment);
    }
    return size;
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setAdditionalSynchronization(void *&commandsBuffer, uint64_t gpuAddress, bool acquire, const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <typename GfxFamily>
inline size_t MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronization(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return 0u;
}

template <typename GfxFamily>
inline size_t MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronizationForDirectSubmission(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronization(rootDeviceEnvironment);
}

template <typename GfxFamily>
inline size_t MemorySynchronizationCommands<GfxFamily>::getSizeForAdditonalSynchronization(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return 0u;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Gen9);
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::alignSlmSize(uint32_t slmSize) const {
    return EncodeDispatchKernel<GfxFamily>::alignSlmSize(slmSize);
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) const {
    return EncodeDispatchKernel<GfxFamily>::computeSlmValues(hwInfo, slmSize);
}

template <typename GfxFamily>
uint8_t GfxCoreHelperHw<GfxFamily>::getBarriersCountFromHasBarriers(uint8_t hasBarriers) const {
    return hasBarriers;
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isWaDisableRccRhwoOptimizationRequired() const {
    return false;
}

template <typename GfxFamily>
inline uint32_t GfxCoreHelperHw<GfxFamily>::getMinimalSIMDSize() const {
    return 8u;
}

template <typename GfxFamily>
std::unique_ptr<TagAllocatorBase> GfxCoreHelperHw<GfxFamily>::createTimestampPacketAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, MemoryManager *memoryManager,
                                                                                             size_t initialTagCount, CommandStreamReceiverType csrType, DeviceBitfield deviceBitfield) const {
    bool doNotReleaseNodes = (csrType > CommandStreamReceiverType::hardware) ||
                             debugManager.flags.DisableTimestampPacketOptimizations.get();

    auto tagAlignment = getTimestampPacketAllocatorAlignment();

    if (debugManager.flags.OverrideTimestampPacketSize.get() != -1) {
        if (debugManager.flags.OverrideTimestampPacketSize.get() == 4) {
            using TimestampPackets32T = TimestampPackets<uint32_t, GfxFamily::timestampPacketCount>;
            return std::make_unique<TagAllocator<TimestampPackets32T>>(rootDeviceIndices, memoryManager, initialTagCount, tagAlignment, sizeof(TimestampPackets32T), NEO::TimestampPacketConstants::initValue,
                                                                       doNotReleaseNodes, true, deviceBitfield);
        } else if (debugManager.flags.OverrideTimestampPacketSize.get() == 8) {
            using TimestampPackets64T = TimestampPackets<uint64_t, GfxFamily::timestampPacketCount>;
            return std::make_unique<TagAllocator<TimestampPackets64T>>(rootDeviceIndices, memoryManager, initialTagCount, tagAlignment, sizeof(TimestampPackets64T), NEO::TimestampPacketConstants::initValue,
                                                                       doNotReleaseNodes, true, deviceBitfield);
        } else {
            UNRECOVERABLE_IF(true);
        }
    }

    using TimestampPacketType = typename GfxFamily::TimestampPacketType;
    using TimestampPacketsT = TimestampPackets<TimestampPacketType, GfxFamily::timestampPacketCount>;

    return std::make_unique<TagAllocator<TimestampPacketsT>>(rootDeviceIndices, memoryManager, initialTagCount, tagAlignment, sizeof(TimestampPacketsT), NEO::TimestampPacketConstants::initValue,
                                                             doNotReleaseNodes, true, deviceBitfield);
}

template <typename GfxFamily>
size_t GfxCoreHelperHw<GfxFamily>::getTimestampPacketAllocatorAlignment() const {
    return MemoryConstants::cacheLineSize * 4;
}

template <typename GfxFamily>
size_t GfxCoreHelperHw<GfxFamily>::getSingleTimestampPacketSize() const {
    return GfxCoreHelperHw<GfxFamily>::getSingleTimestampPacketSizeHw();
}

template <typename GfxFamily>
size_t GfxCoreHelperHw<GfxFamily>::getSingleTimestampPacketSizeHw() {
    if (debugManager.flags.OverrideTimestampPacketSize.get() != -1) {
        if (debugManager.flags.OverrideTimestampPacketSize.get() == 4) {
            return TimestampPackets<uint32_t, GfxFamily::timestampPacketCount>::getSinglePacketSize();
        } else if (debugManager.flags.OverrideTimestampPacketSize.get() == 8) {
            return TimestampPackets<uint64_t, GfxFamily::timestampPacketCount>::getSinglePacketSize();
        } else {
            UNRECOVERABLE_IF(true);
        }
    }

    return TimestampPackets<typename GfxFamily::TimestampPacketType, GfxFamily::timestampPacketCount>::getSinglePacketSize();
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForFullCacheFlush() {
    return MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(true);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addFullCacheFlush(LinearStream &commandStream, const RootDeviceEnvironment &rootDeviceEnvironment) {
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, rootDeviceEnvironment);
    args.renderTargetCacheFlushEnable = true;
    args.instructionCacheInvalidateEnable = true;
    args.textureCacheInvalidationEnable = true;
    args.pipeControlFlushEnable = true;
    args.constantCacheInvalidationEnable = true;
    args.stateCacheInvalidationEnable = true;
    args.tlbInvalidation = true;
    MemorySynchronizationCommands<GfxFamily>::setCacheFlushExtraProperties(args);
    MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStream, args);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addStateCacheFlush(LinearStream &commandStream, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl;
    cmd.setCommandStreamerStallEnable(true);
    cmd.setRenderTargetCacheFlushEnable(true);
    cmd.setStateCacheInvalidationEnable(true);
    cmd.setTextureCacheInvalidationEnable(true);

    auto commandsBuffer = commandStream.getSpace(sizeof(PIPE_CONTROL));
    *reinterpret_cast<PIPE_CONTROL *>(commandsBuffer) = cmd;
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForInstructionCacheFlush() {
    return MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(false);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addInstructionCacheFlush(LinearStream &commandStream) {
    PipeControlArgs args;
    args.instructionCacheInvalidateEnable = true;

    MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStream, args);
}

template <typename GfxFamily>
const StackVec<size_t, 3> GfxCoreHelperHw<GfxFamily>::getDeviceSubGroupSizes() const {
    return {8, 16, 32};
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isBankOverrideRequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const {
    return false;
}

template <typename GfxFamily>
int32_t GfxCoreHelperHw<GfxFamily>::getDefaultThreadArbitrationPolicy() const {
    return 0;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::useOnlyGlobalTimestamps() const {
    return debugManager.flags.ForceUseOnlyGlobalTimestamps.get();
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::useSystemMemoryPlacementForISA(const HardwareInfo &hwInfo) const {
    return !getEnableLocalMemory(hwInfo);
}

template <typename GfxFamily>
bool MemorySynchronizationCommands<GfxFamily>::isBarrierPriorToPipelineSelectWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isSubDeviceEngineSupported(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const {
    return true;
}

template <typename GfxFamily>
size_t GfxCoreHelperHw<GfxFamily>::getPreemptionAllocationAlignment() const {
    return 256 * MemoryConstants::kiloByte;
}

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const {}

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::applyRenderCompressionFlag(Gmm &gmm, uint32_t isCompressed) const {
    gmm.resourceParams.Flags.Info.RenderCompressed = isCompressed;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isSipKernelAsHexadecimalArrayPreferred() const {
    return false;
}

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::setSipKernelData(uint32_t *&sipKernelBinary, size_t &kernelBinarySize, const RootDeviceEnvironment &rootDeviceEnvironment) const {
}

template <typename GfxFamily>
size_t GfxCoreHelperHw<GfxFamily>::getSipKernelMaxDbgSurfaceSize(const HardwareInfo &hwInfo) const {
    return 24 * MemoryConstants::megaByte;
}

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::adjustPreemptionSurfaceSize(size_t &csrSize, const RootDeviceEnvironment &rootDeviceEnvironment) const {
}

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) const {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(args.outMemory);
    *surfaceState = GfxFamily::cmdInitRenderSurfaceState;

    EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
}
template <typename GfxFamily>
size_t GfxCoreHelperHw<GfxFamily>::getBatchBufferEndSize() const {
    return EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferEndSize();
}
template <typename GfxFamily>
const void *GfxCoreHelperHw<GfxFamily>::getBatchBufferEndReference() const {
    return reinterpret_cast<const void *>(&GfxFamily::cmdInitBatchBufferEnd);
}

template <typename GfxFamily>
size_t GfxCoreHelperHw<GfxFamily>::getBatchBufferStartSize() const {
    return EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize();
}

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::encodeBatchBufferStart(void *cmdBuffer, uint64_t address, bool secondLevel, bool indirect, bool predicate) const {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    MI_BATCH_BUFFER_START *bbBuffer = reinterpret_cast<MI_BATCH_BUFFER_START *>(cmdBuffer);
    EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(bbBuffer, address, secondLevel, indirect, predicate);
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isPlatformFlushTaskEnabled(const ProductHelper &productHelper) const {
    return productHelper.isFlushTaskAllowed();
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::copyThroughLockedPtrEnabled(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const {
    if (debugManager.flags.ExperimentalCopyThroughLock.get() != -1) {
        return debugManager.flags.ExperimentalCopyThroughLock.get() == 1;
    }
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isChipsetUniqueUUIDSupported() const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isTimestampShiftRequired() const {
    return true;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isRelaxedOrderingSupported() const {
    return false;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::overrideMaxWorkGroupSize(uint32_t maxWG) const {
    return std::min(maxWG, 1024u);
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::adjustMaxWorkGroupSize(const uint32_t grfCount, const uint32_t simd, bool isHwLocalGeneration, const uint32_t defaultMaxGroupSize, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return defaultMaxGroupSize;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMinimalGrfSize() const {
    return 128u;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::calculateNumThreadsPerThreadGroup(uint32_t simd, uint32_t totalWorkItems, uint32_t grfCount, bool isHwLocalIdGeneration, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return getThreadsPerWG(simd, totalWorkItems);
}

template <typename GfxFamily>
DeviceHierarchyMode GfxCoreHelperHw<GfxFamily>::getDefaultDeviceHierarchy() const {
    return DeviceHierarchyMode::composite;
}

template <typename GfxFamily>
uint64_t GfxCoreHelperHw<GfxFamily>::getGpuTimeStampInNS(uint64_t timeStamp, double resolution) const {
    auto numBitsForResolution = Math::log2(static_cast<uint64_t>(resolution)) + 1u;
    auto timestampMask = maxNBitValue(64 - numBitsForResolution);
    return static_cast<uint64_t>(static_cast<uint64_t>(timeStamp & timestampMask) * resolution);
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::areSecondaryContextsSupported() const {
    return getContextGroupContextsCount() > 1;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getContextGroupContextsCount() const {
    if (debugManager.flags.ContextGroupSize.get() != -1) {
        return debugManager.flags.ContextGroupSize.get();
    }
    return 0;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getContextGroupHpContextsCount(EngineGroupType type, bool hpEngineAvailable) const {
    if (hpEngineAvailable) {
        return 0;
    }
    return std::min(getContextGroupContextsCount() / 2, 4u);
}

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::adjustCopyEngineRegularContextCount(const size_t enginesCount, uint32_t &contextCount) const {
}

template <typename GfxFamily>
aub_stream::EngineType GfxCoreHelperHw<GfxFamily>::getDefaultHpCopyEngine(const HardwareInfo &hwInfo) const {
    return hpCopyEngineType;
}

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::initializeDefaultHpCopyEngine(const HardwareInfo &hwInfo) {
    hpCopyEngineType = aub_stream::EngineType::NUM_ENGINES;
}

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::initializeFromProductHelper(const ProductHelper &productHelper) {
    secondaryContextsEnabled = productHelper.areSecondaryContextsSupported();
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::is48ResourceNeededForCmdBuffer() const {
    return true;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::singleTileExecImplicitScalingRequired(bool cooperativeKernel) const {
    return EncodeDispatchKernel<GfxFamily>::singleTileExecImplicitScalingRequired(cooperativeKernel);
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::duplicatedInOrderCounterStorageEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return (debugManager.flags.InOrderDuplicatedCounterStorageEnabled.get() == 1);
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::inOrderAtomicSignallingEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return (debugManager.flags.InOrderAtomicSignallingEnabled.get() == 1);
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getRenderSurfaceStatePitch(void *renderSurfaceState, const ProductHelper &productHelper) const {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(renderSurfaceState);
    return EncodeSurfaceState<GfxFamily>::getPitchForScratchInBytes(surfaceState, productHelper);
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isRuntimeLocalIdsGenerationRequired(uint32_t activeChannels,
                                                                     const size_t *lws,
                                                                     std::array<uint8_t, 3> &walkOrder,
                                                                     bool requireInputWalkOrder,
                                                                     uint32_t &requiredWalkOrder,
                                                                     uint32_t simd) const {
    return EncodeDispatchKernel<GfxFamily>::isRuntimeLocalIdsGenerationRequired(activeChannels,
                                                                                lws,
                                                                                walkOrder,
                                                                                requireInputWalkOrder,
                                                                                requiredWalkOrder,
                                                                                simd);
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMaxPtssIndex(const ProductHelper &productHelper) const {
    return 15u;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getDefaultSshSize(const ProductHelper &productHelper) const {
    return HeapSize::defaultHeapSize;
}

template <typename GfxFamily>
void *LriHelper<GfxFamily>::program(LinearStream *cmdStream, uint32_t address, uint32_t value, bool remap, bool isBcs) {
    auto lri = cmdStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    return LriHelper<GfxFamily>::program(lri, address, value, remap, isBcs);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::encodeAdditionalTimestampOffsets(LinearStream &commandStream, uint64_t contextAddress, uint64_t globalAddress, bool isBcs) {
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const {
    auto maxThreadsPerEuCount = 8u;
    if (grfCount == GrfConfig::largeGrfNumber) {
        maxThreadsPerEuCount = 4;
    }
    return std::min(hwInfo.gtSystemInfo.ThreadCount, maxThreadsPerEuCount * hwInfo.gtSystemInfo.EUCount);
}

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::alignThreadGroupCountToDssSize(uint32_t &threadCount, uint32_t dssCount, uint32_t threadsPerDss, uint32_t threadGroupSize) const {
    uint32_t availableTreadCount = (threadsPerDss / threadGroupSize) * dssCount;
    threadCount = std::min(threadCount, availableTreadCount);
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getDeviceTimestampWidth() const {
    if (debugManager.flags.OverrideTimestampWidth.get() != -1) {
        return debugManager.flags.OverrideTimestampWidth.get();
    }
    return 0u;
}

template <typename Family>
uint32_t GfxCoreHelperHw<Family>::getInternalCopyEngineIndex(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.ForceBCSForInternalCopyEngine.get() != -1) {
        return debugManager.flags.ForceBCSForInternalCopyEngine.get();
    }

    constexpr uint32_t defaultInternalCopyEngineIndex = 3u;
    auto highestAvailableIndex = getMostSignificantSetBitIndex(hwInfo.featureTable.ftrBcsInfo.to_ullong());
    return std::min(defaultInternalCopyEngineIndex, highestAvailableIndex);
}

} // namespace NEO
