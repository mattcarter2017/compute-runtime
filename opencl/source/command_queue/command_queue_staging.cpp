/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/staging_buffer_manager.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/command_queue/csr_selection_args.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/mem_obj/image.h"

#include "CL/cl_ext.h"

namespace NEO {

cl_int CommandQueue::enqueueStagingBufferMemcpy(cl_bool blockingCopy, void *dstPtr, const void *srcPtr, size_t size, cl_event *event) {
    CsrSelectionArgs csrSelectionArgs{CL_COMMAND_SVM_MEMCPY, &size};
    csrSelectionArgs.direction = TransferDirection::hostToLocal;
    auto csr = &selectCsrForBuiltinOperation(csrSelectionArgs);
    cl_event profilingEvent = nullptr;

    bool isSingleTransfer = false;
    ChunkCopyFunction chunkCopy = [&](void *chunkSrc, void *chunkDst, size_t chunkSize) -> int32_t {
        auto isFirstTransfer = (chunkDst == dstPtr);
        auto isLastTransfer = ptrOffset(chunkDst, chunkSize) == ptrOffset(dstPtr, size);
        isSingleTransfer = isFirstTransfer && isLastTransfer;
        cl_event *outEvent = assignEventForStaging(event, &profilingEvent, isFirstTransfer, isLastTransfer);

        auto ret = this->enqueueSVMMemcpy(false, chunkDst, chunkSrc, chunkSize, 0, nullptr, outEvent, csr);
        ret |= this->flush();
        return ret;
    };

    auto stagingBufferManager = this->context->getStagingBufferManager();
    auto ret = stagingBufferManager->performCopy(dstPtr, srcPtr, size, chunkCopy, csr);
    return postStagingTransferSync(ret, event, profilingEvent, isSingleTransfer, blockingCopy, CL_COMMAND_SVM_MEMCPY);
}

cl_int CommandQueue::enqueueStagingImageTransfer(cl_command_type commandType, Image *image, cl_bool blockingCopy, const size_t *globalOrigin, const size_t *globalRegion,
                                                 size_t inputRowPitch, size_t inputSlicePitch, const void *ptr, cl_event *event) {
    auto isRead = commandType == CL_COMMAND_READ_IMAGE;
    CsrSelectionArgs csrSelectionArgs{commandType, isRead ? image : nullptr, isRead ? nullptr : image, this->getDevice().getRootDeviceIndex(), globalRegion, nullptr, globalOrigin};
    auto &csr = selectCsrForBuiltinOperation(csrSelectionArgs);
    cl_event profilingEvent = nullptr;

    bool isSingleTransfer = false;
    ChunkTransferImageFunc chunkWrite = [&](void *stagingBuffer, const size_t *origin, const size_t *region) -> int32_t {
        auto isFirstTransfer = (globalOrigin[1] == origin[1]);
        auto isLastTransfer = (globalOrigin[1] + globalRegion[1] == origin[1] + region[1]);
        isSingleTransfer = isFirstTransfer && isLastTransfer;
        cl_event *outEvent = assignEventForStaging(event, &profilingEvent, isFirstTransfer, isLastTransfer);
        cl_int ret = 0;
        if (isRead) {
            ret = this->enqueueReadImageImpl(image, false, origin, region, inputRowPitch, inputSlicePitch, stagingBuffer, nullptr, 0, nullptr, outEvent, csr);
        } else {
            ret = this->enqueueWriteImageImpl(image, false, origin, region, inputRowPitch, inputSlicePitch, stagingBuffer, nullptr, 0, nullptr, outEvent, csr);
        }
        ret |= this->flush();
        return ret;
    };
    auto bytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes;
    auto dstRowPitch = inputRowPitch ? inputRowPitch : globalRegion[0] * bytesPerPixel;

    auto stagingBufferManager = this->context->getStagingBufferManager();
    auto ret = stagingBufferManager->performImageTransfer(ptr, globalOrigin, globalRegion, dstRowPitch, chunkWrite, &csr, isRead);
    return postStagingTransferSync(ret, event, profilingEvent, isSingleTransfer, blockingCopy, commandType);
}

cl_int CommandQueue::enqueueStagingWriteBuffer(Buffer *buffer, cl_bool blockingCopy, size_t offset, size_t size, const void *ptr, cl_event *event) {
    CsrSelectionArgs csrSelectionArgs{CL_COMMAND_WRITE_BUFFER, {}, buffer, this->getDevice().getRootDeviceIndex(), &size};
    CommandStreamReceiver &csr = selectCsrForBuiltinOperation(csrSelectionArgs);
    cl_event profilingEvent = nullptr;

    bool isSingleTransfer = false;
    ChunkTransferBufferFunc chunkWrite = [&](void *stagingBuffer, size_t chunkOffset, size_t chunkSize) -> int32_t {
        auto isFirstTransfer = (chunkOffset == offset);
        auto isLastTransfer = (offset + size == chunkOffset + chunkSize);
        isSingleTransfer = isFirstTransfer && isLastTransfer;
        cl_event *outEvent = assignEventForStaging(event, &profilingEvent, isFirstTransfer, isLastTransfer);

        auto ret = this->enqueueWriteBufferImpl(buffer, false, chunkOffset, chunkSize, stagingBuffer, nullptr, 0, nullptr, outEvent, csr);
        ret |= this->flush();
        return ret;
    };
    auto stagingBufferManager = this->context->getStagingBufferManager();
    auto ret = stagingBufferManager->performBufferTransfer(ptr, offset, size, chunkWrite, &csr, false);
    return postStagingTransferSync(ret, event, profilingEvent, isSingleTransfer, blockingCopy, CL_COMMAND_WRITE_BUFFER);
}

/*
 * If there's single transfer, use user event.
 * Otherwise, first transfer uses profiling event to obtain queue/submit/start timestamps.
 * Last transfer uses user event in case of IOQ.
 * For OOQ user event will be passed to barrier to gather all submitted transfers.
 */
cl_event *CommandQueue::assignEventForStaging(cl_event *userEvent, cl_event *profilingEvent, bool isFirstTransfer, bool isLastTransfer) const {
    cl_event *outEvent = nullptr;
    if (userEvent != nullptr) {
        if (isFirstTransfer && isProfilingEnabled()) {
            outEvent = profilingEvent;
        } else if (isLastTransfer && !this->isOOQEnabled()) {
            outEvent = userEvent;
        }
    }
    if (isFirstTransfer && isLastTransfer) {
        outEvent = userEvent;
    }
    return outEvent;
}

cl_int CommandQueue::postStagingTransferSync(const StagingTransferStatus &status, cl_event *event, const cl_event profilingEvent, bool isSingleTransfer, bool isBlocking, cl_command_type commandType) {
    if (status.waitStatus == WaitStatus::gpuHang) {
        return CL_OUT_OF_RESOURCES;
    } else if (status.chunkCopyStatus != CL_SUCCESS) {
        return status.chunkCopyStatus;
    }

    cl_int ret = CL_SUCCESS;
    if (event != nullptr) {
        if (!isSingleTransfer && this->isOOQEnabled()) {
            ret = this->enqueueBarrierWithWaitList(0, nullptr, event);
        }
        auto pEvent = castToObjectOrAbort<Event>(*event);
        if (!isSingleTransfer && isProfilingEnabled()) {
            auto pProfilingEvent = castToObjectOrAbort<Event>(profilingEvent);
            pEvent->copyTimestamps(*pProfilingEvent);
            pProfilingEvent->release();
        }
        pEvent->setCmdType(commandType);
    }

    if (isBlocking) {
        ret = this->finish();
    }
    return ret;
}

bool CommandQueue::isValidForStagingBufferCopy(Device &device, void *dstPtr, const void *srcPtr, size_t size, bool hasDependencies) {
    GraphicsAllocation *allocation = nullptr;
    context->tryGetExistingMapAllocation(srcPtr, size, allocation);
    if (allocation != nullptr) {
        // Direct transfer from mapped allocation is faster than staging buffer
        return false;
    }
    CsrSelectionArgs csrSelectionArgs{CL_COMMAND_SVM_MEMCPY, nullptr};
    csrSelectionArgs.direction = TransferDirection::hostToLocal;
    auto csr = &selectCsrForBuiltinOperation(csrSelectionArgs);
    auto osContextId = csr->getOsContext().getContextId();
    auto stagingBufferManager = context->getStagingBufferManager();
    UNRECOVERABLE_IF(stagingBufferManager == nullptr);
    return stagingBufferManager->isValidForCopy(device, dstPtr, srcPtr, size, hasDependencies, osContextId);
}

bool CommandQueue::isValidForStagingTransfer(MemObj *memObj, const void *ptr, bool hasDependencies) {
    GraphicsAllocation *allocation = nullptr;
    context->tryGetExistingMapAllocation(ptr, memObj->getSize(), allocation);
    if (allocation != nullptr) {
        // Direct transfer from mapped allocation is faster than staging buffer
        return false;
    }
    auto stagingBufferManager = context->getStagingBufferManager();
    if (!stagingBufferManager) {
        return false;
    }
    switch (memObj->peekClMemObjType()) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_BUFFER:
        return stagingBufferManager->isValidForStagingTransfer(this->getDevice(), ptr, memObj->getSize(), hasDependencies);
    default:
        return false;
    }
}

} // namespace NEO
