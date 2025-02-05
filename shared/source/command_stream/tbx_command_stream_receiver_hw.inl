/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"
#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/source/aub_mem_dump/aub_alloc_dump.inl"
#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/aub_memory_operations_handler.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/page_fault_manager/tbx_page_fault_manager.h"

#include <cstring>

namespace NEO {

template <typename GfxFamily>
CpuPageFaultManager *TbxCommandStreamReceiverHw<GfxFamily>::getTbxPageFaultManager() {
    return this->getMemoryManager()->getPageFaultManager();
}

template <typename GfxFamily>
TbxCommandStreamReceiverHw<GfxFamily>::TbxCommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment,
                                                                  uint32_t rootDeviceIndex,
                                                                  const DeviceBitfield deviceBitfield)
    : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield) {

    forceSkipResourceCleanupRequired = true;

    auto releaseHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper();
    physicalAddressAllocator.reset(this->createPhysicalAddressAllocator(&this->peekHwInfo(), releaseHelper));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initAubCenter(this->localMemoryEnabled, "", this->getType());
    auto aubCenter = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter.get();
    UNRECOVERABLE_IF(nullptr == aubCenter);

    aubManager = aubCenter->getAubManager();

    ppgtt = std::make_unique<std::conditional<is64bit, PML4, PDPE>::type>(physicalAddressAllocator.get());
    ggtt = std::make_unique<PDPE>(physicalAddressAllocator.get());

    auto debugDeviceId = debugManager.flags.OverrideAubDeviceId.get();
    this->aubDeviceId = debugDeviceId == -1
                            ? this->peekHwInfo().capabilityTable.aubDeviceId
                            : static_cast<uint32_t>(debugDeviceId);
    this->stream = &tbxStream;
    this->downloadAllocationImpl = [this](GraphicsAllocation &graphicsAllocation) {
        this->downloadAllocationTbx(graphicsAllocation);
    };
}

template <typename GfxFamily>
TbxCommandStreamReceiverHw<GfxFamily>::~TbxCommandStreamReceiverHw() {
    this->downloadAllocationImpl = nullptr;

    if (streamInitialized) {
        tbxStream.close();
    }

    this->freeEngineInfo(gttRemap);
}

