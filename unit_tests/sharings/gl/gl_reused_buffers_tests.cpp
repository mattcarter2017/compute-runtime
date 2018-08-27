/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/mem_obj/buffer.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/sharings/gl/gl_buffer.h"
#include "unit_tests/mocks/gl/mock_gl_sharing.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "test.h"

using namespace OCLRT;

struct GlReusedBufferTests : public ::testing::Test {
    void SetUp() override {
        glSharingFunctions = new GlSharingFunctionsMock();
        context.setSharingFunctions(glSharingFunctions);
        graphicsAllocationsForGlBufferReuse = &glSharingFunctions->graphicsAllocationsForGlBufferReuse;
    }

    GlSharingFunctionsMock *glSharingFunctions = nullptr;
    MockContext context;

    std::vector<std::pair<unsigned int, GraphicsAllocation *>> *graphicsAllocationsForGlBufferReuse = nullptr;
    unsigned int bufferId1 = 5;
    unsigned int bufferId2 = 7;
    cl_int retVal = CL_SUCCESS;
};

class FailingMemoryManager : public MockMemoryManager {
  public:
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override {
        return nullptr;
    }
};

TEST_F(GlReusedBufferTests, givenMultipleBuffersWithTheSameIdWhenCreatedThenReuseGraphicsAllocation) {
    std::unique_ptr<Buffer> glBuffers[10]; // first 5 with bufferId1, next 5 with bufferId2

    for (size_t i = 0; i < 10; i++) {
        glBuffers[i].reset(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, (i < 5 ? bufferId1 : bufferId2), &retVal));
        EXPECT_NE(nullptr, glBuffers[i].get());
        EXPECT_NE(nullptr, glBuffers[i]->getGraphicsAllocation());
    }

    EXPECT_EQ(2u, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(bufferId1, graphicsAllocationsForGlBufferReuse->at(0).first);
    EXPECT_EQ(bufferId2, graphicsAllocationsForGlBufferReuse->at(1).first);

    auto storedGraphicsAllocation1 = graphicsAllocationsForGlBufferReuse->at(0).second;
    auto storedGraphicsAllocation2 = graphicsAllocationsForGlBufferReuse->at(1).second;
    EXPECT_EQ(5u, storedGraphicsAllocation1->peekReuseCount());
    EXPECT_EQ(5u, storedGraphicsAllocation2->peekReuseCount());

    for (size_t i = 0; i < 10; i++) {
        EXPECT_EQ(i < 5 ? storedGraphicsAllocation1 : storedGraphicsAllocation2,
                  glBuffers[i]->getGraphicsAllocation());
    }
}

