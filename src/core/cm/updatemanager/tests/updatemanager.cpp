/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/tests/mocks/cloudconnectionmock.hpp>
#include <core/common/tests/mocks/identprovidermock.hpp>
#include <core/common/tests/mocks/instancestatusprovidermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/time.hpp>

#include <core/cm/tests/mocks/nodeinfoprovidermock.hpp>
#include <core/cm/updatemanager/itf/sender.hpp>
#include <core/cm/updatemanager/updatemanager.hpp>

#include "mocks/imagemanagermock.hpp"
#include "mocks/unitconfigmock.hpp"
#include "stubs/senderstub.hpp"

using namespace testing;

namespace aos::cm::updatemanager {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

static constexpr Duration cUnitStatusSendTimeout = 500 * Time::cMilliseconds;

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

void CreateNodeInfo(UnitNodeInfo& nodeInfo, const String& nodeID, const String& nodeType,
    const NodeState& state = NodeStateEnum::eOnline)
{
    nodeInfo.mNodeID      = nodeID;
    nodeInfo.mNodeType    = nodeType;
    nodeInfo.mState       = state;
    nodeInfo.mProvisioned = true;

    ResourceInfo resourceInfo1;

    resourceInfo1.mName        = "resource1";
    resourceInfo1.mSharedCount = 4;

    ResourceInfo resourceInfo2;

    resourceInfo2.mName        = "resource2";
    resourceInfo2.mSharedCount = 8;

    auto err = nodeInfo.mResources.PushBack(resourceInfo1);
    EXPECT_TRUE(err.IsNone());
    err = nodeInfo.mResources.PushBack(resourceInfo2);
    EXPECT_TRUE(err.IsNone());

    RuntimeInfo runtimeInfo1;

    runtimeInfo1.mRuntimeID   = "runtime1";
    runtimeInfo1.mRuntimeType = "runc";

    RuntimeInfo runtimeInfo2;

    runtimeInfo2.mRuntimeID   = "runtime2";
    runtimeInfo2.mRuntimeType = "xrun";

    err = nodeInfo.mRuntimes.PushBack(runtimeInfo1);
    EXPECT_TRUE(err.IsNone());
    err = nodeInfo.mRuntimes.PushBack(runtimeInfo2);
    EXPECT_TRUE(err.IsNone());
}

void CreateUpdateItemStatus(UpdateItemStatus& itemStatus, const String& itemID, const String& version, size_t numImages,
    const ImageState& state = ImageStateEnum::eInstalled)
{
    itemStatus.mItemID  = itemID;
    itemStatus.mVersion = version;
    itemStatus.mStatuses.Clear();

    for (size_t i = 0; i < numImages; i++) {
        ImageStatus imageStatus;

        imageStatus.mImageID.Format("image%zu", i + 1);
        imageStatus.mState = state;

        auto err = itemStatus.mStatuses.PushBack(imageStatus);
        EXPECT_TRUE(err.IsNone());
    }
}

void CreateInstancesStatuses(UnitInstancesStatuses& instancesStatuses, const String& itemID, const String& subjectID,
    const String& version, size_t numInstances, const InstanceState& state = InstanceStateEnum::eActive)
{
    instancesStatuses.mItemID    = itemID;
    instancesStatuses.mSubjectID = subjectID;
    instancesStatuses.mVersion   = version;
    instancesStatuses.mInstances.Clear();

    for (size_t i = 0; i < numInstances; i++) {
        UnitInstanceStatus instanceStatus;

        instanceStatus.mInstance  = i;
        instanceStatus.mImageID   = "image1";
        instanceStatus.mNodeID    = "node1";
        instanceStatus.mRuntimeID = "runtime1";
        instanceStatus.mState     = state;

        auto err = instancesStatuses.mInstances.PushBack(instanceStatus);
        EXPECT_TRUE(err.IsNone());
    }
}

void ClearUnitStatus(UnitStatus& unitStatus)
{
    unitStatus.mIsDeltaInfo = false;
    unitStatus.mUnitConfig.Reset();
    unitStatus.mNodes.Reset();
    unitStatus.mUpdateItems.Reset();
    unitStatus.mInstances.Reset();
    unitStatus.mUnitSubjects.Reset();
};

void CreateInstanceStatus(InstanceStatus& instanceStatus, const String& itemID, const String& subjectID,
    uint64_t instance, const String& version, const UnitInstanceStatus& unitInstanceStatus)
{
    instanceStatus.mItemID    = itemID;
    instanceStatus.mSubjectID = subjectID;
    instanceStatus.mInstance  = instance;
    instanceStatus.mVersion   = version;
    instanceStatus.mNodeID    = unitInstanceStatus.mNodeID;
    instanceStatus.mRuntimeID = unitInstanceStatus.mRuntimeID;
    instanceStatus.mImageID   = unitInstanceStatus.mImageID;
    instanceStatus.mState     = unitInstanceStatus.mState;
    instanceStatus.mError     = unitInstanceStatus.mError;
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class UpdateManagerTest : public Test {
protected:
    static void SetUpTestSuite()
    {
        tests::utils::InitLog();

        LOG_INF() << "Update manager size" << Log::Field("size", sizeof(UpdateManager));
    }

    void SetUp() override
    {
        Config config {cUnitStatusSendTimeout};

        auto err = mUpdateManager.Init(config, mIdentProviderMock, mUnitConfigMock, mNodeInfoProviderMock,
            mImageManagerMock, mInstanceStatusProviderMock, mCloudConnectionMock, mSenderStub);
        EXPECT_TRUE(err.IsNone()) << "Failed to initialize update manager: " << tests::utils::ErrorToStr(err);

        EXPECT_CALL(mCloudConnectionMock, SubscribeListener(_))
            .WillOnce(Invoke([&](cloudconnection::ConnectionListenerItf& listener) {
                mConnectionListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mNodeInfoProviderMock, SubscribeListener(_))
            .WillOnce(Invoke([&](nodeinfoprovider::NodeInfoListenerItf& listener) {
                mNodeInfoListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mImageManagerMock, SubscribeListener(_))
            .WillOnce(Invoke([&](imagemanager::ImageStatusListenerItf& listener) {
                mImageStatusListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mInstanceStatusProviderMock, SubscribeListener(_))
            .WillOnce(Invoke([&](instancestatusprovider::ListenerItf& listener) {
                mInstanceStatusListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mIdentProviderMock, SubscribeListener(_))
            .WillOnce(Invoke([&](iamclient::SubjectsListenerItf& listener) {
                mSubjectsListener = &listener;

                return ErrorEnum::eNone;
            }));

        err = mUpdateManager.Start();
        EXPECT_TRUE(err.IsNone()) << "Failed to start update manager: " << tests::utils::ErrorToStr(err);
    }

    void TearDown() override
    {
        EXPECT_CALL(mCloudConnectionMock, UnsubscribeListener(_))
            .WillOnce(Invoke([&](cloudconnection::ConnectionListenerItf& listener) {
                (void)listener;

                mConnectionListener = nullptr;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mNodeInfoProviderMock, UnsubscribeListener(_))
            .WillOnce(Invoke([&](nodeinfoprovider::NodeInfoListenerItf& listener) {
                (void)listener;

                mNodeInfoListener = nullptr;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mImageManagerMock, UnsubscribeListener(_))
            .WillOnce(Invoke([&](imagemanager::ImageStatusListenerItf& listener) {
                (void)listener;

                mImageStatusListener = nullptr;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mInstanceStatusProviderMock, UnsubscribeListener(_))
            .WillOnce(Invoke([&](instancestatusprovider::ListenerItf& listener) {
                (void)listener;

                mInstanceStatusListener = nullptr;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mIdentProviderMock, UnsubscribeListener(_))
            .WillOnce(Invoke([&](iamclient::SubjectsListenerItf& listener) {
                (void)listener;

                mSubjectsListener = nullptr;

                return ErrorEnum::eNone;
            }));

        auto err = mUpdateManager.Stop();
        EXPECT_TRUE(err.IsNone()) << "Failed to stop update manager: " << tests::utils::ErrorToStr(err);
    }

    UpdateManager                                    mUpdateManager;
    NiceMock<iamclient::IdentProviderMock>           mIdentProviderMock;
    NiceMock<unitconfig::UnitConfigMock>             mUnitConfigMock;
    NiceMock<nodeinfoprovider::NodeInfoProviderMock> mNodeInfoProviderMock;
    NiceMock<imagemanager::ImageManagerMock>         mImageManagerMock;
    NiceMock<instancestatusprovider::ProviderMock>   mInstanceStatusProviderMock;
    NiceMock<cloudconnection::CloudConnectionMock>   mCloudConnectionMock;
    SenderStub                                       mSenderStub;

    cloudconnection::ConnectionListenerItf* mConnectionListener {};
    nodeinfoprovider::NodeInfoListenerItf*  mNodeInfoListener {};
    imagemanager::ImageStatusListenerItf*   mImageStatusListener {};
    instancestatusprovider::ListenerItf*    mInstanceStatusListener {};
    iamclient::SubjectsListenerItf*         mSubjectsListener {};
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(UpdateManagerTest, SendUnitStatusOnCloudConnect)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();

    expectedUnitStatus->mIsDeltaInfo = false;

    // Set unit config status

    expectedUnitStatus->mUnitConfig.EmplaceValue();
    expectedUnitStatus->mUnitConfig->EmplaceBack(
        UnitConfigStatus {"1.0.0", UnitConfigStateEnum::eInstalled, ErrorEnum::eNone});

    EXPECT_CALL(mUnitConfigMock, GetUnitConfigStatus(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUnitConfig.GetValue()[0]), Return(ErrorEnum::eNone)));

    // Set node infos

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIDs;

    nodeIDs.EmplaceBack("node1");
    nodeIDs.EmplaceBack("node2");

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeTypes;

    nodeTypes.EmplaceBack("nodeType1");
    nodeTypes.EmplaceBack("nodeType2");

    expectedUnitStatus->mNodes.EmplaceValue();

    for (size_t i = 0; i < nodeIDs.Size(); i++) {
        expectedUnitStatus->mNodes->EmplaceBack();
        CreateNodeInfo(expectedUnitStatus->mNodes->Back(), nodeIDs[i], nodeTypes[i]);
    }

    EXPECT_CALL(mNodeInfoProviderMock, GetAllNodeIDs(_))
        .WillOnce(DoAll(SetArgReferee<0>(nodeIDs), Return(ErrorEnum::eNone)));

    EXPECT_CALL(mNodeInfoProviderMock, GetNodeInfo(_, _))
        .WillRepeatedly(Invoke([&](const String& nodeID, UnitNodeInfo& nodeInfo) {
            auto it
                = expectedUnitStatus->mNodes->FindIf([&](const UnitNodeInfo& info) { return info.mNodeID == nodeID; });
            if (it == expectedUnitStatus->mNodes->end()) {
                return ErrorEnum::eNotFound;
            }

            nodeInfo = *it;

            return ErrorEnum::eNone;
        }));

    // Set update items

    expectedUnitStatus->mUpdateItems.EmplaceValue();
    expectedUnitStatus->mUpdateItems->Resize(2);

    CreateUpdateItemStatus(expectedUnitStatus->mUpdateItems.GetValue()[0], "item1", "1.0.0", 2);
    CreateUpdateItemStatus(expectedUnitStatus->mUpdateItems.GetValue()[1], "item2", "1.0.0", 3);

    EXPECT_CALL(mImageManagerMock, GetUpdateItemsStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUpdateItems.GetValue()), Return(ErrorEnum::eNone)));

    // Set instances statuses

    expectedUnitStatus->mInstances.EmplaceValue();
    expectedUnitStatus->mInstances->Resize(3);

    CreateInstancesStatuses(expectedUnitStatus->mInstances.GetValue()[0], "item1", "subject1", "1.0.0", 2);
    CreateInstancesStatuses(expectedUnitStatus->mInstances.GetValue()[1], "item2", "subject1", "1.0.0", 1);
    CreateInstancesStatuses(expectedUnitStatus->mInstances.GetValue()[2], "item2", "subject2", "1.0.0", 3);

    EXPECT_CALL(mInstanceStatusProviderMock, GetInstancesStatuses(_))
        .WillOnce(Invoke([&](Array<InstanceStatus>& instances) {
            for (const auto& instancesStatuses : expectedUnitStatus->mInstances.GetValue()) {
                for (const auto& instanceStatus : instancesStatuses.mInstances) {
                    InstanceStatus status;

                    status.mItemID    = instancesStatuses.mItemID;
                    status.mSubjectID = instancesStatuses.mSubjectID;
                    status.mVersion   = instancesStatuses.mVersion;
                    status.mInstance  = instanceStatus.mInstance;
                    status.mNodeID    = instanceStatus.mNodeID;
                    status.mRuntimeID = instanceStatus.mRuntimeID;
                    status.mImageID   = instanceStatus.mImageID;
                    status.mState     = instanceStatus.mState;

                    instances.PushBack(status);
                }
            }

            return ErrorEnum::eNone;
        }));

    // Set subjects

    expectedUnitStatus->mUnitSubjects.EmplaceValue();

    expectedUnitStatus->mUnitSubjects->EmplaceBack("subject1");
    expectedUnitStatus->mUnitSubjects->EmplaceBack("subject2");
    expectedUnitStatus->mUnitSubjects->EmplaceBack("subject3");

    EXPECT_CALL(mIdentProviderMock, GetSubjects(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUnitSubjects.GetValue()), Return(ErrorEnum::eNone)));

    // Notify cloud connection established

    mConnectionListener->OnConnect();

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
}

TEST_F(UpdateManagerTest, SendDeltaUnitStatus)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();

    expectedUnitStatus->mUnitConfig.EmplaceValue();
    expectedUnitStatus->mUnitConfig->EmplaceBack();
    expectedUnitStatus->mNodes.EmplaceValue();
    expectedUnitStatus->mUpdateItems.EmplaceValue();
    expectedUnitStatus->mInstances.EmplaceValue();
    expectedUnitStatus->mUnitSubjects.EmplaceValue();

    // Notify cloud connection established

    mConnectionListener->OnConnect();

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    ClearUnitStatus(*expectedUnitStatus);

    // Set node infos

    expectedUnitStatus->mIsDeltaInfo = true;

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIDs;

    nodeIDs.EmplaceBack("node3");
    nodeIDs.EmplaceBack("node4");

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeTypes;

    nodeTypes.EmplaceBack("nodeType3");
    nodeTypes.EmplaceBack("nodeType4");

    expectedUnitStatus->mNodes.EmplaceValue();

    for (size_t i = 0; i < nodeIDs.Size(); i++) {
        expectedUnitStatus->mNodes->EmplaceBack();
        CreateNodeInfo(expectedUnitStatus->mNodes->Back(), nodeIDs[i], nodeTypes[i]);
    }

    // Notify node info changed

    for (const auto& nodeInfo : *expectedUnitStatus->mNodes) {
        mNodeInfoListener->OnNodeInfoChanged(nodeInfo);
    }

    CreateNodeInfo(expectedUnitStatus->mNodes.GetValue()[0], nodeIDs[0], nodeTypes[0], NodeStateEnum::eOffline);

    mNodeInfoListener->OnNodeInfoChanged(expectedUnitStatus->mNodes.GetValue()[0]);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    ClearUnitStatus(*expectedUnitStatus);

    // Set update items

    expectedUnitStatus->mIsDeltaInfo = true;

    expectedUnitStatus->mUpdateItems.EmplaceValue();
    expectedUnitStatus->mUpdateItems->Resize(2);

    CreateUpdateItemStatus(
        expectedUnitStatus->mUpdateItems.GetValue()[0], "item3", "1.0.0", 3, ImageStateEnum::eInstalling);
    CreateUpdateItemStatus(
        expectedUnitStatus->mUpdateItems.GetValue()[1], "item4", "1.0.0", 4, ImageStateEnum::eInstalling);

    // Notify image statuses changed

    for (const auto& itemStatus : *expectedUnitStatus->mUpdateItems) {
        for (const auto& imageStatus : itemStatus.mStatuses) {
            mImageStatusListener->OnImageStatusChanged(itemStatus.mItemID, itemStatus.mVersion, imageStatus);
        }
    }

    CreateUpdateItemStatus(
        expectedUnitStatus->mUpdateItems.GetValue()[0], "item3", "1.0.0", 3, ImageStateEnum::eInstalled);
    CreateUpdateItemStatus(
        expectedUnitStatus->mUpdateItems.GetValue()[1], "item4", "1.0.0", 4, ImageStateEnum::eInstalled);

    // Notify image statuses changed

    for (const auto& itemStatus : *expectedUnitStatus->mUpdateItems) {
        for (const auto& imageStatus : itemStatus.mStatuses) {
            mImageStatusListener->OnImageStatusChanged(itemStatus.mItemID, itemStatus.mVersion, imageStatus);
        }
    }

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    ClearUnitStatus(*expectedUnitStatus);

    // Set instances statuses

    expectedUnitStatus->mIsDeltaInfo = true;

    expectedUnitStatus->mInstances.EmplaceValue();
    expectedUnitStatus->mInstances->Resize(2);

    CreateInstancesStatuses(
        expectedUnitStatus->mInstances.GetValue()[0], "item1", "subject1", "1.0.0", 4, InstanceStateEnum::eActivating);
    CreateInstancesStatuses(
        expectedUnitStatus->mInstances.GetValue()[1], "item2", "subject1", "1.0.0", 3, InstanceStateEnum::eActivating);

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    for (const auto& instancesStatuses : *expectedUnitStatus->mInstances) {
        for (const auto& instanceStatus : instancesStatuses.mInstances) {
            auto err = statuses->EmplaceBack();
            EXPECT_TRUE(err.IsNone());

            CreateInstanceStatus(statuses->Back(), instancesStatuses.mItemID, instancesStatuses.mSubjectID,
                instanceStatus.mInstance, instancesStatuses.mVersion, instanceStatus);
        }
    }

    // Notify instances statuses changed

    mInstanceStatusListener->OnInstancesStatusesChanged(*statuses);

    CreateInstancesStatuses(
        expectedUnitStatus->mInstances.GetValue()[0], "item1", "subject1", "1.0.0", 4, InstanceStateEnum::eActive);
    CreateInstancesStatuses(
        expectedUnitStatus->mInstances.GetValue()[1], "item2", "subject1", "1.0.0", 3, InstanceStateEnum::eActive);

    statuses->Clear();

    for (const auto& instancesStatuses : *expectedUnitStatus->mInstances) {
        for (const auto& instanceStatus : instancesStatuses.mInstances) {
            auto err = statuses->EmplaceBack();
            EXPECT_TRUE(err.IsNone());

            CreateInstanceStatus(statuses->Back(), instancesStatuses.mItemID, instancesStatuses.mSubjectID,
                instanceStatus.mInstance, instancesStatuses.mVersion, instanceStatus);
        }
    }

    // Notify instances statuses changed

    mInstanceStatusListener->OnInstancesStatusesChanged(*statuses);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    ClearUnitStatus(*expectedUnitStatus);

    // Set subjects

    expectedUnitStatus->mIsDeltaInfo = true;

    expectedUnitStatus->mUnitSubjects.EmplaceValue();

    expectedUnitStatus->mUnitSubjects->EmplaceBack("subject1");
    expectedUnitStatus->mUnitSubjects->EmplaceBack("subject2");
    expectedUnitStatus->mUnitSubjects->EmplaceBack("subject3");

    // Notify subjects changed

    mSubjectsListener->SubjectsChanged(*expectedUnitStatus->mUnitSubjects);

    expectedUnitStatus->mUnitSubjects->Clear();

    expectedUnitStatus->mUnitSubjects->EmplaceBack("subject4");
    expectedUnitStatus->mUnitSubjects->EmplaceBack("subject5");

    // Notify subjects changed

    mSubjectsListener->SubjectsChanged(*expectedUnitStatus->mUnitSubjects);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    ClearUnitStatus(*expectedUnitStatus);
}

} // namespace aos::cm::updatemanager