template <typename GfxFamily>
bool TbxCommandStreamReceiverHw<GfxFamily>::isAllocTbxFaultable(GraphicsAllocation *gfxAlloc) {
    // indicates host memory not managed by the driver
    if (gfxAlloc->getDriverAllocatedCpuPtr() == nullptr || !debugManager.isTbxPageFaultManagerEnabled() || this->getTbxPageFaultManager() == nullptr) {
        return false;
    }
    auto allocType = gfxAlloc->getAllocationType();
    return AubHelper::isOneTimeAubWritableAllocationType(allocType) && GraphicsAllocation::isLockable(allocType);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::registerAllocationWithTbxFaultMngrIfTbxFaultable(GraphicsAllocation *gfxAlloc, void *cpuAddress, size_t size) {
    if (!isAllocTbxFaultable(gfxAlloc)) {
        return;
    }
    auto bank = this->getMemoryBank(gfxAlloc);
    if (bank == 0u || gfxAlloc->storageInfo.cloningOfPageTables) {
        bank = GraphicsAllocation::defaultBank;
    }
    auto faultManager = getTbxPageFaultManager();
    faultManager->insertAllocation(this, gfxAlloc, bank, cpuAddress, size);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::allowCPUMemoryAccessIfTbxFaultable(GraphicsAllocation *gfxAlloc, void *cpuAddress, size_t size) {
    if (!isAllocTbxFaultable(gfxAlloc)) {
        return;
    }
    auto faultManager = getTbxPageFaultManager();
    faultManager->allowCPUMemoryAccess(cpuAddress, size);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::protectCPUMemoryAccessIfTbxFaultable(GraphicsAllocation *gfxAlloc, void *cpuAddress, size_t size) {
    if (!isAllocTbxFaultable(gfxAlloc)) {
        return;
    }
    auto faultManager = getTbxPageFaultManager();
    faultManager->protectCPUMemoryAccess(cpuAddress, size);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::protectCPUMemoryFromWritesIfTbxFaultable(GraphicsAllocation *gfxAlloc, void *cpuAddress, size_t size) {
    if (!isAllocTbxFaultable(gfxAlloc)) {
        return;
    }
    auto faultManager = getTbxPageFaultManager();
    faultManager->protectCpuMemoryFromWrites(cpuAddress, size);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::initializeEngine() {
    isEngineInitialized = true;

    if (hardwareContextController) {
        hardwareContextController->initialize();
        return;
    }
    DEBUG_BREAK_IF(this->aubManager);

    auto csTraits = this->getCsTraits(osContext->getEngineType());

    if (engineInfo.pLRCA) {
        return;
    }

    this->initGlobalMMIO();
    this->initEngineMMIO();
    this->initAdditionalMMIO();

    // Global HW Status Page
    {
        const size_t sizeHWSP = 0x1000;
        const size_t alignHWSP = 0x1000;
        engineInfo.pGlobalHWStatusPage = alignedMalloc(sizeHWSP, alignHWSP);
        engineInfo.ggttHWSP = gttRemap.map(engineInfo.pGlobalHWStatusPage, sizeHWSP);
        auto physHWSP = ggtt->map(engineInfo.ggttHWSP, sizeHWSP, this->getGTTBits(), this->getMemoryBankForGtt());

        // Write our GHWSP
        AubGTTData data = {0};
        this->getGTTData(reinterpret_cast<void *>(physHWSP), data);
        AUB::reserveAddressGGTT(tbxStream, engineInfo.ggttHWSP, sizeHWSP, physHWSP, data);
        tbxStream.writeMMIO(AubMemDump::computeRegisterOffset(csTraits.mmioBase, 0x2080), engineInfo.ggttHWSP);
    }

    // Allocate the LRCA
    const size_t sizeLRCA = csTraits.sizeLRCA;
    const size_t alignLRCA = csTraits.alignLRCA;
    auto pLRCABase = alignedMalloc(sizeLRCA, alignLRCA);
    engineInfo.pLRCA = pLRCABase;

    // Initialize the LRCA to a known state
    csTraits.initialize(pLRCABase);

    // Reserve the RCS ring buffer
    engineInfo.sizeRingBuffer = 0x4 * 0x1000;
    {
        const size_t alignRCS = 0x1000;
        engineInfo.pRingBuffer = alignedMalloc(engineInfo.sizeRingBuffer, alignRCS);
        engineInfo.ggttRingBuffer = gttRemap.map(engineInfo.pRingBuffer, engineInfo.sizeRingBuffer);
        auto physRCS = ggtt->map(engineInfo.ggttRingBuffer, engineInfo.sizeRingBuffer, this->getGTTBits(), this->getMemoryBankForGtt());

        AubGTTData data = {0};
        this->getGTTData(reinterpret_cast<void *>(physRCS), data);
        AUB::reserveAddressGGTT(tbxStream, engineInfo.ggttRingBuffer, engineInfo.sizeRingBuffer, physRCS, data);
    }

    // Initialize the ring MMIO registers
    {
        uint32_t ringHead = 0x000;
        uint32_t ringTail = 0x000;
        auto ringBase = engineInfo.ggttRingBuffer;
        auto ringCtrl = (uint32_t)((engineInfo.sizeRingBuffer - 0x1000) | 1);
        csTraits.setRingHead(pLRCABase, ringHead);
        csTraits.setRingTail(pLRCABase, ringTail);
        csTraits.setRingBase(pLRCABase, ringBase);
        csTraits.setRingCtrl(pLRCABase, ringCtrl);
    }

    // Write our LRCA
    {
        engineInfo.ggttLRCA = gttRemap.map(engineInfo.pLRCA, sizeLRCA);
        auto lrcAddressPhys = ggtt->map(engineInfo.ggttLRCA, sizeLRCA, this->getGTTBits(), this->getMemoryBankForGtt());

        AubGTTData data = {0};
        this->getGTTData(reinterpret_cast<void *>(lrcAddressPhys), data);
        AUB::reserveAddressGGTT(tbxStream, engineInfo.ggttLRCA, sizeLRCA, lrcAddressPhys, data);
        AUB::addMemoryWrite(
            tbxStream,
            lrcAddressPhys,
            pLRCABase,
            sizeLRCA,
            this->getAddressSpace(csTraits.aubHintLRCA),
            csTraits.aubHintLRCA);
    }

    DEBUG_BREAK_IF(!engineInfo.pLRCA);
}

template <typename GfxFamily>
CommandStreamReceiver *TbxCommandStreamReceiverHw<GfxFamily>::create(const std::string &baseName,
                                                                     bool withAubDump,
                                                                     ExecutionEnvironment &executionEnvironment,
                                                                     uint32_t rootDeviceIndex,
                                                                     const DeviceBitfield deviceBitfield) {
    TbxCommandStreamReceiverHw<GfxFamily> *csr;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    if (withAubDump) {
        auto localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(hwInfo);
        auto fullName = AUBCommandStreamReceiver::createFullFilePath(hwInfo, baseName, rootDeviceIndex);
        if (debugManager.flags.AUBDumpCaptureFileName.get() != "unk") {
            fullName.assign(debugManager.flags.AUBDumpCaptureFileName.get());
        }
        rootDeviceEnvironment.initAubCenter(localMemoryEnabled, fullName, CommandStreamReceiverType::tbxWithAub);

        csr = new CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<GfxFamily>>(baseName, executionEnvironment, rootDeviceIndex, deviceBitfield);

        auto aubCenter = rootDeviceEnvironment.aubCenter.get();
        UNRECOVERABLE_IF(nullptr == aubCenter);

        auto subCaptureCommon = aubCenter->getSubCaptureCommon();
        UNRECOVERABLE_IF(nullptr == subCaptureCommon);

        if (subCaptureCommon->subCaptureMode > AubSubCaptureManager::SubCaptureMode::off) {
            csr->subCaptureManager = std::make_unique<AubSubCaptureManager>(fullName, *subCaptureCommon, ApiSpecificConfig::getRegistryPath());
        }

        if (csr->aubManager) {
            if (!csr->aubManager->isOpen()) {
                csr->aubManager->open(csr->subCaptureManager ? csr->subCaptureManager->getSubCaptureFileName("") : fullName);
                UNRECOVERABLE_IF(!csr->aubManager->isOpen());
            }
        }
    } else {
        csr = new TbxCommandStreamReceiverHw<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield);
    }

    if (!csr->aubManager) {
        // Open our stream
        csr->stream->open(nullptr);

        // Add the file header.
        bool streamInitialized = csr->stream->init(productHelper.getAubStreamSteppingFromHwRevId(hwInfo), csr->aubDeviceId);
        csr->streamInitialized = streamInitialized;
    }
    return csr;
}

template <typename GfxFamily>
SubmissionStatus TbxCommandStreamReceiverHw<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    if (subCaptureManager) {
        if (aubManager) {
            aubManager->pause(false);
        }
    }

    initializeEngine();

    // Write our batch buffer
    auto pBatchBuffer = ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset);
    auto batchBufferGpuAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);
    auto currentOffset = batchBuffer.usedSize;
    DEBUG_BREAK_IF(currentOffset < batchBuffer.startOffset);
    auto sizeBatchBuffer = currentOffset - batchBuffer.startOffset;
    auto overrideRingHead = false;

    auto submissionTaskCount = this->taskCount + 1;
    allocationsForResidency.push_back(batchBuffer.commandBufferAllocation);
    batchBuffer.commandBufferAllocation->updateResidencyTaskCount(submissionTaskCount, this->osContext->getContextId());
    batchBuffer.commandBufferAllocation->updateTaskCount(submissionTaskCount, osContext->getContextId());

    // Write allocations for residency
    processResidency(allocationsForResidency, 0u);

    if (subCaptureManager) {
        if (aubManager) {
            auto status = subCaptureManager->getSubCaptureStatus();
            if (!status.wasActiveInPreviousEnqueue && status.isActive) {
                overrideRingHead = true;
            }
            if (!status.wasActiveInPreviousEnqueue && !status.isActive) {
                aubManager->pause(true);
            }
        }
    }

    submitBatchBufferTbx(
        batchBufferGpuAddress, pBatchBuffer, sizeBatchBuffer,
        this->getMemoryBank(batchBuffer.commandBufferAllocation),
        this->getPPGTTAdditionalBits(batchBuffer.commandBufferAllocation),
        overrideRingHead);

    if (subCaptureManager) {
        pollForCompletion();
        subCaptureManager->disableSubCapture();
    }

    return SubmissionStatus::success;
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::submitBatchBufferTbx(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits, bool overrideRingHead) {
    if (hardwareContextController) {
        if (batchBufferSize) {
            hardwareContextController->submit(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, MemoryConstants::pageSize64k, overrideRingHead);
        }
        return;
    }

    auto csTraits = this->getCsTraits(osContext->getEngineType());

    {
        auto physBatchBuffer = ppgtt->map(static_cast<uintptr_t>(batchBufferGpuAddress), batchBufferSize, entryBits, memoryBank);

        AubHelperHw<GfxFamily> aubHelperHw(this->localMemoryEnabled);
        AUB::reserveAddressPPGTT(tbxStream, static_cast<uintptr_t>(batchBufferGpuAddress), batchBufferSize, physBatchBuffer,
                                 entryBits, aubHelperHw);

        AUB::addMemoryWrite(
            tbxStream,
            physBatchBuffer,
            batchBuffer,
            batchBufferSize,
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary),
            AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary);
    }

    // Add a batch buffer start to the RCS
    auto previousTail = engineInfo.tailRingBuffer;
    {
        typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
        typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
        typedef typename GfxFamily::MI_NOOP MI_NOOP;

        auto pTail = ptrOffset(engineInfo.pRingBuffer, engineInfo.tailRingBuffer);
        auto ggttTail = ptrOffset(engineInfo.ggttRingBuffer, engineInfo.tailRingBuffer);

        auto sizeNeeded =
            sizeof(MI_BATCH_BUFFER_START) +
            sizeof(MI_NOOP) +
            sizeof(MI_LOAD_REGISTER_IMM);
        if (engineInfo.tailRingBuffer + sizeNeeded >= engineInfo.sizeRingBuffer) {
            // Pad the remaining ring with NOOPs
            auto sizeToWrap = engineInfo.sizeRingBuffer - engineInfo.tailRingBuffer;
            memset(pTail, 0, sizeToWrap);
            // write remaining ring
            auto physDumpStart = ggtt->map(ggttTail, sizeToWrap, this->getGTTBits(), this->getMemoryBankForGtt());
            AUB::addMemoryWrite(
                tbxStream,
                physDumpStart,
                pTail,
                sizeToWrap,
                this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceCommandBuffer),
                AubMemDump::DataTypeHintValues::TraceCommandBuffer);
            previousTail = 0;
            engineInfo.tailRingBuffer = 0;
            pTail = engineInfo.pRingBuffer;
        } else if (engineInfo.tailRingBuffer == 0) {
            // Add a LRI if this is our first submission
            auto lri = GfxFamily::cmdInitLoadRegisterImm;
            lri.setRegisterOffset(AubMemDump::computeRegisterOffset(csTraits.mmioBase, 0x2244));
            lri.setDataDword(0x00010000);
            *(MI_LOAD_REGISTER_IMM *)pTail = lri;
            pTail = ((MI_LOAD_REGISTER_IMM *)pTail) + 1;
        }

        // Add our BBS
        auto bbs = GfxFamily::cmdInitBatchBufferStart;
        bbs.setBatchBufferStartAddress(static_cast<uint64_t>(batchBufferGpuAddress));
        bbs.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
        *(MI_BATCH_BUFFER_START *)pTail = bbs;
        pTail = ((MI_BATCH_BUFFER_START *)pTail) + 1;

        // Add a NOOP as our tail needs to be aligned to a QWORD
        *(MI_NOOP *)pTail = GfxFamily::cmdInitNoop;
        pTail = ((MI_NOOP *)pTail) + 1;

        // Compute our new ring tail.
        engineInfo.tailRingBuffer = (uint32_t)ptrDiff(pTail, engineInfo.pRingBuffer);

        // Only dump the new commands
        auto ggttDumpStart = ptrOffset(engineInfo.ggttRingBuffer, previousTail);
        auto dumpStart = ptrOffset(engineInfo.pRingBuffer, previousTail);
        auto dumpLength = engineInfo.tailRingBuffer - previousTail;

        // write RCS
        auto physDumpStart = ggtt->map(ggttDumpStart, dumpLength, this->getGTTBits(), this->getMemoryBankForGtt());
        AUB::addMemoryWrite(
            tbxStream,
            physDumpStart,
            dumpStart,
            dumpLength,
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceCommandBuffer),
            AubMemDump::DataTypeHintValues::TraceCommandBuffer);

        // update the RCS mmio tail in the LRCA
        auto physLRCA = ggtt->map(engineInfo.ggttLRCA, sizeof(engineInfo.tailRingBuffer), this->getGTTBits(), this->getMemoryBankForGtt());
        AUB::addMemoryWrite(
            tbxStream,
            physLRCA + 0x101c,
            &engineInfo.tailRingBuffer,
            sizeof(engineInfo.tailRingBuffer),
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype));

        DEBUG_BREAK_IF(engineInfo.tailRingBuffer >= engineInfo.sizeRingBuffer);
    }

    // Submit our execlist by submitting to the execlist submit ports
    {
        typename AUB::MiContextDescriptorReg contextDescriptor = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

        contextDescriptor.sData.valid = true;
        contextDescriptor.sData.forcePageDirRestore = false;
        contextDescriptor.sData.forceRestore = false;
        contextDescriptor.sData.legacy = true;
        contextDescriptor.sData.faultSupport = 0;
        contextDescriptor.sData.privilegeAccessOrPPGTT = true;
        contextDescriptor.sData.aDor64bitSupport = AUB::Traits::addressingBits > 32;

        auto ggttLRCA = engineInfo.ggttLRCA;
        contextDescriptor.sData.logicalRingCtxAddress = ggttLRCA / 4096;
        contextDescriptor.sData.contextID = 0;

        this->submitLRCA(contextDescriptor);
    }
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::pollForCompletion(bool skipTaskCountCheck) {
    if (hardwareContextController) {
        hardwareContextController->pollForCompletion();
        return;
    }

    typedef typename AubMemDump::CmdServicesMemTraceRegisterPoll CmdServicesMemTraceRegisterPoll;

    auto mmioBase = this->getCsTraits(osContext->getEngineType()).mmioBase;
    bool pollNotEqual = getpollNotEqualValueForPollForCompletion();
    uint32_t mask = getMaskAndValueForPollForCompletion();
    uint32_t value = mask;
    tbxStream.registerPoll(
        AubMemDump::computeRegisterOffset(mmioBase, 0x2234), // EXECLIST_STATUS
        mask,
        value,
        pollNotEqual,
        CmdServicesMemTraceRegisterPoll::TimeoutActionValues::Abort);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) {
    UNRECOVERABLE_IF(!isEngineInitialized);

    AubHelperHw<GfxFamily> aubHelperHw(this->localMemoryEnabled);

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        AUB::reserveAddressGGTTAndWriteMmeory(tbxStream, static_cast<uintptr_t>(gpuAddress), cpuAddress, physAddress, size, offset, entryBits,
                                              aubHelperHw);
    };

    ppgtt->pageWalk(static_cast<uintptr_t>(gpuAddress), size, 0, entryBits, walker, memoryBank);
}

