/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/debug_mock_drm_xe.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "uapi-eudebug/drm/xe_drm.h"
#include "uapi-eudebug/drm/xe_drm_tmp.h"

using namespace NEO;

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGetIoctForDebuggerThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    auto verifyIoctlRequestValue = [&xeIoctlHelper](auto value, DrmIoctl drmIoctl) {
        EXPECT_EQ(xeIoctlHelper->getIoctlRequestValue(drmIoctl), static_cast<unsigned int>(value));
    };
    auto verifyIoctlString = [&xeIoctlHelper](DrmIoctl drmIoctl, const char *string) {
        EXPECT_STREQ(string, xeIoctlHelper->getIoctlString(drmIoctl).c_str());
    };

    verifyIoctlString(DrmIoctl::debuggerOpen, "DRM_IOCTL_XE_EUDEBUG_CONNECT");

    verifyIoctlRequestValue(DRM_IOCTL_XE_EUDEBUG_CONNECT, DrmIoctl::debuggerOpen);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingaddDebugMetadataThenDataIsAdded) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    uint64_t temp = 0;
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::moduleHeapDebugArea, &temp, 8000u);
    ASSERT_EQ(1u, xeIoctlHelper->debugMetadata.size());
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::contextSaveArea, &temp, 8000u);
    ASSERT_EQ(2u, xeIoctlHelper->debugMetadata.size());
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::sbaTrackingBuffer, &temp, 8000u);
    ASSERT_EQ(3u, xeIoctlHelper->debugMetadata.size());
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::isa, &temp, 8000u); // ISA should be ignored
    ASSERT_EQ(3u, xeIoctlHelper->debugMetadata.size());
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingVmCreateThenDebugMetadadaIsAttached) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    uint64_t temp = 0;
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::moduleHeapDebugArea, &temp, 8000u);
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::contextSaveArea, &temp, 8000u);
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::sbaTrackingBuffer, &temp, 8000u);
    xeIoctlHelper->addDebugMetadata(DrmResourceClass::isa, &temp, 8000u); // ISA should be ignored
    xeIoctlHelper->addDebugMetadataCookie(123u);

    GemVmControl test = {};
    xeIoctlHelper->ioctl(DrmIoctl::gemVmCreate, &test);

    ASSERT_EQ(4u, drm.vmCreateMetadata.size());
    ASSERT_EQ(drm.vmCreateMetadata[0].type, static_cast<unsigned long long>(DRM_XE_VM_DEBUG_METADATA_MODULE_AREA));
    ASSERT_EQ(drm.vmCreateMetadata[0].offset, reinterpret_cast<unsigned long long>(&temp));
    ASSERT_EQ(drm.vmCreateMetadata[0].len, 8000ul);

    ASSERT_EQ(drm.vmCreateMetadata[1].type, static_cast<unsigned long long>(DRM_XE_VM_DEBUG_METADATA_SIP_AREA));
    ASSERT_EQ(drm.vmCreateMetadata[1].offset, reinterpret_cast<unsigned long long>(&temp));
    ASSERT_EQ(drm.vmCreateMetadata[1].len, 8000ul);

    ASSERT_EQ(drm.vmCreateMetadata[2].type, static_cast<unsigned long long>(DRM_XE_VM_DEBUG_METADATA_SBA_AREA));
    ASSERT_EQ(drm.vmCreateMetadata[2].offset, reinterpret_cast<unsigned long long>(&temp));
    ASSERT_EQ(drm.vmCreateMetadata[2].len, 8000ul);

    ASSERT_EQ(drm.vmCreateMetadata[3].type, static_cast<unsigned long long>(DRM_XE_VM_DEBUG_METADATA_COOKIE));
    ASSERT_EQ(drm.vmCreateMetadata[3].offset, 123ul);
    ASSERT_EQ(drm.vmCreateMetadata[3].len, 0ul);
    ASSERT_EQ(drm.vmCreateMetadata[3].base.next_extension, 0ul);
}

