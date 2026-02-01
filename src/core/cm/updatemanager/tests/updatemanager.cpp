/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/tests/mocks/cloudconnectionmock.hpp>
#include <core/common/tests/mocks/identprovidermock.hpp>
#include <core/common/tests/mocks/instancestatusprovidermock.hpp>
#include <core/common/tests/mocks/nodehandlermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/time.hpp>

#include <core/cm/tests/mocks/nodeinfoprovidermock.hpp>
#include <core/cm/updatemanager/itf/sender.hpp>
#include <core/cm/updatemanager/updatemanager.hpp>

#include "mocks/imagemanagermock.hpp"
#include "mocks/launchermock.hpp"
#include "mocks/unitconfigmock.hpp"
#include "stubs/senderstub.hpp"
#include "stubs/storagestub.hpp"

using namespace testing;

namespace aos::cm::updatemanager {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

static constexpr Duration cUnitStatusSendTimeout = 500 * Time::cMilliseconds;
const auto                cCVTimeout             = std::chrono::seconds(5);

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

void SetNodeInfo(UnitNodeInfo& nodeInfo, const String& nodeID, const String& nodeType,
    const NodeState& state = NodeStateEnum::eProvisioned, bool isConnected = true)
{
    nodeInfo.mNodeID      = nodeID;
    nodeInfo.mNodeType    = nodeType;
    nodeInfo.mState       = state;
    nodeInfo.mIsConnected = isConnected;

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

void CreateNodeInfo(UnitStatus& unitStatus, const String& nodeID, const String& nodeType,
    const NodeState& state = NodeStateEnum::eProvisioned, bool isConnected = true)
{
    if (!unitStatus.mNodes.HasValue()) {
        unitStatus.mNodes.EmplaceValue();
    }

    unitStatus.mNodes->EmplaceBack();

    SetNodeInfo(unitStatus.mNodes->Back(), nodeID, nodeType, state, isConnected);
}

void ChangeNodeInfo(UnitStatus& unitStatus, const String& nodeID, const String& nodeType,
    const NodeState& state = NodeStateEnum::eProvisioned, bool isConnected = true)
{
    auto it = unitStatus.mNodes->FindIf([&nodeID](const UnitNodeInfo& nodeInfo) { return nodeInfo.mNodeID == nodeID; });
    EXPECT_NE(it, unitStatus.mNodes->end());

    SetNodeInfo(*it, nodeID, nodeType, state, isConnected);
}

void CreateUpdateItemStatus(UnitStatus& unitStatus, const String& itemID, const String& version,
    const ItemState& state = ItemStateEnum::eInstalled)
{
    if (!unitStatus.mUpdateItems.HasValue()) {
        unitStatus.mUpdateItems.EmplaceValue();
    }

    unitStatus.mUpdateItems->EmplaceBack();

    auto& itemStatus = unitStatus.mUpdateItems->Back();

    itemStatus.mItemID  = itemID;
    itemStatus.mVersion = version;
    itemStatus.mState   = state;
}

void ChangeUpdateItemStatus(UnitStatus& unitStatus, const String& itemID, const String& version,
    const ItemState& state = ItemStateEnum::eInstalled)
{
    auto it = unitStatus.mUpdateItems->FindIf([&itemID, &version](const UpdateItemStatus& itemStatus) {
        return itemStatus.mItemID == itemID && itemStatus.mVersion == version;
    });
    EXPECT_NE(it, unitStatus.mUpdateItems->end());

    it->mState = state;
}

void CreateInstancesStatuses(UnitStatus& unitStatus, const String& itemID, const String& subjectID,
    const String& version, size_t numInstances, const InstanceState& state = InstanceStateEnum::eActive)
{

    if (!unitStatus.mInstances.HasValue()) {
        unitStatus.mInstances.EmplaceValue();
    }

    unitStatus.mInstances->EmplaceBack();

    auto& instancesStatuses = unitStatus.mInstances->Back();

    instancesStatuses.mItemID    = itemID;
    instancesStatuses.mSubjectID = subjectID;
    instancesStatuses.mVersion   = version;

    for (size_t i = 0; i < numInstances; i++) {
        UnitInstanceStatus instanceStatus;

        instanceStatus.mInstance       = i;
        instanceStatus.mManifestDigest = "digest1";
        instanceStatus.mNodeID         = "node1";
        instanceStatus.mRuntimeID      = "runtime1";
        instanceStatus.mState          = state;

        auto err = instancesStatuses.mInstances.PushBack(instanceStatus);
        EXPECT_TRUE(err.IsNone());
    }
}

void ChangeInstancesStatuses(UnitStatus& unitStatus, const String& itemID, const String& subjectID,
    const String& version, size_t numInstances, const InstanceState& state = InstanceStateEnum::eActive)
{
    auto it = unitStatus.mInstances->FindIf(
        [&itemID, &subjectID, &version](const UnitInstancesStatuses& instancesStatuses) {
            return instancesStatuses.mItemID == itemID && instancesStatuses.mSubjectID == subjectID
                && instancesStatuses.mVersion == version;
        });
    EXPECT_NE(it, unitStatus.mInstances->end());

    auto& instancesStatuses = *it;

    instancesStatuses.mInstances.Clear();

    for (size_t i = 0; i < numInstances; i++) {
        UnitInstanceStatus instanceStatus;

        instanceStatus.mInstance       = i;
        instanceStatus.mManifestDigest = "digest1";
        instanceStatus.mNodeID         = "node1";
        instanceStatus.mRuntimeID      = "runtime1";
        instanceStatus.mState          = state;

        auto err = instancesStatuses.mInstances.PushBack(instanceStatus);
        EXPECT_TRUE(err.IsNone());
    }
}

void CreateUnitConfigStatus(UnitStatus& unitStatus, const String& version,
    const UnitConfigState& state = UnitConfigStateEnum::eInstalled, Error err = ErrorEnum::eNone)
{
    if (!unitStatus.mUnitConfig.HasValue()) {
        unitStatus.mUnitConfig.EmplaceValue();
    }

    unitStatus.mUnitConfig->EmplaceBack(UnitConfigStatus {version, state, err});
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

void EmptyUnitStatus(UnitStatus& unitStatus)
{
    ClearUnitStatus(unitStatus);

    unitStatus.mUnitConfig.EmplaceValue();
    unitStatus.mUnitConfig->EmplaceBack();
    unitStatus.mNodes.EmplaceValue();
    unitStatus.mUpdateItems.EmplaceValue();
    unitStatus.mInstances.EmplaceValue();
    unitStatus.mUnitSubjects.EmplaceValue();
};

void CreateInstanceStatus(InstanceStatus& instanceStatus, const String& itemID, const String& subjectID,
    uint64_t instance, const String& version, const UnitInstanceStatus& unitInstanceStatus)
{
    instanceStatus.mItemID         = itemID;
    instanceStatus.mSubjectID      = subjectID;
    instanceStatus.mInstance       = instance;
    instanceStatus.mVersion        = version;
    instanceStatus.mNodeID         = unitInstanceStatus.mNodeID;
    instanceStatus.mRuntimeID      = unitInstanceStatus.mRuntimeID;
    instanceStatus.mManifestDigest = unitInstanceStatus.mManifestDigest;
    instanceStatus.mState          = unitInstanceStatus.mState;
    instanceStatus.mError          = unitInstanceStatus.mError;
};

void CreateUpdateItemInfo(
    DesiredStatus& desiredStatus, const String& itemID, UpdateItemType updateItemType, const String& version)
{
    desiredStatus.mUpdateItems.EmplaceBack();

    auto& itemInfo = desiredStatus.mUpdateItems.Back();

    itemInfo.mItemID  = itemID;
    itemInfo.mType    = updateItemType;
    itemInfo.mVersion = version;
}

void CreateRunRequest(const DesiredStatus& desiredStatus, Array<launcher::RunInstanceRequest>& runRequest)
{
    for (const auto& desiredInstance : desiredStatus.mInstances) {
        launcher::RunInstanceRequest request {};

        {
            auto it = desiredStatus.mUpdateItems.FindIf(
                [&desiredInstance](const UpdateItemInfo& item) { return item.mItemID == desiredInstance.mItemID; });
            EXPECT_NE(it, desiredStatus.mUpdateItems.end());

            request.mVersion        = it->mVersion;
            request.mOwnerID        = it->mOwnerID;
            request.mUpdateItemType = it->mType;
        }

        {
            auto it = desiredStatus.mSubjects.FindIf([&desiredInstance](const SubjectInfo& subject) {
                return subject.mSubjectID == desiredInstance.mSubjectID;
            });
            EXPECT_NE(it, desiredStatus.mSubjects.end());

            request.mSubjectInfo = *it;
        }

        request.mItemID       = desiredInstance.mItemID;
        request.mPriority     = desiredInstance.mPriority;
        request.mNumInstances = desiredInstance.mNumInstances;
        request.mLabels       = desiredInstance.mLabels;

        auto err = runRequest.PushBack(request);
        EXPECT_TRUE(err.IsNone());
    }
}

void ConvertInstancesStatuses(
    const Array<UnitInstancesStatuses>& unitInstancesStatuses, Array<InstanceStatus>& instancesStatuses)
{
    for (const auto& unitInstanceStatus : unitInstancesStatuses) {
        for (const auto& instanceStatus : unitInstanceStatus.mInstances) {
            InstanceStatus status;

            status.mItemID         = unitInstanceStatus.mItemID;
            status.mSubjectID      = unitInstanceStatus.mSubjectID;
            status.mVersion        = unitInstanceStatus.mVersion;
            status.mInstance       = instanceStatus.mInstance;
            status.mNodeID         = instanceStatus.mNodeID;
            status.mRuntimeID      = instanceStatus.mRuntimeID;
            status.mManifestDigest = instanceStatus.mManifestDigest;
            status.mState          = instanceStatus.mState;

            instancesStatuses.PushBack(status);
        }
    }
}

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

        auto err = mUpdateManager.Init(config, mIdentProviderMock, mNodeHandlerMock, mUnitConfigMock,
            mNodeInfoProviderMock, mImageManagerMock, mLauncherMock, mCloudConnectionMock, mSenderStub, mStorageStub);
        EXPECT_TRUE(err.IsNone()) << "Failed to initialize update manager: " << tests::utils::ErrorToStr(err);

        EXPECT_CALL(mCloudConnectionMock, SubscribeListener(_))
            .WillRepeatedly(Invoke([&](cloudconnection::ConnectionListenerItf& listener) {
                mConnectionListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mNodeInfoProviderMock, SubscribeListener(_))
            .WillRepeatedly(Invoke([&](nodeinfoprovider::NodeInfoListenerItf& listener) {
                mNodeInfoListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mImageManagerMock, SubscribeListener(_))
            .WillRepeatedly(Invoke([&](imagemanager::ItemStatusListenerItf& listener) {
                mItemStatusListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mLauncherMock, SubscribeListener(_))
            .WillRepeatedly(Invoke([&](instancestatusprovider::ListenerItf& listener) {
                mInstanceStatusListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mIdentProviderMock, SubscribeListener(_))
            .WillRepeatedly(Invoke([&](iamclient::SubjectsListenerItf& listener) {
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
            .WillOnce(Invoke([&](imagemanager::ItemStatusListenerItf& listener) {
                (void)listener;

                mItemStatusListener = nullptr;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mLauncherMock, UnsubscribeListener(_))
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
    NiceMock<iamclient::NodeHandlerMock>             mNodeHandlerMock;
    NiceMock<unitconfig::UnitConfigMock>             mUnitConfigMock;
    NiceMock<nodeinfoprovider::NodeInfoProviderMock> mNodeInfoProviderMock;
    NiceMock<imagemanager::ImageManagerMock>         mImageManagerMock;
    NiceMock<launcher::LauncherMock>                 mLauncherMock;
    NiceMock<cloudconnection::CloudConnectionMock>   mCloudConnectionMock;
    SenderStub                                       mSenderStub;
    StorageStub                                      mStorageStub;

    cloudconnection::ConnectionListenerItf* mConnectionListener {};
    nodeinfoprovider::NodeInfoListenerItf*  mNodeInfoListener {};
    imagemanager::ItemStatusListenerItf*    mItemStatusListener {};
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

    CreateUnitConfigStatus(*expectedUnitStatus, "1.0.0");

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
        CreateNodeInfo(*expectedUnitStatus, nodeIDs[i], nodeTypes[i]);
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

    CreateUpdateItemStatus(*expectedUnitStatus, "item1", "1.0.0");
    CreateUpdateItemStatus(*expectedUnitStatus, "item2", "1.0.0");

    EXPECT_CALL(mImageManagerMock, GetUpdateItemsStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUpdateItems.GetValue()), Return(ErrorEnum::eNone)));

    // Set instances statuses

    CreateInstancesStatuses(*expectedUnitStatus, "item1", "subject1", "1.0.0", 2);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", "subject1", "1.0.0", 1);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", "subject2", "1.0.0", 3);

    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillOnce(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

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

    EmptyUnitStatus(*expectedUnitStatus);

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

    for (size_t i = 0; i < nodeIDs.Size(); i++) {
        CreateNodeInfo(*expectedUnitStatus, nodeIDs[i], nodeTypes[i]);
    }

    // Notify node info changed

    for (const auto& nodeInfo : *expectedUnitStatus->mNodes) {
        mNodeInfoListener->OnNodeInfoChanged(nodeInfo);
    }

    ChangeNodeInfo(*expectedUnitStatus, nodeIDs[0], nodeTypes[0], NodeStateEnum::eProvisioned, false);

    mNodeInfoListener->OnNodeInfoChanged(expectedUnitStatus->mNodes.GetValue()[0]);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    ClearUnitStatus(*expectedUnitStatus);

    // Set update items

    expectedUnitStatus->mIsDeltaInfo = true;

    CreateUpdateItemStatus(*expectedUnitStatus, "item3", "1.0.0", ItemStateEnum::eInstalling);
    CreateUpdateItemStatus(*expectedUnitStatus, "item4", "1.0.0", ItemStateEnum::eInstalling);

    // Notify items statuses changed

    mItemStatusListener->OnItemsStatusesChanged(*expectedUnitStatus->mUpdateItems);

    ChangeUpdateItemStatus(*expectedUnitStatus, "item3", "1.0.0", ItemStateEnum::eInstalled);
    ChangeUpdateItemStatus(*expectedUnitStatus, "item4", "1.0.0", ItemStateEnum::eInstalled);

    // Notify items statuses changed

    mItemStatusListener->OnItemsStatusesChanged(*expectedUnitStatus->mUpdateItems);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    ClearUnitStatus(*expectedUnitStatus);

    // Set instances statuses

    expectedUnitStatus->mIsDeltaInfo = true;

    CreateInstancesStatuses(*expectedUnitStatus, "item1", "subject1", "1.0.0", 4, InstanceStateEnum::eActivating);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", "subject1", "1.0.0", 3, InstanceStateEnum::eActivating);

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

    ChangeInstancesStatuses(*expectedUnitStatus, "item1", "subject1", "1.0.0", 4, InstanceStateEnum::eActive);
    ChangeInstancesStatuses(*expectedUnitStatus, "item2", "subject1", "1.0.0", 3, InstanceStateEnum::eActive);

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

TEST_F(UpdateManagerTest, ProcessEmptyDesiredStatus)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    EXPECT_CALL(mImageManagerMock, DownloadUpdateItems(desiredStatus->mUpdateItems, _, _, _)).Times(1);
    EXPECT_CALL(mLauncherMock, RunInstances(Array<launcher::RunInstanceRequest>(), _)).Times(1);
    EXPECT_CALL(mImageManagerMock, InstallUpdateItems(desiredStatus->mUpdateItems, _)).Times(1);

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
}

TEST_F(UpdateManagerTest, ProcessFullDesiredStatus)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired node states

    desiredStatus->mNodes.EmplaceBack(DesiredNodeStateInfo {"node1", DesiredNodeStateEnum::ePaused});
    desiredStatus->mNodes.EmplaceBack(DesiredNodeStateInfo {"node2", DesiredNodeStateEnum::eProvisioned});

    // Set desired unit config

    desiredStatus->mUnitConfig.EmplaceValue(UnitConfig {"2.0.0", "1.0.0", {}});

    // Set desired update items

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemInfo(*desiredStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemInfo(*desiredStatus, "item3", UpdateItemTypeEnum::eService, "1.0.0");

    // Set desired instances

    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item2", "subject2", 1, 2, {}});
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item3", "subject3", 2, 3, {}});

    // Set desired unit subjects

    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject2", SubjectTypeEnum::eGroup});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject3", SubjectTypeEnum::eGroup});

    // Create launcher run request

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Set expected node infos

    CreateNodeInfo(*expectedUnitStatus, "node1", "type1", NodeStateEnum::ePaused, true);
    CreateNodeInfo(*expectedUnitStatus, "node2", "type2", NodeStateEnum::eProvisioned, true);

    // Set expected unit config status

    expectedUnitStatus->mUnitConfig.EmplaceValue();

    CreateUnitConfigStatus(*expectedUnitStatus, "2.0.0");

    // Set expected update items status

    CreateUpdateItemStatus(*expectedUnitStatus, "item1", "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(*expectedUnitStatus, "item2", "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(*expectedUnitStatus, "item3", "1.0.0", ItemStateEnum::eInstalled);

    // Set expected instances statuses

    CreateInstancesStatuses(*expectedUnitStatus, "item1", "subject1", "1.0.0", 1);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", "subject2", "1.0.0", 2);
    CreateInstancesStatuses(*expectedUnitStatus, "item3", "subject3", "1.0.0", 3);

    EXPECT_CALL(mNodeHandlerMock, PauseNode(String("node1"))).Times(1);
    EXPECT_CALL(mNodeHandlerMock, ResumeNode(String("node2"))).Times(1);
    EXPECT_CALL(mUnitConfigMock, CheckUnitConfig(desiredStatus->mUnitConfig.GetValue())).Times(1);
    EXPECT_CALL(mUnitConfigMock, UpdateUnitConfig(desiredStatus->mUnitConfig.GetValue())).Times(1);
    EXPECT_CALL(mImageManagerMock, DownloadUpdateItems(desiredStatus->mUpdateItems, _, _, _)).Times(1);
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(1);
    EXPECT_CALL(mImageManagerMock, InstallUpdateItems(desiredStatus->mUpdateItems, _)).Times(1);

    EXPECT_CALL(mNodeInfoProviderMock, GetAllNodeIDs(_)).WillOnce(Invoke([&](Array<StaticString<cIDLen>>& nodeIDs) {
        nodeIDs.EmplaceBack("node1");
        nodeIDs.EmplaceBack("node2");

        return ErrorEnum::eNone;
    }));
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
    EXPECT_CALL(mUnitConfigMock, GetUnitConfigStatus(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUnitConfig.GetValue()[0]), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mImageManagerMock, GetUpdateItemsStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUpdateItems.GetValue()), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillOnce(Invoke([&](Array<InstanceStatus>& instances) {
        for (const auto& instancesStatuses : expectedUnitStatus->mInstances.GetValue()) {
            for (const auto& instanceStatus : instancesStatuses.mInstances) {
                InstanceStatus status;

                status.mItemID         = instancesStatuses.mItemID;
                status.mSubjectID      = instancesStatuses.mSubjectID;
                status.mVersion        = instancesStatuses.mVersion;
                status.mInstance       = instanceStatus.mInstance;
                status.mNodeID         = instanceStatus.mNodeID;
                status.mRuntimeID      = instanceStatus.mRuntimeID;
                status.mManifestDigest = instanceStatus.mManifestDigest;
                status.mState          = instanceStatus.mState;

                instances.PushBack(status);
            }
        }

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
}

TEST_F(UpdateManagerTest, CancelCurrentUpdate)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired update items

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemStatus(*expectedUnitStatus, "item1", "1.0.0", ItemStateEnum::eInstalled);

    // Set desired instances

    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});

    // Set desired unit subjects

    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    std::condition_variable cv;
    std::mutex              mutex;
    bool                    cancel          = false;
    bool                    downloadStarted = false;

    EXPECT_CALL(mImageManagerMock, DownloadUpdateItems(_, _, _, _))
        .WillOnce(Invoke([&](const Array<UpdateItemInfo>&, const Array<crypto::CertificateInfo>&,
                             const Array<crypto::CertificateChainInfo>&, Array<UpdateItemStatus>&) -> Error {
            std::unique_lock lock(mutex);

            downloadStarted = true;
            cv.notify_one();

            EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return cancel; }));

            return ErrorEnum::eCanceled;
        }))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, Cancel()).WillOnce(Invoke([&]() {
        std::unique_lock lock(mutex);

        cancel = true;
        cv.notify_one();

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    // wait for download to start

    {
        std::unique_lock lock(mutex);

        ASSERT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return downloadStarted; }));
    }

    // Send new desired status to cancel current update

    desiredStatus->mInstances[0].mNumInstances = 2;

    // Set expected instances statuses

    CreateInstancesStatuses(*expectedUnitStatus, "item1", "subject1", "1.0.0", 2);

    // Create launcher run request

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(1);
    EXPECT_CALL(mImageManagerMock, InstallUpdateItems(desiredStatus->mUpdateItems, _)).Times(1);
    EXPECT_CALL(mImageManagerMock, GetUpdateItemsStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUpdateItems.GetValue()), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillOnce(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        return ErrorEnum::eNone;
    }));

    err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
}

TEST_F(UpdateManagerTest, ResumeUpdateAfterRestart)
{
    auto err = mUpdateManager.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop update manager: " << tests::utils::ErrorToStr(err);

    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 2, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Store desired status and update state to simulate restart during downloading

    err = mStorageStub.StoreDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to save desired status: " << tests::utils::ErrorToStr(err);

    err = mStorageStub.StoreUpdateState(UpdateStateEnum::eDownloading);
    EXPECT_TRUE(err.IsNone()) << "Failed to save update state: " << tests::utils::ErrorToStr(err);

    std::condition_variable cv;
    std::mutex              mutex;
    bool                    continueUpdate  = false;
    bool                    downloadStarted = false;

    EXPECT_CALL(mImageManagerMock, DownloadUpdateItems(_, _, _, _))
        .WillOnce(Invoke([&](const Array<UpdateItemInfo>&, const Array<crypto::CertificateInfo>&,
                             const Array<crypto::CertificateChainInfo>&, Array<UpdateItemStatus>&) -> Error {
            std::unique_lock lock(mutex);

            downloadStarted = true;
            cv.notify_one();

            EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return continueUpdate; }));

            return ErrorEnum::eNone;
        }));

    err = mUpdateManager.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start update manager: " << tests::utils::ErrorToStr(err);

    // wait for download to start

    {
        std::unique_lock lock(mutex);

        ASSERT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return downloadStarted; }));
    }

    // Notify cloud connection established and wait for unit status

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    CreateUpdateItemStatus(*expectedUnitStatus, "item1", "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", "subject1", "1.0.0", 2);

    // Create launcher run request

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(1);
    EXPECT_CALL(mImageManagerMock, InstallUpdateItems(desiredStatus->mUpdateItems, _)).Times(1);
    EXPECT_CALL(mImageManagerMock, GetUpdateItemsStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUpdateItems.GetValue()), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillOnce(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        return ErrorEnum::eNone;
    }));

    // Continue update

    {
        std::unique_lock lock(mutex);

        continueUpdate = true;
        cv.notify_one();
    }

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
}

} // namespace aos::cm::updatemanager
