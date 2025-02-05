/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_reuse_cleaner.h"

#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/os_thread.h"

#include <thread>
namespace NEO {

UnifiedMemoryReuseCleaner::UnifiedMemoryReuseCleaner() {
}

UnifiedMemoryReuseCleaner::~UnifiedMemoryReuseCleaner() {
    UNRECOVERABLE_IF(this->unifiedMemoryReuseCleanerThread);
}

void UnifiedMemoryReuseCleaner::stopThread() {
    keepCleaning.store(false);
    runCleaning.store(false);
    if (unifiedMemoryReuseCleanerThread) {
        unifiedMemoryReuseCleanerThread->join();
        unifiedMemoryReuseCleanerThread.reset();
    }
};

void *UnifiedMemoryReuseCleaner::cleanUnifiedMemoryReuse(void *self) {
    auto cleaner = reinterpret_cast<UnifiedMemoryReuseCleaner *>(self);
    while (!cleaner->runCleaning.load()) {
        if (!cleaner->keepCleaning.load()) {
            return nullptr;
        }
        NEO::sleep(sleepTime);
    }

    while (true) {
        if (!cleaner->keepCleaning.load()) {
            return nullptr;
        }
        NEO::sleep(sleepTime);
        cleaner->trimOldInCaches();
    }
}

void UnifiedMemoryReuseCleaner::registerSvmAllocationCache(SvmAllocationCache *cache) {
    std::lock_guard<std::mutex> lockSvmAllocationCaches(this->svmAllocationCachesMutex);
    this->svmAllocationCaches.push_back(cache);
    this->startCleaning();
}

void UnifiedMemoryReuseCleaner::unregisterSvmAllocationCache(SvmAllocationCache *cache) {
    std::lock_guard<std::mutex> lockSvmAllocationCaches(this->svmAllocationCachesMutex);
    this->svmAllocationCaches.erase(std::find(this->svmAllocationCaches.begin(), this->svmAllocationCaches.end(), cache));
}

void UnifiedMemoryReuseCleaner::trimOldInCaches() {
    const std::chrono::high_resolution_clock::time_point trimTimePoint = std::chrono::high_resolution_clock::now() - maxHoldTime;
    std::lock_guard<std::mutex> lockSvmAllocationCaches(this->svmAllocationCachesMutex);
    for (auto svmAllocCache : this->svmAllocationCaches) {
        svmAllocCache->trimOldAllocs(trimTimePoint);
    }
}

void UnifiedMemoryReuseCleaner::startThread() {
    this->unifiedMemoryReuseCleanerThread = Thread::createFunc(cleanUnifiedMemoryReuse, reinterpret_cast<void *>(this));
}

} // namespace NEO