template <typename GfxFamily>
bool TbxCommandStreamReceiverHw<GfxFamily>::writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) {
    uint64_t gpuAddress;
    void *cpuAddress;
    size_t size;

    if (!this->getParametersForMemory(gfxAllocation, gpuAddress, cpuAddress, size)) {
        return false;
    }

    auto allocType = gfxAllocation.getAllocationType();
    this->registerAllocationWithTbxFaultMngrIfTbxFaultable(&gfxAllocation, cpuAddress, size);

    if (!this->isTbxWritable(gfxAllocation)) {
        return false;
    }

    this->allowCPUMemoryAccessIfTbxFaultable(&gfxAllocation, cpuAddress, size);

    if (!isEngineInitialized) {
        initializeEngine();
    }

    if (aubManager) {
        this->writeMemoryWithAubManager(gfxAllocation, isChunkCopy, gpuVaChunkOffset, chunkSize);
    } else {
        if (isChunkCopy) {
            gpuAddress += gpuVaChunkOffset;
            cpuAddress = ptrOffset(cpuAddress, static_cast<uintptr_t>(gpuVaChunkOffset));
            size = chunkSize;
        }
        writeMemory(gpuAddress, cpuAddress, size, this->getMemoryBank(&gfxAllocation), this->getPPGTTAdditionalBits(&gfxAllocation));
    }

    if (AubHelper::isOneTimeAubWritableAllocationType(allocType)) {
        this->setTbxWritable(false, gfxAllocation);
    }
    this->protectCPUMemoryAccessIfTbxFaultable(&gfxAllocation, cpuAddress, size);

    return true;
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::writeMMIO(uint32_t offset, uint32_t value) {
    if (hardwareContextController) {
        hardwareContextController->writeMMIO(offset, value);
    }
}

template <typename GfxFamily>
bool TbxCommandStreamReceiverHw<GfxFamily>::expectMemory(const void *gfxAddress, const void *srcAddress,
                                                         size_t length, uint32_t compareOperation) {
    if (hardwareContextController) {
        auto readMemory = std::make_unique<char[]>(length);
        // note: memory bank should not matter assuming that we call expect on the memory that was previously allocated
        hardwareContextController->readMemory((uint64_t)gfxAddress, readMemory.get(), length, this->getMemoryBankForGtt(), MemoryConstants::pageSize64k);
        auto isMemoryEqual = (memcmp(readMemory.get(), srcAddress, length) == 0);
        auto isEqualMemoryExpected = (compareOperation == AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual);
        return (isMemoryEqual == isEqualMemoryExpected);
    }

    return BaseClass::expectMemory(gfxAddress, srcAddress, length, compareOperation);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::flushSubmissionsAndDownloadAllocations(TaskCountType taskCountToWait, bool skipAllocationsDownload) {
    this->flushBatchedSubmissions();

    if (this->latestFlushedTaskCount < taskCountToWait) {
        this->flushTagUpdate();
    }

    volatile TagAddressType *pollAddress = this->getTagAddress();
    for (uint32_t i = 0; i < this->activePartitions; i++) {
        while (*pollAddress < this->latestFlushedTaskCount) {
            this->downloadAllocation(*this->getTagAllocation());
        }
        pollAddress = ptrOffset(pollAddress, this->immWritePostSyncWriteOffset);
    }

    if (skipAllocationsDownload) {
        return;
    }

    auto lockCSR = this->obtainUniqueOwnership();
    for (GraphicsAllocation *graphicsAllocation : this->allocationsForDownload) {
        this->downloadAllocation(*graphicsAllocation);
    }
    this->allocationsForDownload.clear();
}

template <typename GfxFamily>
WaitStatus TbxCommandStreamReceiverHw<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) {
    flushSubmissionsAndDownloadAllocations(taskCountToWait, false);
    return BaseClass::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, useQuickKmdSleep, throttle);
}

