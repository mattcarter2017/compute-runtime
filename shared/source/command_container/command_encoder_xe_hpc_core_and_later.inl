/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/utilities/lookup_array.h"

namespace NEO {

template <typename Family>
size_t EncodeMemoryFence<Family>::getSystemMemoryFenceSize() {
    return sizeof(typename Family::STATE_SYSTEM_MEM_FENCE_ADDRESS);
}

template <typename Family>
void EncodeMemoryFence<Family>::encodeSystemMemoryFence(LinearStream &commandStream, const GraphicsAllocation *globalFenceAllocation) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename Family::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto stateSystemFenceAddressSpace = commandStream.getSpaceForCmd<STATE_SYSTEM_MEM_FENCE_ADDRESS>();
    STATE_SYSTEM_MEM_FENCE_ADDRESS stateSystemFenceAddress = Family::cmdInitStateSystemMemFenceAddress;
    stateSystemFenceAddress.setSystemMemoryFenceAddress(globalFenceAllocation->getGpuAddress());
    *stateSystemFenceAddressSpace = stateSystemFenceAddress;
}

template <typename Family>
void EncodeBatchBufferStartOrEnd<Family>::appendBatchBufferStart(MI_BATCH_BUFFER_START &cmd, bool indirect, bool predicate) {
    cmd.setIndirectAddressEnable(indirect);
    cmd.setPredicationEnable(predicate);
}

template <typename Family>
inline void EncodeAtomic<Family>::setMiAtomicAddress(MI_ATOMIC &atomic, uint64_t writeAddress) {
    atomic.setMemoryAddress(writeAddress);
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::programBarrierEnable(InterfaceDescriptorType &interfaceDescriptor,
                                                        const KernelDescriptor &kernelDescriptor,
                                                        const HardwareInfo &hwInfo) {
    using BARRIERS = typename InterfaceDescriptorType::NUMBER_OF_BARRIERS;
    static const LookupArray<uint32_t, BARRIERS, 8> barrierLookupArray({{{0, BARRIERS::NUMBER_OF_BARRIERS_NONE},
                                                                         {1, BARRIERS::NUMBER_OF_BARRIERS_B1},
                                                                         {2, BARRIERS::NUMBER_OF_BARRIERS_B2},
                                                                         {4, BARRIERS::NUMBER_OF_BARRIERS_B4},
                                                                         {8, BARRIERS::NUMBER_OF_BARRIERS_B8},
                                                                         {16, BARRIERS::NUMBER_OF_BARRIERS_B16},
                                                                         {24, BARRIERS::NUMBER_OF_BARRIERS_B24},
                                                                         {32, BARRIERS::NUMBER_OF_BARRIERS_B32}}});
    BARRIERS numBarriers = barrierLookupArray.lookUp(kernelDescriptor.kernelAttributes.barrierCount);
    interfaceDescriptor.setNumberOfBarriers(numBarriers);
}

} // namespace NEO