TEST(IoctlHelperXeTest, givenFreeDebugMetadataWhenVmCreateHasMultipleExtTypesThenOnlyDebugMetadataIsDeleted) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);

    drm_xe_ext_vm_set_debug_metadata *node1 = new drm_xe_ext_vm_set_debug_metadata();
    drm_xe_ext_vm_set_debug_metadata *node2 = new drm_xe_ext_vm_set_debug_metadata();
    drm_xe_ext_vm_set_debug_metadata *node3 = new drm_xe_ext_vm_set_debug_metadata();
    node1->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node2->base.name = 0x1234ul;
    node3->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node1->base.next_extension = reinterpret_cast<unsigned long long>(node2);
    node2->base.next_extension = reinterpret_cast<unsigned long long>(node3);

    drm_xe_ext_vm_set_debug_metadata *newRoot = static_cast<drm_xe_ext_vm_set_debug_metadata *>(xeIoctlHelper->freeDebugMetadata(node1));
    ASSERT_EQ(newRoot, node2);
    ASSERT_EQ(newRoot->base.next_extension, 0ul);
    delete node2;

    node1 = new drm_xe_ext_vm_set_debug_metadata();
    node2 = new drm_xe_ext_vm_set_debug_metadata();
    node3 = new drm_xe_ext_vm_set_debug_metadata();
    node1->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node2->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node3->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node1->base.next_extension = reinterpret_cast<unsigned long long>(node2);
    node2->base.next_extension = reinterpret_cast<unsigned long long>(node3);

    newRoot = static_cast<drm_xe_ext_vm_set_debug_metadata *>(xeIoctlHelper->freeDebugMetadata(node1));
    ASSERT_EQ(newRoot, nullptr);

    node1 = new drm_xe_ext_vm_set_debug_metadata();
    node2 = new drm_xe_ext_vm_set_debug_metadata();
    node3 = new drm_xe_ext_vm_set_debug_metadata();
    node1->base.name = 0x1234;
    node2->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node3->base.name = DRM_XE_VM_EXTENSION_SET_DEBUG_METADATA;
    node1->base.next_extension = reinterpret_cast<unsigned long long>(node2);
    node2->base.next_extension = reinterpret_cast<unsigned long long>(node3);

    newRoot = static_cast<drm_xe_ext_vm_set_debug_metadata *>(xeIoctlHelper->freeDebugMetadata(node1));
    ASSERT_EQ(newRoot, node1);
    ASSERT_EQ(newRoot->base.next_extension, 0ul);
    delete node1;

    node1 = new drm_xe_ext_vm_set_debug_metadata();
    node2 = new drm_xe_ext_vm_set_debug_metadata();
    node3 = new drm_xe_ext_vm_set_debug_metadata();
    node1->base.name = 0x1234;
    node2->base.name = 0x1234;
    node3->base.name = 0x1234;
    node1->base.next_extension = reinterpret_cast<unsigned long long>(node2);
    node2->base.next_extension = reinterpret_cast<unsigned long long>(node3);

    newRoot = static_cast<drm_xe_ext_vm_set_debug_metadata *>(xeIoctlHelper->freeDebugMetadata(node1));
    ASSERT_EQ(newRoot, node1);
    ASSERT_EQ(newRoot->base.next_extension, reinterpret_cast<unsigned long long>(node2));
    delete node1;
    delete node2;
    delete node3;
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGetRunaloneExtPropertyThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    EXPECT_EQ(xeIoctlHelper->getRunaloneExtProperty(), DRM_XE_EXEC_QUEUE_SET_PROPERTY_RUNALONE);
}

using IoctlHelperXeTestFixture = ::testing::Test;
HWTEST_F(IoctlHelperXeTestFixture, GivenRunaloneModeRequiredReturnFalseWhenCreateDrmContextThenRunAloneContextIsNotRequested) {
    DebugManagerStateRestore restorer;
    struct MockGfxCoreHelperHw : NEO::GfxCoreHelperHw<FamilyType> {
        bool isRunaloneModeRequired(DebuggingMode debuggingMode) const override {
            return false;
        }
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto raiiFactory = RAIIGfxCoreHelperFactory<MockGfxCoreHelperHw>(rootDeviceEnvironment);
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    xeIoctlHelper->createDrmContext(drm, osContext, 0, 0);

    auto ext = drm.receivedContextCreateSetParam;
    EXPECT_NE(ext.property, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_SET_PROPERTY_RUNALONE));
}

HWTEST_F(IoctlHelperXeTestFixture, GivenRunaloneModeRequiredReturnTrueWhenCreateDrmContextThenRunAloneContextIsRequested) {
    DebugManagerStateRestore restorer;
    struct MockGfxCoreHelperHw : NEO::GfxCoreHelperHw<FamilyType> {
        bool isRunaloneModeRequired(DebuggingMode debuggingMode) const override {
            return true;
        }
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto raiiFactory = RAIIGfxCoreHelperFactory<MockGfxCoreHelperHw>(rootDeviceEnvironment);
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    DrmMockXeDebug drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    xeIoctlHelper->createDrmContext(drm, osContext, 0, 0);

    auto ext = drm.receivedContextCreateSetParam;
    EXPECT_EQ(ext.base.name, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY));
    EXPECT_EQ(ext.base.next_extension, 0ULL);
    EXPECT_EQ(ext.property, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_SET_PROPERTY_RUNALONE));
    EXPECT_EQ(ext.value, 1ULL);
}