template <typename GfxFamily>
WaitStatus TbxCommandStreamReceiverHw<GfxFamily>::waitForCompletionWithTimeout(const WaitParams &params, TaskCountType taskCountToWait) {
    flushSubmissionsAndDownloadAllocations(taskCountToWait, params.skipTbxDownload);
    return BaseClass::waitForCompletionWithTimeout(params, taskCountToWait);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::processEviction() {
    this->allocationsForDownload.insert(this->getEvictionAllocations().begin(), this->getEvictionAllocations().end());
    BaseClass::processEviction();
}

template <typename GfxFamily>
SubmissionStatus TbxCommandStreamReceiverHw<GfxFamily>::processResidency(ResidencyContainer &allocationsForResidency, uint32_t handleId) {

    for (auto &gfxAllocation : allocationsForResidency) {
        if (dumpTbxNonWritable) {
            this->setTbxWritable(true, *gfxAllocation);
        }
        if (!writeMemory(*gfxAllocation)) {
            DEBUG_BREAK_IF(!((gfxAllocation->getUnderlyingBufferSize() == 0) ||
                             !this->isTbxWritable(*gfxAllocation)));
        }
        gfxAllocation->updateResidencyTaskCount(this->taskCount + 1, this->osContext->getContextId());
    }

    if (this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]->memoryOperationsInterface) {
        this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]->memoryOperationsInterface->processFlushResidency(this);
    }

    dumpTbxNonWritable = false;
    return SubmissionStatus::success;
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::downloadAllocationTbx(GraphicsAllocation &gfxAllocation) {

    uint64_t gpuAddress = 0;
    void *cpuAddress = nullptr;
    size_t size = 0;

    this->getParametersForMemory(gfxAllocation, gpuAddress, cpuAddress, size);

    this->allowCPUMemoryAccessIfTbxFaultable(&gfxAllocation, cpuAddress, size);

    if (hardwareContextController) {
        hardwareContextController->readMemory(gpuAddress, cpuAddress, size,
                                              this->getMemoryBank(&gfxAllocation), gfxAllocation.getUsedPageSize());
        this->protectCPUMemoryFromWritesIfTbxFaultable(&gfxAllocation, cpuAddress, size);
        return;
    }

    if (size) {
        PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
            DEBUG_BREAK_IF(offset > size);
            tbxStream.readMemory(physAddress, ptrOffset(cpuAddress, offset), size);
        };
        ppgtt->pageWalk(static_cast<uintptr_t>(gpuAddress), size, 0, 0, walker, this->getMemoryBank(&gfxAllocation));
    }
    this->protectCPUMemoryFromWritesIfTbxFaultable(&gfxAllocation, cpuAddress, size);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::downloadAllocations(bool blockingWait, TaskCountType taskCount) {
    volatile TagAddressType *pollAddress = this->getTagAddress();

    auto waitTaskCount = std::min(taskCount, this->latestFlushedTaskCount.load());

    for (uint32_t i = 0; i < this->activePartitions; i++) {
        if (*pollAddress < waitTaskCount) {
            this->downloadAllocation(*this->getTagAllocation());

            auto startTime = std::chrono::high_resolution_clock::now();
            uint64_t timeDiff = 0;

            while (*pollAddress < waitTaskCount) {
                if (!blockingWait) {
                    // Additional delay to reach PC in case of Event wait
                    timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
                    if (timeDiff > getNonBlockingDownloadTimeoutMs()) {
                        return;
                    }
                }
                this->downloadAllocation(*this->getTagAllocation());
            }
        }

        pollAddress = ptrOffset(pollAddress, this->immWritePostSyncWriteOffset);
    }
    auto lockCSR = this->obtainUniqueOwnership();

    std::vector<GraphicsAllocation *> notReadyAllocations;

    for (GraphicsAllocation *graphicsAllocation : this->allocationsForDownload) {
        this->downloadAllocation(*graphicsAllocation);

        // Used again while waiting for completion. Another download will be needed.
        if (graphicsAllocation->getTaskCount(this->osContext->getContextId()) > taskCount) {
            notReadyAllocations.push_back(graphicsAllocation);
        }
    }
    this->allocationsForDownload.clear();
    this->allocationsForDownload = std::set<GraphicsAllocation *>(notReadyAllocations.begin(), notReadyAllocations.end());
}

