/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "opencl/source/command_queue/gpgpu_walker_xehp_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_xehp_and_later.inl"

namespace NEO {

using Family = XeHpcCoreFamily;

template <>
template <>
void GpgpuWalkerHelper<Family>::setSystolicModeEnable(Family::COMPUTE_WALKER *walkerCmd) {
    if (debugManager.flags.OverrideSystolicInComputeWalker.get() != -1) {
        walkerCmd->setSystolicModeEnable((debugManager.flags.OverrideSystolicInComputeWalker.get()));
    }
}

template class GpgpuWalkerHelper<Family>;
template void GpgpuWalkerHelper<Family>::setupTimestampPacket<Family::DefaultWalkerType>(LinearStream *cmdStream, Family::DefaultWalkerType *walkerCmd, TagNodeBase *timestampPacketNode, const RootDeviceEnvironment &rootDeviceEnvironment);
template size_t GpgpuWalkerHelper<Family>::setGpgpuWalkerThreadData<Family::DefaultWalkerType>(Family::DefaultWalkerType *walkerCmd, const KernelDescriptor &kernelDescriptor, const size_t startWorkGroups[3],
                                                                                               const size_t numWorkGroups[3], const size_t localWorkSizesIn[3], uint32_t simd, uint32_t workDim, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, uint32_t requiredWorkGroupOrder);

template class HardwareInterface<Family>;

template void HardwareInterface<Family>::dispatchWalker<Family::DefaultWalkerType>(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, const CsrDependencies &csrDependencies, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::programWalker<Family::DefaultWalkerType>(LinearStream &commandStream, Kernel &kernel, CommandQueue &commandQueue, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, const DispatchInfo &dispatchInfo, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::dispatchKernelCommands<Family::DefaultWalkerType>(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, LinearStream &commandStream, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, HardwareInterfaceWalkerArgs &walkerArgs);
template Family::DefaultWalkerType *HardwareInterface<Family>::allocateWalkerSpace<Family::DefaultWalkerType>(LinearStream &commandStream, const Kernel &kernel);

template struct EnqueueOperation<Family>;

} // namespace NEO