TEST_F(GlReusedBufferTests, givenMultipleBuffersWithReusedAllocationWhenReleasingThenClearVectorByLastObject) {
    std::unique_ptr<Buffer> glBuffer1(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    std::unique_ptr<Buffer> glBuffer2(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));

    EXPECT_EQ(1u, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(2u, graphicsAllocationsForGlBufferReuse->at(0).second->peekReuseCount());

    glBuffer1.reset(nullptr);

    EXPECT_EQ(1u, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(1u, graphicsAllocationsForGlBufferReuse->at(0).second->peekReuseCount());

    glBuffer2.reset(nullptr);
    EXPECT_EQ(0u, graphicsAllocationsForGlBufferReuse->size());
}

TEST_F(GlReusedBufferTests, givenMultipleBuffersWithReusedAllocationWhenCreatingThenReuseGmmResourceToo) {
    std::unique_ptr<Buffer> glBuffer1(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    glBuffer1->getGraphicsAllocation()->gmm = new Gmm((void *)0x100, 1, false);

    std::unique_ptr<Buffer> glBuffer2(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));

    EXPECT_EQ(glBuffer1->getGraphicsAllocation()->gmm->gmmResourceInfo->peekHandle(),
              glBuffer2->getGraphicsAllocation()->gmm->gmmResourceInfo->peekHandle());
}

TEST_F(GlReusedBufferTests, givenGlobalShareHandleChangedWhenAcquiringSharedBufferThenChangeGraphicsAllocation) {
    bufferInfoOutput.globalShareHandle = 40;
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = clBuffer->peekSharingHandler();
    auto oldGraphicsAllocation = clBuffer->getGraphicsAllocation();

    ASSERT_EQ(40, oldGraphicsAllocation->peekSharedHandle());

    bufferInfoOutput.globalShareHandle = 41;
    glBuffer->acquire(clBuffer.get());
    auto newGraphicsAllocation = clBuffer->getGraphicsAllocation();

    EXPECT_NE(oldGraphicsAllocation, newGraphicsAllocation);
    EXPECT_EQ(41, newGraphicsAllocation->peekSharedHandle());

    glBuffer->release(clBuffer.get());
}

TEST_F(GlReusedBufferTests, givenGlobalShareHandleDidNotChangeWhenAcquiringSharedBufferThenDontDynamicallyAllocateBufferInfo) {
    class MyGlBuffer : public GlBuffer {
      public:
        MyGlBuffer(GLSharingFunctions *sharingFunctions, unsigned int glObjectId) : GlBuffer(sharingFunctions, glObjectId) {}

      protected:
        void resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) override {
            EXPECT_EQ(nullptr, updateData->updateData);
            GlBuffer::resolveGraphicsAllocationChange(currentSharedHandle, updateData);
        }
    };

    bufferInfoOutput.globalShareHandle = 40;
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = new MyGlBuffer(context.getSharing<GLSharingFunctions>(), bufferId1);
    clBuffer->setSharingHandler(glBuffer);

    glBuffer->acquire(clBuffer.get());

    glBuffer->release(clBuffer.get());
}

TEST_F(GlReusedBufferTests, givenGlobalShareHandleChangedWhenAcquiringSharedBufferThenDynamicallyAllocateBufferInfo) {
    class MyGlBuffer : public GlBuffer {
      public:
        MyGlBuffer(GLSharingFunctions *sharingFunctions, unsigned int glObjectId) : GlBuffer(sharingFunctions, glObjectId) {}

      protected:
        void resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) override {
            EXPECT_NE(nullptr, updateData->updateData);
            GlBuffer::resolveGraphicsAllocationChange(currentSharedHandle, updateData);
        }
    };

    bufferInfoOutput.globalShareHandle = 40;
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = new MyGlBuffer(context.getSharing<GLSharingFunctions>(), bufferId1);
    clBuffer->setSharingHandler(glBuffer);

    bufferInfoOutput.globalShareHandle = 41;
    glBuffer->acquire(clBuffer.get());

    glBuffer->release(clBuffer.get());
}

TEST_F(GlReusedBufferTests, givenMultipleBuffersAndGlobalShareHandleChangedWhenAcquiringSharedBufferDeleteOldGfxAllocationFromReuseVector) {
    bufferInfoOutput.globalShareHandle = 40;
    auto clBuffer1 = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto clBuffer2 = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto graphicsAllocation1 = clBuffer1->getGraphicsAllocation();
    auto graphicsAllocation2 = clBuffer2->getGraphicsAllocation();
    ASSERT_EQ(graphicsAllocation1, graphicsAllocation2);
    ASSERT_EQ(2, graphicsAllocation1->peekReuseCount());
    ASSERT_EQ(1, graphicsAllocationsForGlBufferReuse->size());

    bufferInfoOutput.globalShareHandle = 41;
    clBuffer1->peekSharingHandler()->acquire(clBuffer1.get());
    auto newGraphicsAllocation = clBuffer1->getGraphicsAllocation();
    EXPECT_EQ(1, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(newGraphicsAllocation, graphicsAllocationsForGlBufferReuse->at(0).second);

    clBuffer2->peekSharingHandler()->acquire(clBuffer2.get());
    EXPECT_EQ(clBuffer2->getGraphicsAllocation(), newGraphicsAllocation);
    EXPECT_EQ(1, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(newGraphicsAllocation, graphicsAllocationsForGlBufferReuse->at(0).second);

    clBuffer1->peekSharingHandler()->release(clBuffer1.get());
    clBuffer2->peekSharingHandler()->release(clBuffer2.get());
}

TEST_F(GlReusedBufferTests, givenGraphicsAllocationCreationReturnsNullptrWhenAcquiringGlBufferThenReturnOutOfResourcesAndNullifyAllocation) {
    auto suceedingMemoryManager = context.getMemoryManager();
    auto failingMemoryManager = std::unique_ptr<FailingMemoryManager>(new FailingMemoryManager());

    bufferInfoOutput.globalShareHandle = 40;
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = clBuffer->peekSharingHandler();

    bufferInfoOutput.globalShareHandle = 41;
    context.setMemoryManager(failingMemoryManager.get());
    auto result = glBuffer->acquire(clBuffer.get());

    EXPECT_EQ(CL_OUT_OF_RESOURCES, result);
    EXPECT_EQ(nullptr, clBuffer->getGraphicsAllocation());

    context.setMemoryManager(suceedingMemoryManager);
}