template <typename GfxFamily>
uint32_t TbxCommandStreamReceiverHw<GfxFamily>::getMaskAndValueForPollForCompletion() const {
    return 0x100;
}

template <typename GfxFamily>
bool TbxCommandStreamReceiverHw<GfxFamily>::getpollNotEqualValueForPollForCompletion() const {
    return false;
}

template <typename GfxFamily>
AubSubCaptureStatus TbxCommandStreamReceiverHw<GfxFamily>::checkAndActivateAubSubCapture(const std::string &kernelName) {
    if (!subCaptureManager) {
        return {false, false};
    }

    auto status = subCaptureManager->checkAndActivateSubCapture(kernelName);
    if (status.isActive && !status.wasActiveInPreviousEnqueue) {
        dumpTbxNonWritable = true;
    }
    return status;
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::dumpAllocation(GraphicsAllocation &gfxAllocation) {
    if (!hardwareContextController) {
        return;
    }

    bool isBcsCsr = EngineHelpers::isBcs(this->osContext->getEngineType());

    if (isBcsCsr != gfxAllocation.getAubInfo().bcsDumpOnly) {
        return;
    }

    if (debugManager.flags.AUBDumpAllocsOnEnqueueReadOnly.get() || debugManager.flags.AUBDumpAllocsOnEnqueueSVMMemcpyOnly.get()) {
        if (!gfxAllocation.isAllocDumpable()) {
            return;
        }
        gfxAllocation.setAllocDumpable(false, isBcsCsr);
    }

    auto dumpFormat = AubAllocDump::getDumpFormat(gfxAllocation);
    auto surfaceInfo = std::unique_ptr<aub_stream::SurfaceInfo>(AubAllocDump::getDumpSurfaceInfo<GfxFamily>(gfxAllocation, *this->peekGmmHelper(), dumpFormat));
    if (surfaceInfo) {
        hardwareContextController->pollForCompletion();
        hardwareContextController->dumpSurface(*surfaceInfo.get());
    }
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::removeDownloadAllocation(GraphicsAllocation *alloc) {
    auto lockCSR = this->obtainUniqueOwnership();

    this->allocationsForDownload.erase(alloc);

    auto faultManager = getTbxPageFaultManager();
    if (faultManager != nullptr) {
        faultManager->removeAllocation(alloc);
    }
}
} // namespace NEO
