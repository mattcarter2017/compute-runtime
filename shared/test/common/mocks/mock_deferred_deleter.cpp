/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_deferred_deleter.h"

#include "shared/source/memory_manager/deferrable_deletion.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/test/common/mocks/mock_deferrable_deletion.h"

#include "gtest/gtest.h"

namespace NEO {

MockDeferredDeleter::MockDeferredDeleter() {
    shouldStopCalled = 0;
    clearCalled = 0;
}

void MockDeferredDeleter::deferDeletion(DeferrableDeletion *deletion) {
    deferDeletionCalled++;
    deletion->apply();
    delete deletion;
}

void MockDeferredDeleter::addClient() {
    ++numClients;
}

void MockDeferredDeleter::removeClient() {
    --numClients;
}

void MockDeferredDeleter::drain(bool blocking, bool hostptrsOnly) {
    if (expectDrainCalled) {
        EXPECT_EQ(expectedDrainValue, blocking);
    }
    DeferredDeleter::drain(blocking, hostptrsOnly);
    drainCalled++;
}

void MockDeferredDeleter::drain() {
    return drain(true, false);
}

bool MockDeferredDeleter::areElementsReleased(bool hostptrsOnly) {
    this->areElementsReleasedCalledForHostptrs = hostptrsOnly;
    areElementsReleasedCalled++;
    return areElementsReleasedCalled != 1;
}

bool MockDeferredDeleter::shouldStop() {

    shouldStopCalled++;
    if (stopAfter3loopsInRun && shouldStopCalled < 3) {
        auto deletion = new MockDeferrableDeletion();
        elementsToRelease++;
        queue.pushTailOne(*deletion);
        condition.notify_one();

        return false;
    }
    return shouldStopCalled > 1;
}

void MockDeferredDeleter::clearQueue(bool hostptrsOnly) {
    DeferredDeleter::clearQueue(hostptrsOnly);
    clearCalled++;
}

int MockDeferredDeleter::getClientsNum() {
    return numClients;
}

int MockDeferredDeleter::getElementsToRelease() {
    return elementsToRelease;
}

bool MockDeferredDeleter::isWorking() {
    return doWorkInBackground;
}

bool MockDeferredDeleter::isThreadRunning() {
    return worker != nullptr;
}

bool MockDeferredDeleter::isQueueEmpty() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return queue.peekIsEmpty();
}

void MockDeferredDeleter::setElementsToRelease(int elementsNum) {
    elementsToRelease = elementsNum;
}

void MockDeferredDeleter::setDoWorkInBackgroundValue(bool value) {
    doWorkInBackground = value;
}

bool MockDeferredDeleter::baseAreElementsReleased() {
    return DeferredDeleter::areElementsReleased(false);
}

bool MockDeferredDeleter::baseShouldStop() {
    return DeferredDeleter::shouldStop();
}

Thread *MockDeferredDeleter::getThreadHandle() {
    return worker.get();
}

std::unique_ptr<DeferredDeleter> createDeferredDeleter() {
    return std::unique_ptr<DeferredDeleter>(new MockDeferredDeleter());
}

void MockDeferredDeleter::runThread() {
    worker = Thread::createFunc(run, reinterpret_cast<void *>(this));
}

void MockDeferredDeleter::forceStop() {
    allowEarlyStopThread();
    stop();
}

void MockDeferredDeleter::allowEarlyStopThread() {
    shouldStopCalled = 2;
}

MockDeferredDeleter::~MockDeferredDeleter() {
    allowEarlyStopThread();
    if (expectDrainCalled) {
        EXPECT_NE(0, drainCalled);
    }
}

void MockDeferredDeleter::expectDrainBlockingValue(bool value) {
    expectedDrainValue = value;
    expectDrainCalled = true;
}
} // namespace NEO
