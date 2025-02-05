/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/l3_range.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/kernel/implicit_args_helper.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/kernel/kernel.h"

namespace NEO {

template <typename GfxFamily>
typename HardwareCommandsHelper<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *HardwareCommandsHelper<GfxFamily>::getInterfaceDescriptor(
    const IndirectHeap &indirectHeap,
    uint64_t offsetInterfaceDescriptor,
    INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor) {

    return inlineInterfaceDescriptor;
}

template <typename GfxFamily>
uint32_t HardwareCommandsHelper<GfxFamily>::additionalSizeRequiredDsh() {
    return 0u;
}

template <typename GfxFamily>
size_t HardwareCommandsHelper<GfxFamily>::getSizeRequiredCS() {
    return 0;
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::sendMediaStateFlush(
    LinearStream &commandStream,
    size_t offsetInterfaceDescriptorData) {
}

template <typename GfxFamily>
void HardwareCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
    LinearStream &commandStream,
    size_t offsetInterfaceDescriptorData,
    size_t sizeInterfaceDescriptorData) {
}

template <typename GfxFamily>
template <typename WalkerType>
size_t HardwareCommandsHelper<GfxFamily>::sendCrossThreadData(
    IndirectHeap &indirectHeap,
    Kernel &kernel,
    bool inlineDataProgrammingRequired,
    WalkerType *walkerCmd,
    uint32_t &sizeCrossThreadData,
    [[maybe_unused]] uint64_t scratchAddress,
    const RootDeviceEnvironment &rootDeviceEnvironment) {
    constexpr bool heaplessModeEnabled = GfxFamily::template isHeaplessMode<WalkerType>();

    indirectHeap.align(GfxFamily::cacheLineSize);

    auto offsetCrossThreadData = indirectHeap.getUsed();
    char *dest = nullptr;
    char *src = kernel.getCrossThreadData();

    auto pImplicitArgs = kernel.getImplicitArgs();
    if (pImplicitArgs) {
        pImplicitArgs->localIdTablePtr = indirectHeap.getGraphicsAllocation()->getGpuAddress() + offsetCrossThreadData;

        const auto &kernelDescriptor = kernel.getDescriptor();

        const auto &kernelAttributes = kernelDescriptor.kernelAttributes;
        uint32_t requiredWalkOrder = 0u;
        size_t localWorkSize[3] = {pImplicitArgs->localSizeX, pImplicitArgs->localSizeY, pImplicitArgs->localSizeZ};
        auto generationOfLocalIdsByRuntime = EncodeDispatchKernel<GfxFamily>::isRuntimeLocalIdsGenerationRequired(
            3,
            localWorkSize,
            std::array<uint8_t, 3>{
                {kernelAttributes.workgroupWalkOrder[0],
                 kernelAttributes.workgroupWalkOrder[1],
                 kernelAttributes.workgroupWalkOrder[2]}},
            kernelAttributes.flags.requiresWorkgroupWalkOrder,
            requiredWalkOrder,
            kernelDescriptor.kernelAttributes.simdSize);

        auto sizeForImplicitArgsProgramming = ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernelDescriptor, !generationOfLocalIdsByRuntime, rootDeviceEnvironment);

        auto sizeForLocalIdsProgramming = sizeForImplicitArgsProgramming - NEO::ImplicitArgsHelper::getSizeForImplicitArgsStruct(pImplicitArgs, kernelDescriptor, true, rootDeviceEnvironment);
        offsetCrossThreadData += sizeForLocalIdsProgramming;

        auto ptrToPatchImplicitArgs = indirectHeap.getSpace(sizeForImplicitArgsProgramming);
        EncodeDispatchKernel<GfxFamily>::template patchScratchAddressInImplicitArgs<heaplessModeEnabled>(*pImplicitArgs, scratchAddress, true);

        ImplicitArgsHelper::patchImplicitArgs(ptrToPatchImplicitArgs, *pImplicitArgs, kernelDescriptor, std::make_pair(generationOfLocalIdsByRuntime, requiredWalkOrder), rootDeviceEnvironment, nullptr);
    }

    uint32_t sizeToCopy = sizeCrossThreadData;
    if (inlineDataProgrammingRequired == true) {

        constexpr uint32_t inlineDataSize = WalkerType::getInlineDataSize();
        sizeToCopy = std::min(inlineDataSize, sizeCrossThreadData);
        dest = reinterpret_cast<char *>(walkerCmd->getInlineDataPointer());
        memcpy_s(dest, sizeToCopy, kernel.getCrossThreadData(), sizeToCopy);
        auto offset = std::min(inlineDataSize, sizeCrossThreadData);
        sizeCrossThreadData -= offset;
        src += offset;

        if constexpr (heaplessModeEnabled) {
            uint64_t indirectDataAddress = indirectHeap.getHeapGpuBase();
            indirectDataAddress += indirectHeap.getHeapGpuStartOffset();
            indirectDataAddress += offsetCrossThreadData;
            HardwareCommandsHelper<GfxFamily>::programInlineData<WalkerType>(kernel, walkerCmd, indirectDataAddress, scratchAddress);
        }
    }

    if (sizeCrossThreadData > 0) {
        dest = static_cast<char *>(indirectHeap.getSpace(sizeCrossThreadData));
        memcpy_s(dest, sizeCrossThreadData, src, sizeCrossThreadData);
    }

    if (debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        FlatBatchBufferHelper::fixCrossThreadDataInfo(kernel.getPatchInfoDataList(), offsetCrossThreadData, indirectHeap.getGraphicsAllocation()->getGpuAddress());
    }

    return offsetCrossThreadData + static_cast<size_t>(is64bit ? indirectHeap.getHeapGpuStartOffset() : indirectHeap.getHeapGpuBase());
}

template <typename GfxFamily>
template <typename WalkerType>
void HardwareCommandsHelper<GfxFamily>::setInterfaceDescriptorOffset(
    WalkerType *walkerCmd,
    uint32_t &interfaceDescriptorIndex) {
}

} // namespace NEO
