/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <core/common/tests/mocks/cloudconnectionmock.hpp>
#include <core/common/tests/mocks/identprovidermock.hpp>
#include <core/common/tests/mocks/instancestatusprovidermock.hpp>
#include <core/common/tests/mocks/itemstatusprovidermock.hpp>
#include <core/common/tests/mocks/nodehandlermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>
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

static StaticArray<UnitInstanceStatus, cMaxNumInstances> sInstanceStatusStorage;

void ResetInstanceStatusStorage()
{
    sInstanceStatusStorage.Clear();
}

void SetNodeInfo(UnitNodeInfo& nodeInfo, const String& nodeID, const String& nodeType,
    const NodeState& state = NodeStateEnum::eProvisioned, bool isConnected = true, Error error = ErrorEnum::eNone)
{
    nodeInfo.mNodeID      = nodeID;
    nodeInfo.mNodeType    = nodeType;
    nodeInfo.mState       = state;
    nodeInfo.mIsConnected = isConnected;
    nodeInfo.mError       = error;

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
    const NodeState& state = NodeStateEnum::eProvisioned, bool isConnected = true, Error error = ErrorEnum::eNone)
{
    if (!unitStatus.mNodes.HasValue()) {
        unitStatus.mNodes.EmplaceValue();
    }

    unitStatus.mNodes->EmplaceBack();

    SetNodeInfo(unitStatus.mNodes->Back(), nodeID, nodeType, state, isConnected, error);
}

void ChangeNodeInfo(UnitStatus& unitStatus, const String& nodeID, const String& nodeType,
    const NodeState& state = NodeStateEnum::eProvisioned, bool isConnected = true, Error error = ErrorEnum::eNone)
{
    auto it = unitStatus.mNodes->FindIf([&nodeID](const UnitNodeInfo& nodeInfo) { return nodeInfo.mNodeID == nodeID; });
    EXPECT_NE(it, unitStatus.mNodes->end());

    SetNodeInfo(*it, nodeID, nodeType, state, isConnected, error);
}

void CreateUpdateItemStatus(UnitStatus& unitStatus, const String& itemID, const UpdateItemType& type,
    const String& version, const ItemState& state = ItemStateEnum::eInstalled, bool preinstalled = false)
{
    if (!unitStatus.mUpdateItems.HasValue()) {
        unitStatus.mUpdateItems.EmplaceValue();
    }

    unitStatus.mUpdateItems->EmplaceBack();

    auto& itemStatus = unitStatus.mUpdateItems->Back();

    itemStatus.mItemID       = itemID;
    itemStatus.mType         = type;
    itemStatus.mVersion      = version;
    itemStatus.mState        = state;
    itemStatus.mPreinstalled = preinstalled;
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

void CreateInstancesStatuses(UnitStatus& unitStatus, const String& itemID, const UpdateItemType& type,
    const String& subjectID, const String& version, size_t numInstances,
    const InstanceState& state = InstanceStateEnum::eActive, bool preinstalled = false)
{
    if (!unitStatus.mInstances.HasValue()) {
        unitStatus.mInstances.EmplaceValue();
    }

    unitStatus.mInstances->EmplaceBack();

    auto& instancesStatuses = unitStatus.mInstances->Back();

    instancesStatuses.mItemID       = itemID;
    instancesStatuses.mType         = type;
    instancesStatuses.mSubjectID    = subjectID;
    instancesStatuses.mVersion      = version;
    instancesStatuses.mPreinstalled = preinstalled;

    for (size_t i = 0; i < numInstances; i++) {
        auto err = sInstanceStatusStorage.EmplaceBack();
        EXPECT_TRUE(err.IsNone());

        auto& instanceStatus = sInstanceStatusStorage.Back();

        instanceStatus                 = {};
        instanceStatus.mInstance       = i;
        instanceStatus.mManifestDigest = "digest1";
        instanceStatus.mNodeID         = "node1";
        instanceStatus.mRuntimeID      = "runtime1";
        instanceStatus.mState          = state;
        instanceStatus.mPreinstalled   = instancesStatuses.mPreinstalled;

        err = instancesStatuses.mInstances.PushBack(&instanceStatus);
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
        auto err = sInstanceStatusStorage.EmplaceBack();
        EXPECT_TRUE(err.IsNone());

        auto& instanceStatus = sInstanceStatusStorage.Back();

        instanceStatus                 = {};
        instanceStatus.mInstance       = i;
        instanceStatus.mManifestDigest = "digest1";
        instanceStatus.mNodeID         = "node1";
        instanceStatus.mRuntimeID      = "runtime1";
        instanceStatus.mState          = state;
        instanceStatus.mPreinstalled   = instancesStatuses.mPreinstalled;

        err = instancesStatuses.mInstances.PushBack(&instanceStatus);
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
    instanceStatus.mPreinstalled   = unitInstanceStatus.mPreinstalled;
};

StaticString<oci::cDigestLen> CreateIndexDigest(const String& itemID, const String& version)
{
    StaticString<oci::cDigestLen> digest;

    digest = (std::string("sha256:") + itemID.CStr() + "-" + version.CStr()).c_str();

    return digest;
}

void CreateUpdateItemInfo(
    DesiredStatus& desiredStatus, const String& itemID, UpdateItemType updateItemType, const String& version)
{
    desiredStatus.mUpdateItems.EmplaceBack();

    auto& itemInfo = desiredStatus.mUpdateItems.Back();

    itemInfo.mItemID      = itemID;
    itemInfo.mType        = updateItemType;
    itemInfo.mVersion     = version;
    itemInfo.mIndexDigest = CreateIndexDigest(itemID, version);
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
            request.mIndexDigest    = CreateIndexDigest(desiredInstance.mItemID, it->mVersion);
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
            status.mType           = unitInstanceStatus.mType;
            status.mSubjectID      = unitInstanceStatus.mSubjectID;
            status.mVersion        = unitInstanceStatus.mVersion;
            status.mInstance       = instanceStatus->mInstance;
            status.mNodeID         = instanceStatus->mNodeID;
            status.mRuntimeID      = instanceStatus->mRuntimeID;
            status.mManifestDigest = instanceStatus->mManifestDigest;
            status.mState          = instanceStatus->mState;
            status.mError          = instanceStatus->mError;
            status.mPreinstalled   = unitInstanceStatus.mPreinstalled;

            instancesStatuses.PushBack(status);
        }
    }
}

void SetInstancesFailed(Array<InstanceStatus>& instances, const String& itemID, const Error& error)
{
    for (auto& instance : instances) {
        if (instance.mItemID == itemID) {
            instance.mState = InstanceStateEnum::eFailed;
            instance.mError = error;
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
        ResetInstanceStatusStorage();

        Config config {cUnitStatusSendTimeout};

        auto err = mUpdateManager.Init(mAllocator, config, mIdentProviderMock, mNodeHandlerMock, mUnitConfigMock,
            mNodeInfoProviderMock, mImageManagerMock, mItemStatusProviderMock, mLauncherMock, mCloudConnectionMock,
            mSenderStub, mStorageStub);
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
        EXPECT_CALL(mItemStatusProviderMock, SubscribeListener(_))
            .WillRepeatedly(Invoke([&](common::imagemanager::ItemStatusListenerItf& listener) {
                mItemStatusListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mLauncherMock, SubscribeListener(_))
            .WillRepeatedly(Invoke([&](instancestatusprovider::ListenerItf& listener) {
                mInstanceStatusListener = &listener;
                mInstanceStatusListeners.push_back(&listener);

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

    void TearDown() override { StopUpdateManager(); }

    void StopUpdateManager()
    {
        if (mUpdateManagerStopped) {
            return;
        }

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
        EXPECT_CALL(mItemStatusProviderMock, UnsubscribeListener(_))
            .WillOnce(Invoke([&](common::imagemanager::ItemStatusListenerItf& listener) {
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

        mUpdateManagerStopped = true;
    }

    void NotifyInstancesStatusesChanged(const Array<InstanceStatus>& statuses)
    {
        for (auto* listener : mInstanceStatusListeners) {
            listener->OnInstancesStatusesChanged(statuses);
        }
    }

    void NotifyInstancesStatusesChanged(const UnitStatus& unitStatus)
    {
        auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

        ConvertInstancesStatuses(unitStatus.mInstances.GetValue(), *statuses);

        NotifyInstancesStatusesChanged(*statuses);
    }

    std::unique_ptr<UnitStatus> WaitSendFullUnitStatus()
    {
        while (true) {
            auto unitStatus = std::make_unique<UnitStatus>(mSenderStub.WaitSendUnitStatus());
            if (!unitStatus->mIsDeltaInfo) {
                return unitStatus;
            }
        }
    }

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    HeapAllocator mAllocator;

    UpdateManager                                          mUpdateManager;
    NiceMock<iamclient::IdentProviderMock>                 mIdentProviderMock;
    NiceMock<iamclient::NodeHandlerMock>                   mNodeHandlerMock;
    NiceMock<unitconfig::UnitConfigMock>                   mUnitConfigMock;
    NiceMock<nodeinfoprovider::NodeInfoProviderMock>       mNodeInfoProviderMock;
    NiceMock<imagemanager::ImageManagerMock>               mImageManagerMock;
    NiceMock<common::imagemanager::ItemStatusProviderMock> mItemStatusProviderMock;
    NiceMock<launcher::LauncherMock>                       mLauncherMock;
    NiceMock<cloudconnection::CloudConnectionMock>         mCloudConnectionMock;
    SenderStub                                             mSenderStub;
    StorageStub                                            mStorageStub;

    cloudconnection::ConnectionListenerItf*      mConnectionListener {};
    nodeinfoprovider::NodeInfoListenerItf*       mNodeInfoListener {};
    common::imagemanager::ItemStatusListenerItf* mItemStatusListener {};
    instancestatusprovider::ListenerItf*         mInstanceStatusListener {};
    iamclient::SubjectsListenerItf*              mSubjectsListener {};

    std::vector<instancestatusprovider::ListenerItf*> mInstanceStatusListeners;
    bool                                              mUpdateManagerStopped {};
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

    CreateUpdateItemStatus(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemStatus(*expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemStatus(*expectedUnitStatus, "item3", UpdateItemTypeEnum::eComponent, "1.0.0");

    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUpdateItems.GetValue()), Return(ErrorEnum::eNone)));

    // Set instances statuses

    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 2);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "subject2", "1.0.0", 3);
    CreateInstancesStatuses(*expectedUnitStatus, "item3", UpdateItemTypeEnum::eComponent, "subject3", "1.0.0", 1);

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

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item3", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalling);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item4", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalling);

    // Notify items statuses changed

    for (const auto& itemStatus : *expectedUnitStatus->mUpdateItems) {
        mItemStatusListener->OnItemStatusChanged(itemStatus);
    }

    ChangeUpdateItemStatus(*expectedUnitStatus, "item3", "1.0.0", ItemStateEnum::eInstalled);
    ChangeUpdateItemStatus(*expectedUnitStatus, "item4", "1.0.0", ItemStateEnum::eInstalled);

    // Notify items statuses changed

    for (const auto& itemStatus : *expectedUnitStatus->mUpdateItems) {
        mItemStatusListener->OnItemStatusChanged(itemStatus);
    }

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    ClearUnitStatus(*expectedUnitStatus);

    // Set instances statuses

    expectedUnitStatus->mIsDeltaInfo = true;

    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 4,
        InstanceStateEnum::eActivating);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 3,
        InstanceStateEnum::eActivating);

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    for (const auto& instancesStatuses : *expectedUnitStatus->mInstances) {
        for (const auto& instanceStatus : instancesStatuses.mInstances) {
            auto err = statuses->EmplaceBack();
            EXPECT_TRUE(err.IsNone());

            CreateInstanceStatus(statuses->Back(), instancesStatuses.mItemID, instancesStatuses.mSubjectID,
                instanceStatus->mInstance, instancesStatuses.mVersion, *instanceStatus);
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
                instanceStatus->mInstance, instancesStatuses.mVersion, *instanceStatus);
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

TEST_F(UpdateManagerTest, SendUnitStatusWithPreinstalledItems)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Set update items

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled, true);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled, true);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item3", UpdateItemTypeEnum::eComponent, "1.0.0", ItemStateEnum::eInstalled, true);

    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1,
        InstanceStateEnum::eActive, true);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1,
        InstanceStateEnum::eActive, true);
    CreateInstancesStatuses(*expectedUnitStatus, "item3", UpdateItemTypeEnum::eComponent, "subject2", "1.0.0", 1,
        InstanceStateEnum::eActive, true);

    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillOnce(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        return ErrorEnum::eNone;
    }));

    // Notify cloud connection established

    mConnectionListener->OnConnect();

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

    desiredStatus->mUnitConfig.EmplaceValue(UnitConfig {"1.0.0", "2.0.0", {}});

    // Set desired update items

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemInfo(*desiredStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemInfo(*desiredStatus, "item3", UpdateItemTypeEnum::eService, "1.0.0");

    // Set desired certificates

    desiredStatus->mCertificates.EmplaceBack();
    desiredStatus->mCertificates.Back().mFingerprint = "fingerprint1";

    desiredStatus->mCertificateChains.EmplaceBack();
    desiredStatus->mCertificateChains.Back().mName = "chain1";
    desiredStatus->mCertificateChains.Back().mFingerprints.EmplaceBack("fingerprint1");

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

    CreateUnitConfigStatus(*expectedUnitStatus, "2.0.0");

    // Set expected update items status

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item3", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);

    // Set expected instances statuses

    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "subject2", "1.0.0", 2);
    CreateInstancesStatuses(*expectedUnitStatus, "item3", UpdateItemTypeEnum::eService, "subject3", "1.0.0", 3);

    EXPECT_CALL(mNodeHandlerMock, PauseNode(String("node1"))).Times(1);
    EXPECT_CALL(mNodeHandlerMock, ResumeNode(String("node2"))).Times(1);
    EXPECT_CALL(mUnitConfigMock, CheckUnitConfig(desiredStatus->mUnitConfig.GetValue())).Times(1);
    EXPECT_CALL(mUnitConfigMock, UpdateUnitConfig(desiredStatus->mUnitConfig.GetValue())).Times(1);
    EXPECT_CALL(mImageManagerMock, SetCerts(desiredStatus->mCertificates, desiredStatus->mCertificateChains))
        .WillOnce(Return(ErrorEnum::eNone));

    for (const auto& item : desiredStatus->mUpdateItems) {
        EXPECT_CALL(mImageManagerMock, InstallItem(item)).WillOnce(Return(ErrorEnum::eNone));
    }

    std::atomic_bool instancesLaunched {false};

    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        instancesLaunched = true;

        return ErrorEnum::eNone;
    }));

    // Set stored items: item4 is not in desired status and should be removed, item5 is already removed

    std::atomic_bool item4Removed {false};

    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mImageManagerMock, RemoveItem(String("item4"), String("1.0.0"))).WillOnce(InvokeWithoutArgs([&]() {
        item4Removed = true;

        return ErrorEnum::eNone;
    }));

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
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            // Items are reported only after install, otherwise they are skipped by install step

            if (!instancesLaunched) {
                return ErrorEnum::eNone;
            }

            statuses = expectedUnitStatus->mUpdateItems.GetValue();

            if (!item4Removed) {
                statuses.EmplaceBack(UpdateItemStatus {"item4", UpdateItemTypeEnum::eService, "1.0.0", false,
                    ItemStateEnum::eInstalled, ErrorEnum::eNone});
            }

            statuses.EmplaceBack(UpdateItemStatus {
                "item5", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eRemoved, ErrorEnum::eNone});

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_))
        .Times(2)
        .WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
            for (const auto& instancesStatuses : expectedUnitStatus->mInstances.GetValue()) {
                for (const auto& instanceStatus : instancesStatuses.mInstances) {
                    InstanceStatus status;

                    status.mItemID         = instancesStatuses.mItemID;
                    status.mSubjectID      = instancesStatuses.mSubjectID;
                    status.mVersion        = instancesStatuses.mVersion;
                    status.mInstance       = instanceStatus->mInstance;
                    status.mNodeID         = instanceStatus->mNodeID;
                    status.mRuntimeID      = instanceStatus->mRuntimeID;
                    status.mManifestDigest = instanceStatus->mManifestDigest;
                    status.mState          = instanceStatus->mState;

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
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);

    // Set desired instances

    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});

    // Set desired unit subjects

    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    std::condition_variable cv;
    std::mutex              mutex;
    bool                    cancel         = false;
    bool                    installStarted = false;

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0]))
        .WillOnce(Invoke([&](const UpdateItemInfo&) -> Error {
            std::unique_lock lock(mutex);

            installStarted = true;
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

    // wait for install to start

    {
        std::unique_lock lock(mutex);

        ASSERT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return installStarted; }));
    }

    // Send new desired status to cancel current update

    desiredStatus->mInstances[0].mNumInstances = 2;

    // Set expected instances statuses

    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 2);

    // Create launcher run request

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    std::atomic_bool instancesLaunched {false};

    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        instancesLaunched = true;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (instancesLaunched) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();

                return ErrorEnum::eNone;
            }

            // The canceled install is reported as failed with the canceled error and has to be installed again

            statuses.EmplaceBack(
                UpdateItemStatus {desiredStatus->mUpdateItems[0].mItemID, desiredStatus->mUpdateItems[0].mType,
                    desiredStatus->mUpdateItems[0].mVersion, false, ItemStateEnum::eFailed, ErrorEnum::eCanceled});

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_))
        .Times(2)
        .WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
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
    bool                    continueUpdate = false;
    bool                    installStarted = false;

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0]))
        .WillOnce(Invoke([&](const UpdateItemInfo&) -> Error {
            std::unique_lock lock(mutex);

            installStarted = true;
            cv.notify_one();

            EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return continueUpdate; }));

            return ErrorEnum::eNone;
        }));

    err = mUpdateManager.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start update manager: " << tests::utils::ErrorToStr(err);

    // wait for install to start

    {
        std::unique_lock lock(mutex);

        ASSERT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return installStarted; }));
    }

    // Notify cloud connection established and wait for unit status

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 2);

    // Create launcher run request

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(1);
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(expectedUnitStatus->mUpdateItems.GetValue()), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_))
        .Times(2)
        .WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
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

TEST_F(UpdateManagerTest, SetUpdateStatuses)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired node states

    desiredStatus->mNodes.EmplaceBack(DesiredNodeStateInfo {"node1", DesiredNodeStateEnum::ePaused});

    // Set desired unit config

    desiredStatus->mUnitConfig.EmplaceValue(UnitConfig {"1.0.0", "2.0.0", {}});

    // Set expected node infos

    CreateNodeInfo(*expectedUnitStatus, "node1", "type1", NodeStateEnum::eProvisioned, true, ErrorEnum::eFailed);

    // Set expected unit config status

    expectedUnitStatus->mUnitConfig.EmplaceValue();
    expectedUnitStatus->mUnitConfig->EmplaceBack(
        UnitConfigStatus {"1.0.0", UnitConfigStateEnum::eInstalled, ErrorEnum::eNone});
    expectedUnitStatus->mUnitConfig->EmplaceBack(
        UnitConfigStatus {"2.0.0", UnitConfigStateEnum::eFailed, ErrorEnum::eFailed});

    // Expect calls

    EXPECT_CALL(mNodeHandlerMock, PauseNode(String("node1"))).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mUnitConfigMock, CheckUnitConfig(desiredStatus->mUnitConfig.GetValue())).Times(1);
    EXPECT_CALL(mUnitConfigMock, UpdateUnitConfig(desiredStatus->mUnitConfig.GetValue()))
        .WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mNodeInfoProviderMock, GetAllNodeIDs(_)).WillOnce(Invoke([&](Array<StaticString<cIDLen>>& nodeIDs) {
        nodeIDs.EmplaceBack("node1");

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mNodeInfoProviderMock, GetNodeInfo(String("node1"), _))
        .WillOnce(DoAll(SetArgReferee<1>(expectedUnitStatus->mNodes.GetValue()[0]), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mUnitConfigMock, GetUnitConfigStatus(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUnitConfig.GetValue()[0]), Return(ErrorEnum::eNone)));

    // Send desired status to set statuses

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    EmptyUnitStatus(*expectedUnitStatus);

    // Set expected node infos

    CreateNodeInfo(*expectedUnitStatus, "node1", "type1", NodeStateEnum::ePaused);

    // Set expected unit config status

    CreateUnitConfigStatus(*expectedUnitStatus, "2.0.0");

    // Expect calls

    EXPECT_CALL(mNodeHandlerMock, PauseNode(String("node1"))).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mUnitConfigMock, CheckUnitConfig(desiredStatus->mUnitConfig.GetValue())).Times(1);
    EXPECT_CALL(mUnitConfigMock, UpdateUnitConfig(desiredStatus->mUnitConfig.GetValue()))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mNodeInfoProviderMock, GetAllNodeIDs(_)).WillOnce(Invoke([&](Array<StaticString<cIDLen>>& nodeIDs) {
        nodeIDs.EmplaceBack("node1");

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mNodeInfoProviderMock, GetNodeInfo(String("node1"), _))
        .WillOnce(DoAll(SetArgReferee<1>(expectedUnitStatus->mNodes.GetValue()[0]), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mUnitConfigMock, GetUnitConfigStatus(_))
        .WillOnce(DoAll(SetArgReferee<0>(expectedUnitStatus->mUnitConfig.GetValue()[0]), Return(ErrorEnum::eNone)));

    // Send desired status again to update statuses

    err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
}

TEST_F(UpdateManagerTest, InstallItemFailed)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemInfo(*desiredStatus, "item2", UpdateItemTypeEnum::eComponent, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item2", UpdateItemTypeEnum::eComponent, "1.0.0", ItemStateEnum::eFailed);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);

    // Create launcher run request

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[1])).WillOnce(Return(ErrorEnum::eFailed));
    std::atomic_bool instancesLaunched {false};

    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        instancesLaunched = true;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            // Items are reported only after install, otherwise they are skipped by install step

            if (instancesLaunched) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
}

TEST_F(UpdateManagerTest, InstallSkipsInstalledAndFailedItems)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "installed", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemInfo(*desiredStatus, "failed", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemInfo(*desiredStatus, "missing", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemInfo(*desiredStatus, "removed", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"installed", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses

    CreateUpdateItemStatus(
        *expectedUnitStatus, "installed", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "failed", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eFailed);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "missing", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "removed", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "installed", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);

    // Create launcher run request

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls: only missing and removed items are installed

    std::atomic_bool instancesLaunched {false};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(_)).Times(0);
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[2])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[3])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        instancesLaunched = true;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (instancesLaunched) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();

                return ErrorEnum::eNone;
            }

            statuses.EmplaceBack(UpdateItemStatus {"installed", UpdateItemTypeEnum::eService, "1.0.0", false,
                ItemStateEnum::eInstalled, ErrorEnum::eNone});
            statuses.EmplaceBack(UpdateItemStatus {
                "failed", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eFailed, ErrorEnum::eFailed});
            statuses.EmplaceBack(UpdateItemStatus {
                "removed", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eRemoved, ErrorEnum::eNone});

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
}

TEST_F(UpdateManagerTest, FailedItemInDesiredStatusDoesNotRequireUpdate)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");

    // Expect calls: item is failed, it can be retried only after removing it

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(0);
    EXPECT_CALL(mImageManagerMock, InstallItem(_)).Times(0);
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            statuses.EmplaceBack(UpdateItemStatus {
                "item1", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eFailed, ErrorEnum::eFailed});

            return ErrorEnum::eNone;
        }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
}

TEST_F(UpdateManagerTest, ChangedDesiredStatusIsStoredWithoutUpdate)
{
    auto desiredStatus       = std::make_unique<DesiredStatus>();
    auto storedDesiredStatus = std::make_unique<DesiredStatus>();

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mCorrelationID = "correlation1";

    // Expect calls: item is installed, no update is required

    EXPECT_CALL(mImageManagerMock, InstallItem(_)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            statuses.EmplaceBack(UpdateItemStatus {
                "item1", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eInstalled, ErrorEnum::eNone});

            return ErrorEnum::eNone;
        }));

    // The new desired status is stored

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mStorageStub.GetDesiredStatus(*storedDesiredStatus).IsNone());
    EXPECT_EQ(*storedDesiredStatus, *desiredStatus);

    // The same desired status with another protocol part is not stored

    auto sameDesiredStatus = std::make_unique<DesiredStatus>(*desiredStatus);

    sameDesiredStatus->mCorrelationID = "correlation2";

    err = mUpdateManager.ProcessDesiredStatus(*sameDesiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mStorageStub.GetDesiredStatus(*storedDesiredStatus).IsNone());
    EXPECT_EQ(*storedDesiredStatus, *desiredStatus);

    // The changed desired status which requires no update is stored

    auto changedDesiredStatus = std::make_unique<DesiredStatus>(*sameDesiredStatus);

    changedDesiredStatus->mCertificates.EmplaceBack();
    changedDesiredStatus->mCertificates.Back().mFingerprint = "fingerprint1";

    err = mUpdateManager.ProcessDesiredStatus(*changedDesiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(mStorageStub.GetDesiredStatus(*storedDesiredStatus).IsNone());
    EXPECT_EQ(*storedDesiredStatus, *changedDesiredStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
}

TEST_F(UpdateManagerTest, CanceledItemInDesiredStatusIsInstalled)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");

    // Set expected statuses

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);

    // Expect calls: only the canceled item state requires an update

    std::atomic_bool itemInstalled {false};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0])).WillOnce(InvokeWithoutArgs([&]() {
        itemInstalled = true;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            statuses.EmplaceBack(UpdateItemStatus {"item1", UpdateItemTypeEnum::eService, "1.0.0", false,
                itemInstalled ? ItemStateEnum::eInstalled : ItemStateEnum::eFailed,
                itemInstalled ? ErrorEnum::eNone : ErrorEnum::eCanceled});

            return ErrorEnum::eNone;
        }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
}

TEST_F(UpdateManagerTest, FailedItemNotInDesiredStatusIsRemoved)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Expect calls: desired status without failed item removes it

    std::atomic_bool itemRemoved {false};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(_)).Times(0);
    EXPECT_CALL(mImageManagerMock, RemoveItem(String("item1"), String("1.0.0"))).WillOnce(InvokeWithoutArgs([&]() {
        itemRemoved = true;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (!itemRemoved) {
                statuses.EmplaceBack(UpdateItemStatus {
                    "item1", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eFailed, ErrorEnum::eFailed});
            }

            return ErrorEnum::eNone;
        }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
}

TEST_F(UpdateManagerTest, RecoverItemsWithIntegrityError)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemInfo(*desiredStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 2, {}});
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item2", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 2);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls: item1 has an instance failed with the integrity error, it stays installed and it is recovered after
    // the update by the changed instances statuses. item2 failed with another error is set to failed by the update and
    // is not recovered.

    std::atomic_int  numRuns {0};
    std::vector<int> item1InstallRuns;

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0]))
        .Times(2)
        .WillRepeatedly(InvokeWithoutArgs([&]() {
            item1InstallRuns.push_back(numRuns);

            (*expectedUnitStatus->mUpdateItems)[0].mState = ItemStateEnum::eInstalled;
            (*expectedUnitStatus->mUpdateItems)[0].mError = ErrorEnum::eNone;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[1])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item2"), String("1.0.0"), _))
        .WillOnce(Invoke([&](const String&, const String&, const Error& error) {
            EXPECT_TRUE(error.Is(ErrorEnum::eFailed));

            (*expectedUnitStatus->mUpdateItems)[1].mState = ItemStateEnum::eFailed;
            (*expectedUnitStatus->mUpdateItems)[1].mError = error;

            return ErrorEnum::eNone;
        }));
    auto getInstancesStatuses = [&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        if (numRuns == 1) {
            // The first item1 instance error is overridden by the integrity error of the second one

            SetInstancesFailed(instances, "item1", ErrorEnum::eIntegrity);
            instances[0].mError = ErrorEnum::eFailed;

            SetInstancesFailed(instances, "item2", ErrorEnum::eFailed);
        }

        return ErrorEnum::eNone;
    };

    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(2).WillRepeatedly(InvokeWithoutArgs([&]() {
        numRuns++;

        // The launcher reports the changed instances statuses

        auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

        getInstancesStatuses(*statuses);
        NotifyInstancesStatusesChanged(*statuses);

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (numRuns > 0) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke(getInstancesStatuses));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    // The unit status is sent after the update and after the recovery

    WaitSendFullUnitStatus();

    EXPECT_EQ(*WaitSendFullUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(item1InstallRuns, std::vector<int>({0, 1}));
}

TEST_F(UpdateManagerTest, OtherVersionInstanceDoesNotRecoverItems)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "2.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses: the instance reports another version than the desired item

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "2.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1, InstanceStateEnum::eFailed);

    sInstanceStatusStorage.Back().mError = ErrorEnum::eIntegrity;

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls: the update is finished without recovering

    std::atomic_bool instancesLaunched {false};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).Times(0);
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        instancesLaunched = true;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (instancesLaunched) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
}

TEST_F(UpdateManagerTest, PartiallyFailedInstances)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    CreateUpdateItemInfo(*desiredStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 2, {}});
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item2", "subject1", 0, 2, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 2);
    CreateInstancesStatuses(*expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 2);

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls: item1 has one instance failed with a non integrity error and item2 has one instance failed with the
    // integrity error. The update sets none of them to failed: item1 has active instances and item2 is recovered only
    // after the update by the changed instances statuses.

    std::atomic_int numRuns {0};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[1])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).Times(0);
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        numRuns++;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (numRuns > 0) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        if (numRuns == 1) {
            instances[0].mState = InstanceStateEnum::eFailed;
            instances[0].mError = ErrorEnum::eFailed;
            instances[3].mState = InstanceStateEnum::eFailed;
            instances[3].mError = ErrorEnum::eIntegrity;
        }

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(WaitSendFullUnitStatus()->mUpdateItems, expectedUnitStatus->mUpdateItems);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
}

TEST_F(UpdateManagerTest, ResumeRecoveringAfterRestart)
{
    auto err = mUpdateManager.Stop();
    EXPECT_TRUE(err.IsNone()) << "Failed to stop update manager: " << tests::utils::ErrorToStr(err);

    auto failedUnitStatus = std::make_unique<UnitStatus>();
    auto activeUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus    = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*failedUnitStatus);
    EmptyUnitStatus(*activeUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses: the item is installed and its instance is failed with the integrity error

    CreateUpdateItemStatus(
        *failedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(
        *failedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1, InstanceStateEnum::eFailed);

    sInstanceStatusStorage.Back().mError = ErrorEnum::eIntegrity;

    CreateUpdateItemStatus(
        *activeUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*activeUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);

    // Store desired status and update state to simulate restart during recovering

    err = mStorageStub.StoreDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to save desired status: " << tests::utils::ErrorToStr(err);

    err = mStorageStub.StoreUpdateState(UpdateStateEnum::eRecovering);
    EXPECT_TRUE(err.IsNone()) << "Failed to save update state: " << tests::utils::ErrorToStr(err);

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    std::condition_variable cv;
    std::mutex              mutex;
    bool                    continueUpdate = false;
    bool                    installStarted = false;
    std::atomic_bool        instancesLaunched {false};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0]))
        .WillOnce(Invoke([&](const UpdateItemInfo&) -> Error {
            std::unique_lock lock(mutex);

            installStarted = true;
            cv.notify_one();

            EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return continueUpdate; }));

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        instancesLaunched = true;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            statuses = instancesLaunched ? activeUnitStatus->mUpdateItems.GetValue()
                                         : failedUnitStatus->mUpdateItems.GetValue();

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        const auto& status = instancesLaunched ? activeUnitStatus : failedUnitStatus;

        ConvertInstancesStatuses(status->mInstances.GetValue(), instances);

        return ErrorEnum::eNone;
    }));

    err = mUpdateManager.Start();
    EXPECT_TRUE(err.IsNone()) << "Failed to start update manager: " << tests::utils::ErrorToStr(err);

    // wait for install to start

    {
        std::unique_lock lock(mutex);

        ASSERT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return installStarted; }));
    }

    // Notify cloud connection established and wait for unit status

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *failedUnitStatus);

    // Continue update

    {
        std::unique_lock lock(mutex);

        continueUpdate = true;
        cv.notify_one();
    }

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *activeUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
}

TEST_F(UpdateManagerTest, RecoverItemsOnInstanceStatusWithIntegrityError)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls: the item is installed and the instances are launched by the update and by the recovery

    std::atomic_int          numRuns {0};
    std::vector<UpdateState> installStates;

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0]))
        .Times(2)
        .WillRepeatedly(InvokeWithoutArgs([&]() {
            installStates.push_back(mStorageStub.GetUpdateState().mValue);

            expectedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eInstalled;
            expectedUnitStatus->mUpdateItems->Back().mError = ErrorEnum::eNone;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).Times(0);
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(2).WillRepeatedly(InvokeWithoutArgs([&]() {
        sInstanceStatusStorage.Back().mState = InstanceStateEnum::eActive;
        sInstanceStatusStorage.Back().mError = ErrorEnum::eNone;

        numRuns++;

        // The launcher reports the instances statuses while the update is running

        NotifyInstancesStatusesChanged(StaticArray<InstanceStatus, 1>());

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (numRuns > 0) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        if (numRuns > 0) {
            ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);
        }

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(*WaitSendFullUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(numRuns, 1);

    // The instance fails with the integrity error while no update is running and the launcher reports it

    sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
    sInstanceStatusStorage.Back().mError = Error(ErrorEnum::eIntegrity, EIO);

    NotifyInstancesStatusesChanged(*expectedUnitStatus);

    EXPECT_EQ(*WaitSendFullUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(numRuns, 2);
    EXPECT_EQ(installStates, std::vector<UpdateState>({UpdateStateEnum::eDownloading, UpdateStateEnum::eRecovering}));
    EXPECT_EQ(expectedUnitStatus->mUpdateItems->Back().mState, ItemStateEnum::eInstalled);
    EXPECT_EQ(sInstanceStatusStorage.Back().mState, InstanceStateEnum::eActive);

    StopUpdateManager();
}

TEST_F(UpdateManagerTest, RecoverItemsOnInstanceStatusAfterDesiredStatus)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1,
        InstanceStateEnum::eActivating);

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls: the item is already installed, the instances are activating after the restart and are launched
    // only by the recovery

    std::atomic_int          numRuns {0};
    std::atomic_int          numInstalls {0};
    std::vector<UpdateState> installStates;

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0])).WillOnce(InvokeWithoutArgs([&]() {
        installStates.push_back(mStorageStub.GetUpdateState().mValue);

        expectedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eInstalled;
        expectedUnitStatus->mUpdateItems->Back().mError = ErrorEnum::eNone;

        numInstalls++;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).Times(0);
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        sInstanceStatusStorage.Back().mState = InstanceStateEnum::eActive;
        sInstanceStatusStorage.Back().mError = ErrorEnum::eNone;

        numRuns++;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (true) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        if (true) {
            ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);
        }

        return ErrorEnum::eNone;
    }));

    // The desired status arrives before the node reports the instances statuses

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(numInstalls, 0);
    EXPECT_EQ(numRuns, 0);

    auto storedDesiredStatus = std::make_unique<DesiredStatus>();

    err = mStorageStub.GetDesiredStatus(*storedDesiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to get desired status: " << tests::utils::ErrorToStr(err);
    EXPECT_EQ(storedDesiredStatus->mUpdateItems, desiredStatus->mUpdateItems);

    // The node reports the instance failed with the integrity error

    sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
    sInstanceStatusStorage.Back().mError = Error(ErrorEnum::eIntegrity, EIO);

    NotifyInstancesStatusesChanged(*expectedUnitStatus);

    EXPECT_EQ(*WaitSendFullUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(numInstalls, 1);
    EXPECT_EQ(numRuns, 1);
    EXPECT_EQ(installStates, std::vector<UpdateState>({UpdateStateEnum::eRecovering}));
    EXPECT_EQ(sInstanceStatusStorage.Back().mState, InstanceStateEnum::eActive);
}

TEST_F(UpdateManagerTest, RecoverItemsOnInstanceStatusFailsAgain)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls: the instances launched by the recoveries fail with the integrity error again

    const auto integrityErr = Error(ErrorEnum::eIntegrity, EIO);

    std::atomic_int numRuns {0};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(3).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0]))
        .Times(3)
        .WillRepeatedly(InvokeWithoutArgs([&]() {
            expectedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eInstalled;
            expectedUnitStatus->mUpdateItems->Back().mError = ErrorEnum::eNone;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).Times(0);
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(3).WillRepeatedly(InvokeWithoutArgs([&]() {
        if (++numRuns == 1) {
            sInstanceStatusStorage.Back().mState = InstanceStateEnum::eActive;
            sInstanceStatusStorage.Back().mError = ErrorEnum::eNone;
        } else {
            sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
            sInstanceStatusStorage.Back().mError = integrityErr;
        }

        // The launcher reports the instances statuses while the update is running

        NotifyInstancesStatusesChanged(*expectedUnitStatus);

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (numRuns > 0) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        if (numRuns > 0) {
            ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);
        }

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(*WaitSendFullUnitStatus(), *expectedUnitStatus);

    // The instance fails with the integrity error while no update is running and the launcher reports it

    sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
    sInstanceStatusStorage.Back().mError = integrityErr;

    NotifyInstancesStatusesChanged(*expectedUnitStatus);

    // The recovery fails: the item stays installed and the failed instance is reported. The instances statuses changed
    // by the recovery itself don't start the recovery again.

    EXPECT_EQ(*WaitSendFullUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(expectedUnitStatus->mUpdateItems->Back().mState, ItemStateEnum::eInstalled);
    EXPECT_EQ(sInstanceStatusStorage.Back().mState, InstanceStateEnum::eFailed);
    EXPECT_EQ(numRuns, 2);

    // A new instances statuses change starts the recovery again

    NotifyInstancesStatusesChanged(*expectedUnitStatus);

    EXPECT_EQ(*WaitSendFullUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(numRuns, 3);

    StopUpdateManager();
}

TEST_F(UpdateManagerTest, OtherErrorInstanceStatusDoesNotRecoverItems)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls: the item is installed and the instances are launched only by the update

    std::atomic_bool instancesLaunched {false};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).Times(0);
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        instancesLaunched = true;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (instancesLaunched) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        if (instancesLaunched) {
            ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);
        }

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(*WaitSendFullUnitStatus(), *expectedUnitStatus);

    // The instance fails with a non integrity error while no update is running and the launcher reports it: the
    // recovery is not started

    sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
    sInstanceStatusStorage.Back().mError = Error(ErrorEnum::eFailed, EIO);

    NotifyInstancesStatusesChanged(*expectedUnitStatus);

    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
}

TEST_F(UpdateManagerTest, NewDesiredStatusCancelsRecovering)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 2);

    std::condition_variable cv;
    std::mutex              mutex;
    int                     numInstalls = 0;
    bool                    canceled    = false;
    bool                    recovering  = false;
    std::atomic_int         numRuns {0};

    // Expect calls: install, canceled reinstall and install of the canceled item by the new update

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(3).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(_)).Times(3).WillRepeatedly(Invoke([&](const UpdateItemInfo&) -> Error {
        std::unique_lock lock(mutex);

        numInstalls++;

        if (numInstalls != 2) {
            return ErrorEnum::eNone;
        }

        recovering = true;
        cv.notify_all();

        EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return canceled; }));

        return ErrorEnum::eCanceled;
    }));
    EXPECT_CALL(mImageManagerMock, Cancel()).WillOnce(Invoke([&]() {
        std::unique_lock lock(mutex);

        canceled = true;
        cv.notify_all();

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).Times(0);
    EXPECT_CALL(mLauncherMock, RunInstances(_, _)).Times(2).WillRepeatedly(InvokeWithoutArgs([&]() {
        // The launcher reports the instances failed by the first launch, which starts the recovery after the update

        if (++numRuns == 1) {
            auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

            ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), *statuses);
            SetInstancesFailed(*statuses, "item1", ErrorEnum::eIntegrity);

            NotifyInstancesStatusesChanged(*statuses);
        }

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            std::unique_lock lock(mutex);

            if (numInstalls == 0) {
                return ErrorEnum::eNone;
            }

            statuses = expectedUnitStatus->mUpdateItems.GetValue();

            if (canceled && numInstalls < 3) {
                statuses[0].mState = ItemStateEnum::eFailed;
                statuses[0].mError = ErrorEnum::eCanceled;
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        if (numRuns == 1) {
            SetInstancesFailed(instances, "item1", ErrorEnum::eIntegrity);
        }

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    // Wait for recovering to start

    {
        std::unique_lock lock(mutex);

        ASSERT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return recovering; }));
    }

    // The unit status is sent after the first update, before the recovery

    WaitSendFullUnitStatus();

    // Send new desired status to cancel recovering

    desiredStatus->mInstances[0].mNumInstances = 2;

    err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(*WaitSendFullUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
}

TEST_F(UpdateManagerTest, InstallItemsConcurrencyLimit)
{
    constexpr auto cMaxConcurrentItems = static_cast<size_t>(cMaxNumConcurrentItems);
    constexpr auto cNumItems           = cMaxConcurrentItems * 2;

    static_assert(cNumItems <= cMaxNumUpdateItems, "too many update items");

    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired update items

    for (size_t i = 0; i < cNumItems; i++) {
        CreateUpdateItemInfo(
            *desiredStatus, ("item" + std::to_string(i)).c_str(), UpdateItemTypeEnum::eService, "1.0.0");
    }

    std::condition_variable cv;
    std::mutex              mutex;
    size_t                  numStarted = 0;
    size_t                  numActive  = 0;
    size_t                  maxActive  = 0;
    std::set<std::string>   installedItems;

    EXPECT_CALL(mImageManagerMock, InstallItem(_))
        .Times(cNumItems)
        .WillRepeatedly(Invoke([&](const UpdateItemInfo& item) -> Error {
            std::unique_lock lock(mutex);

            numStarted++;
            numActive++;
            maxActive = std::max(maxActive, numActive);

            cv.notify_all();

            EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return numStarted >= cMaxConcurrentItems; }));

            cv.wait_for(lock, std::chrono::milliseconds(100), [&]() { return numStarted > cMaxConcurrentItems; });

            installedItems.insert(item.mItemID.CStr());
            numActive--;

            return ErrorEnum::eNone;
        }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    std::unique_lock lock(mutex);

    EXPECT_EQ(maxActive, cMaxConcurrentItems);
    EXPECT_EQ(installedItems.size(), cNumItems);
}

TEST_F(UpdateManagerTest, RemoveItemsConcurrently)
{
    constexpr auto cMaxConcurrentItems = static_cast<size_t>(cMaxNumConcurrentItems);
    constexpr auto cNumItems           = static_cast<size_t>(cMaxNumUpdateItems) * 2;

    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    std::condition_variable cv;
    std::mutex              mutex;
    size_t                  numStarted = 0;
    size_t                  numActive  = 0;
    size_t                  maxActive  = 0;
    std::set<std::string>   removedItems;

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _))
        .Times(cNumItems)
        .WillRepeatedly(Invoke([&](const String& itemID, const String&) -> Error {
            std::unique_lock lock(mutex);

            numStarted++;
            numActive++;
            maxActive = std::max(maxActive, numActive);

            cv.notify_all();

            EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return numStarted >= cMaxConcurrentItems; }));

            // Keep the first items active, so tasks are queued up to the full queue size

            cv.wait_for(lock, std::chrono::milliseconds(100), [&]() { return numStarted > cMaxConcurrentItems; });

            removedItems.insert(itemID.CStr());
            numActive--;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            std::unique_lock lock(mutex);

            if (numStarted == 0) {
                for (size_t i = 0; i < cNumItems; i++) {
                    statuses.EmplaceBack(UpdateItemStatus {("item" + std::to_string(i)).c_str(),
                        UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eInstalled, ErrorEnum::eNone});
                }
            }

            return ErrorEnum::eNone;
        }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    std::unique_lock lock(mutex);

    EXPECT_EQ(maxActive, cMaxConcurrentItems);
    EXPECT_EQ(removedItems.size(), cNumItems);
}

TEST_F(UpdateManagerTest, FinalizeRemoveItemFails)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses: stale items item2 and item3 fail to be removed and stay installed

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item3", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);

    // Create launcher run request

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0])).WillOnce(Return(ErrorEnum::eNone));
    std::atomic_bool instancesLaunched {false};

    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        instancesLaunched = true;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mImageManagerMock, RemoveItem(String("item2"), String("1.0.0"))).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mImageManagerMock, RemoveItem(String("item3"), String("1.0.0"))).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            // Items are reported only after install, otherwise they are skipped by install step

            if (instancesLaunched) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
}

TEST_F(UpdateManagerTest, FinalizeGetStatusesFails)
{
    auto expectedUnitStatus = std::make_unique<UnitStatus>();
    auto desiredStatus      = std::make_unique<DesiredStatus>();

    EmptyUnitStatus(*expectedUnitStatus);

    // Notify cloud connection established

    mConnectionListener->OnConnect();
    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // Set desired status

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    // Set expected statuses: finalize is aborted, so stale item2 is not removed

    CreateUpdateItemStatus(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateUpdateItemStatus(
        *expectedUnitStatus, "item2", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eInstalled);
    CreateInstancesStatuses(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1);

    // Create launcher run request

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls: the first get items statuses after launching instances is done by finalize and fails

    std::atomic_bool instancesLaunched {false};
    std::atomic_bool finalizeFailed {false};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).WillOnce(InvokeWithoutArgs([&]() {
        instancesLaunched = true;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) -> Error {
            // Items are reported only after install, otherwise they are skipped by install step

            if (!instancesLaunched) {
                return ErrorEnum::eNone;
            }

            if (!finalizeFailed.exchange(true)) {
                return ErrorEnum::eFailed;
            }

            statuses = expectedUnitStatus->mUpdateItems.GetValue();

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
    EXPECT_TRUE(finalizeFailed);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
}

/***********************************************************************************************************************
 * Error handling suite
 **********************************************************************************************************************/

namespace {
const auto cPollPeriod = std::chrono::milliseconds(10);

bool WaitForCondition(const std::function<bool()>& condition)
{
    for (auto start = std::chrono::steady_clock::now(); std::chrono::steady_clock::now() - start < cCVTimeout;) {
        if (condition()) {
            return true;
        }

        std::this_thread::sleep_for(cPollPeriod);
    }

    return condition();
}

class ControlledAllocator : public AllocatorItf {
public:
    void* Allocate(size_t size) override
    {
        auto index = mCount++;

        if (mFailAll || index == mFailIndex) {
            return nullptr;
        }

        return std::malloc(size);
    }

    void Free(void* data) override { std::free(data); }

    void FailAt(int index)
    {
        mCount     = 0;
        mFailIndex = index;
        mFailAll   = false;
    }

    void FailAll()
    {
        mCount     = 0;
        mFailIndex = -1;
        mFailAll   = true;
    }

    void Reset()
    {
        mCount     = 0;
        mFailIndex = -1;
        mFailAll   = false;
    }

private:
    std::atomic_int  mCount {0};
    std::atomic_int  mFailIndex {-1};
    std::atomic_bool mFailAll {false};
};

class ControlledStorage : public StorageItf {
public:
    Error StoreDesiredStatus(const DesiredStatus& desiredStatus) override
    {
        std::lock_guard lock {mMutex};

        mDesiredStatus = desiredStatus;

        return mStoreDesiredStatusErr;
    }

    Error StoreUpdateState(const UpdateState& state) override
    {
        std::lock_guard lock {mMutex};

        mUpdateState = state;

        if (state == UpdateStateEnum::eRecovering) {
            mNumRecoveries++;
        }

        return mStoreUpdateStateErr;
    }

    Error GetDesiredStatus(DesiredStatus& desiredStatus) override
    {
        std::lock_guard lock {mMutex};

        if (!mGetDesiredStatusErr.IsNone()) {
            return mGetDesiredStatusErr;
        }

        desiredStatus = mDesiredStatus;

        return ErrorEnum::eNone;
    }

    RetWithError<UpdateState> GetUpdateState() override
    {
        std::lock_guard lock {mMutex};

        return RetWithError<UpdateState>(mUpdateState, mGetUpdateStateErr);
    }

    UpdateState GetState()
    {
        std::lock_guard lock {mMutex};

        return mUpdateState;
    }

    void SetState(UpdateState state)
    {
        std::lock_guard lock {mMutex};

        mUpdateState = state;
    }

    size_t GetNumRecoveries()
    {
        std::lock_guard lock {mMutex};

        return mNumRecoveries;
    }

    void SetErrors(const Error& storeDesiredStatusErr, const Error& storeUpdateStateErr,
        const Error& getDesiredStatusErr, const Error& getUpdateStateErr)
    {
        std::lock_guard lock {mMutex};

        mStoreDesiredStatusErr = storeDesiredStatusErr;
        mStoreUpdateStateErr   = storeUpdateStateErr;
        mGetDesiredStatusErr   = getDesiredStatusErr;
        mGetUpdateStateErr     = getUpdateStateErr;
    }

private:
    std::mutex    mMutex;
    DesiredStatus mDesiredStatus;
    UpdateState   mUpdateState;
    size_t        mNumRecoveries {};
    Error         mStoreDesiredStatusErr;
    Error         mStoreUpdateStateErr;
    Error         mGetDesiredStatusErr {ErrorEnum::eNotFound};
    Error         mGetUpdateStateErr;
};

class SenderMock : public SenderItf {
public:
    MOCK_METHOD(Error, SendUnitStatus, (const UnitStatus&), (override));
};

InstanceStatus CreateInstanceStatus(const String& itemID, const String& subjectID, uint64_t instance,
    UpdateItemType type, const String& version, InstanceState state, const Error& error = ErrorEnum::eNone,
    bool preinstalled = false)
{
    InstanceStatus status;

    status.mItemID       = itemID;
    status.mSubjectID    = subjectID;
    status.mInstance     = instance;
    status.mType         = type;
    status.mVersion      = version;
    status.mState        = state;
    status.mError        = error;
    status.mPreinstalled = preinstalled;

    return status;
}

} // namespace

class UpdateManagerErrorTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        ON_CALL(mCloudConnectionMock, SubscribeListener(_))
            .WillByDefault(Invoke([&](cloudconnection::ConnectionListenerItf& listener) {
                mConnectionListener = &listener;

                return ErrorEnum::eNone;
            }));
        ON_CALL(mNodeInfoProviderMock, SubscribeListener(_))
            .WillByDefault(Invoke([&](nodeinfoprovider::NodeInfoListenerItf& listener) {
                mNodeInfoListener = &listener;

                return ErrorEnum::eNone;
            }));
        ON_CALL(mItemStatusProviderMock, SubscribeListener(_))
            .WillByDefault(Invoke([&](common::imagemanager::ItemStatusListenerItf& listener) {
                mItemStatusListener = &listener;

                return ErrorEnum::eNone;
            }));
        ON_CALL(mLauncherMock, SubscribeListener(_))
            .WillByDefault(Invoke([&](instancestatusprovider::ListenerItf& listener) {
                mInstanceStatusListeners.push_back(&listener);

                return ErrorEnum::eNone;
            }));
        ON_CALL(mIdentProviderMock, SubscribeListener(_))
            .WillByDefault(Invoke([&](iamclient::SubjectsListenerItf& listener) {
                mSubjectsListener = &listener;

                return ErrorEnum::eNone;
            }));
    }

    std::unique_ptr<UpdateManager> CreateUpdateManager()
    {
        auto updateManager = std::make_unique<UpdateManager>();

        mInstanceStatusListeners.clear();

        auto err = updateManager->Init(mAllocator, Config {cStatusSendTimeout}, mIdentProviderMock, mNodeHandlerMock,
            mUnitConfigMock, mNodeInfoProviderMock, mImageManagerMock, mItemStatusProviderMock, mLauncherMock,
            mCloudConnectionMock, mSenderMock, mStorage);
        EXPECT_TRUE(err.IsNone()) << "Failed to initialize update manager: " << tests::utils::ErrorToStr(err);

        return updateManager;
    }

    std::unique_ptr<UpdateManager> CreateAndStartUpdateManager()
    {
        auto updateManager = CreateUpdateManager();

        auto err = updateManager->Start();
        EXPECT_TRUE(err.IsNone()) << "Failed to start update manager: " << tests::utils::ErrorToStr(err);

        return updateManager;
    }

    bool WaitUpdateFinished()
    {
        return WaitForCondition([&]() { return mStorage.GetState() == UpdateStateEnum::eNone; });
    }

    void ProcessAndWait(UpdateManager& updateManager, const DesiredStatus& desiredStatus)
    {
        auto err = updateManager.ProcessDesiredStatus(desiredStatus);
        EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

        EXPECT_TRUE(WaitUpdateFinished());
    }

    // Starts the recovery by the instances statuses change and waits until the recovery is finished
    bool RecoverAndWait(const Array<InstanceStatus>& statuses)
    {
        auto numRecoveries = mStorage.GetNumRecoveries();

        DesiredStatusInstanceListener()->OnInstancesStatusesChanged(statuses);

        return WaitForCondition([&]() {
            return mStorage.GetNumRecoveries() > numRecoveries && mStorage.GetState() == UpdateStateEnum::eNone;
        });
    }

    // Desired status handler subscribes to the launcher first
    instancestatusprovider::ListenerItf* DesiredStatusInstanceListener() { return mInstanceStatusListeners.front(); }

    // Unit status handler subscribes to the launcher after the desired status handler
    instancestatusprovider::ListenerItf* UnitStatusInstanceListener() { return mInstanceStatusListeners.back(); }

    static constexpr Duration cStatusSendTimeout = 100 * Time::cMilliseconds;

    ControlledAllocator                                    mAllocator;
    NiceMock<iamclient::IdentProviderMock>                 mIdentProviderMock;
    NiceMock<iamclient::NodeHandlerMock>                   mNodeHandlerMock;
    NiceMock<unitconfig::UnitConfigMock>                   mUnitConfigMock;
    NiceMock<nodeinfoprovider::NodeInfoProviderMock>       mNodeInfoProviderMock;
    NiceMock<imagemanager::ImageManagerMock>               mImageManagerMock;
    NiceMock<common::imagemanager::ItemStatusProviderMock> mItemStatusProviderMock;
    NiceMock<launcher::LauncherMock>                       mLauncherMock;
    NiceMock<cloudconnection::CloudConnectionMock>         mCloudConnectionMock;
    NiceMock<SenderMock>                                   mSenderMock;
    ControlledStorage                                      mStorage;

    cloudconnection::ConnectionListenerItf*           mConnectionListener {};
    nodeinfoprovider::NodeInfoListenerItf*            mNodeInfoListener {};
    common::imagemanager::ItemStatusListenerItf*      mItemStatusListener {};
    iamclient::SubjectsListenerItf*                   mSubjectsListener {};
    std::vector<instancestatusprovider::ListenerItf*> mInstanceStatusListeners;
};

TEST_F(UpdateManagerErrorTest, StartFailsOnSubscribeError)
{
    enum class FailedSubscriber { eDesiredLauncher, eNodeInfo, eItemStatus, eStatusLauncher, eIdent, eCloud };

    for (auto failed : {FailedSubscriber::eDesiredLauncher, FailedSubscriber::eNodeInfo, FailedSubscriber::eItemStatus,
             FailedSubscriber::eStatusLauncher, FailedSubscriber::eIdent, FailedSubscriber::eCloud}) {
        auto updateManager         = CreateUpdateManager();
        auto numLauncherSubscribes = 0;

        EXPECT_CALL(mLauncherMock, SubscribeListener(_)).WillRepeatedly(InvokeWithoutArgs([&]() -> Error {
            numLauncherSubscribes++;

            if ((failed == FailedSubscriber::eDesiredLauncher && numLauncherSubscribes == 1)
                || (failed == FailedSubscriber::eStatusLauncher && numLauncherSubscribes == 2)) {
                return ErrorEnum::eFailed;
            }

            return ErrorEnum::eNone;
        }));
        EXPECT_CALL(mNodeInfoProviderMock, SubscribeListener(_))
            .WillRepeatedly(Return(failed == FailedSubscriber::eNodeInfo ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mItemStatusProviderMock, SubscribeListener(_))
            .WillRepeatedly(Return(failed == FailedSubscriber::eItemStatus ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mIdentProviderMock, SubscribeListener(_))
            .WillRepeatedly(Return(failed == FailedSubscriber::eIdent ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mCloudConnectionMock, SubscribeListener(_))
            .WillRepeatedly(Return(failed == FailedSubscriber::eCloud ? ErrorEnum::eFailed : ErrorEnum::eNone));

        auto err = updateManager->Start();
        EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << "Unexpected error: " << tests::utils::ErrorToStr(err);

        err = updateManager->Stop();

        if (failed == FailedSubscriber::eDesiredLauncher) {
            EXPECT_TRUE(err.Is(ErrorEnum::eWrongState)) << "Unexpected error: " << tests::utils::ErrorToStr(err);
        } else {
            EXPECT_TRUE(err.IsNone()) << "Failed to stop update manager: " << tests::utils::ErrorToStr(err);
        }

        Mock::VerifyAndClearExpectations(&mLauncherMock);
        Mock::VerifyAndClearExpectations(&mNodeInfoProviderMock);
        Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);
        Mock::VerifyAndClearExpectations(&mIdentProviderMock);
        Mock::VerifyAndClearExpectations(&mCloudConnectionMock);
    }
}

TEST_F(UpdateManagerErrorTest, StopFailsOnUnsubscribeError)
{
    enum class FailedSubscriber { eNodeInfo, eItemStatus, eLauncher, eIdent, eCloud };

    for (auto failed : {FailedSubscriber::eNodeInfo, FailedSubscriber::eItemStatus, FailedSubscriber::eLauncher,
             FailedSubscriber::eIdent, FailedSubscriber::eCloud}) {
        auto updateManager = CreateAndStartUpdateManager();

        EXPECT_CALL(mNodeInfoProviderMock, UnsubscribeListener(_))
            .WillRepeatedly(Return(failed == FailedSubscriber::eNodeInfo ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mItemStatusProviderMock, UnsubscribeListener(_))
            .WillRepeatedly(Return(failed == FailedSubscriber::eItemStatus ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mLauncherMock, UnsubscribeListener(_))
            .WillRepeatedly(Return(failed == FailedSubscriber::eLauncher ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mIdentProviderMock, UnsubscribeListener(_))
            .WillRepeatedly(Return(failed == FailedSubscriber::eIdent ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mCloudConnectionMock, UnsubscribeListener(_))
            .WillRepeatedly(Return(failed == FailedSubscriber::eCloud ? ErrorEnum::eFailed : ErrorEnum::eNone));

        auto err = updateManager->Stop();
        EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << "Unexpected error: " << tests::utils::ErrorToStr(err);

        Mock::VerifyAndClearExpectations(&mLauncherMock);
        Mock::VerifyAndClearExpectations(&mNodeInfoProviderMock);
        Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);
        Mock::VerifyAndClearExpectations(&mIdentProviderMock);
        Mock::VerifyAndClearExpectations(&mCloudConnectionMock);
    }
}

TEST_F(UpdateManagerErrorTest, StartWithStorageErrors)
{
    // Getting update state and desired status fails: update manager starts without desired status

    mStorage.SetErrors(ErrorEnum::eNone, ErrorEnum::eNone, ErrorEnum::eFailed, ErrorEnum::eFailed);

    auto updateManager = CreateAndStartUpdateManager();

    EXPECT_EQ(mStorage.GetState(), UpdateStateEnum::eNone);
    EXPECT_TRUE(updateManager->Stop().IsNone());

    // Resume interrupted update while getting desired status fails: update is finished with empty desired status

    mStorage.SetErrors(ErrorEnum::eNone, ErrorEnum::eNone, ErrorEnum::eFailed, ErrorEnum::eNone);
    mStorage.SetState(UpdateStateEnum::eInstalling);

    EXPECT_CALL(mLauncherMock, RunInstances(_, _)).WillOnce(Return(ErrorEnum::eNone));

    updateManager = CreateAndStartUpdateManager();

    EXPECT_TRUE(WaitUpdateFinished());
    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, StoreErrorsDoNotBreakUpdate)
{
    mStorage.SetErrors(ErrorEnum::eFailed, ErrorEnum::eFailed, ErrorEnum::eNotFound, ErrorEnum::eNone);

    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    desiredStatus->mNodes.EmplaceBack(DesiredNodeStateInfo {"node1", DesiredNodeStateEnum::ePaused});
    desiredStatus->mUnitConfig.EmplaceValue();
    desiredStatus->mUnitConfig->mVersion = "2.0.0";

    EXPECT_CALL(mNodeHandlerMock, PauseNode(String("node1"))).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mUnitConfigMock, UpdateUnitConfig(_)).WillOnce(Return(ErrorEnum::eNone));

    ProcessAndWait(*updateManager, *desiredStatus);

    // Node update error is reported in node info until full unit status is sent

    UnitNodeInfo nodeInfo;

    nodeInfo.mNodeID = "node1";

    mNodeInfoListener->OnNodeInfoChanged(nodeInfo);

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIDs;

    nodeIDs.EmplaceBack("node1");

    std::unique_ptr<UnitStatus> sentStatus;

    EXPECT_CALL(mNodeInfoProviderMock, GetAllNodeIDs(_))
        .WillOnce(DoAll(SetArgReferee<0>(nodeIDs), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mNodeInfoProviderMock, GetNodeInfo(String("node1"), _))
        .WillOnce(DoAll(SetArgReferee<1>(nodeInfo), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mUnitConfigMock, GetUnitConfigStatus(_))
        .WillOnce(DoAll(SetArgReferee<0>(UnitConfigStatus {"1.0.0", UnitConfigStateEnum::eInstalled, {}}),
            Return(ErrorEnum::eNone)));
    EXPECT_CALL(mSenderMock, SendUnitStatus(_)).WillOnce(Invoke([&](const UnitStatus& unitStatus) {
        sentStatus = std::make_unique<UnitStatus>(unitStatus);

        return ErrorEnum::eNone;
    }));

    mConnectionListener->OnConnect();

    ASSERT_TRUE(sentStatus);
    ASSERT_TRUE(sentStatus->mNodes.HasValue());
    ASSERT_EQ(sentStatus->mNodes->Size(), 1);
    EXPECT_TRUE(sentStatus->mNodes.GetValue()[0].mError.Is(ErrorEnum::eFailed));
    ASSERT_TRUE(sentStatus->mUnitConfig.HasValue());
    ASSERT_EQ(sentStatus->mUnitConfig->Size(), 2);
    EXPECT_EQ(sentStatus->mUnitConfig.GetValue()[1].mVersion, "2.0.0");

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, UpdateStepsAllocationFailures)
{
    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    desiredStatus->mNodes.EmplaceBack(DesiredNodeStateInfo {"node1", DesiredNodeStateEnum::eProvisioned});

    std::atomic_int numResumes {0};
    std::atomic_int numRuns {0};

    ON_CALL(mNodeHandlerMock, ResumeNode(_)).WillByDefault(InvokeWithoutArgs([&]() {
        numResumes++;

        return ErrorEnum::eNone;
    }));
    ON_CALL(mLauncherMock, RunInstances(_, _)).WillByDefault(InvokeWithoutArgs([&]() {
        numRuns++;

        return ErrorEnum::eNone;
    }));

    // Allocations order: install items, launch request, launch statuses, wait active, finalize

    struct TestCase {
        int mFailAt;
        int mExpectedResumes;
        int mExpectedRuns;
    };

    for (const auto& testCase : std::vector<TestCase> {{0, 0, 0}, {1, 1, 0}, {2, 1, 0}, {3, 1, 1}, {4, 1, 1}}) {
        numResumes = 0;
        numRuns    = 0;

        mAllocator.FailAt(testCase.mFailAt);

        ProcessAndWait(*updateManager, *desiredStatus);

        mAllocator.Reset();

        EXPECT_EQ(numResumes, testCase.mExpectedResumes) << "failAt: " << testCase.mFailAt;
        EXPECT_EQ(numRuns, testCase.mExpectedRuns) << "failAt: " << testCase.mFailAt;
    }

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, CheckUpdateRequiredAllocationFailures)
{
    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    std::atomic_int numSetCerts {0};

    ON_CALL(mImageManagerMock, SetCerts(_, _)).WillByDefault(InvokeWithoutArgs([&]() {
        numSetCerts++;

        return ErrorEnum::eNone;
    }));

    // Allocations order: update items check, instances check. Failed update required check starts the update, no
    // update is started when both checks succeed.

    struct TestCase {
        int  mFailAt;
        bool mFailAll;
        int  mExpectedSetCerts;
    };

    for (const auto& testCase : std::vector<TestCase> {{0, true, 1}, {1, false, 1}, {2, false, 0}}) {
        numSetCerts = 0;

        if (testCase.mFailAll) {
            mAllocator.FailAll();
        } else {
            mAllocator.FailAt(testCase.mFailAt);
        }

        ProcessAndWait(*updateManager, *desiredStatus);

        mAllocator.Reset();

        EXPECT_EQ(numSetCerts, testCase.mExpectedSetCerts) << "failAt: " << testCase.mFailAt;
    }

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, IntegrityCheckErrors)
{
    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});

    auto itemsStatuses = std::make_unique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems>>();

    itemsStatuses->EmplaceBack(UpdateItemStatus {
        "item1", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eInstalled, ErrorEnum::eNone});

    auto instancesStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    instancesStatuses->EmplaceBack(CreateInstanceStatus("item1", "subject1", 0, UpdateItemTypeEnum::eService, "1.0.0",
        InstanceStateEnum::eFailed, ErrorEnum::eIntegrity));

    std::atomic_int numInstalls {0};
    std::atomic_int numRuns {0};
    std::atomic_int numSetItemFails {0};

    ON_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillByDefault(DoAll(SetArgReferee<0>(*itemsStatuses), Return(ErrorEnum::eNone)));
    ON_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).WillByDefault(InvokeWithoutArgs([&]() {
        numSetItemFails++;

        return ErrorEnum::eNone;
    }));
    ON_CALL(mLauncherMock, GetInstancesStatuses(_))
        .WillByDefault(DoAll(SetArgReferee<0>(*instancesStatuses), Return(ErrorEnum::eNone)));
    ON_CALL(mImageManagerMock, InstallItem(_)).WillByDefault(InvokeWithoutArgs([&]() {
        numInstalls++;

        return ErrorEnum::eNone;
    }));
    ON_CALL(mLauncherMock, RunInstances(_, _)).WillByDefault(InvokeWithoutArgs([&]() {
        numRuns++;

        return ErrorEnum::eNone;
    }));

    // Desired status requires no update: it becomes the current one

    ProcessAndWait(*updateManager, *desiredStatus);

    EXPECT_EQ(mStorage.GetNumRecoveries(), 0);

    auto checkRecovery = [&](const char* scenario, int expectedInstalls, int expectedRuns) {
        numInstalls = 0;
        numRuns     = 0;

        EXPECT_TRUE(RecoverAndWait(*instancesStatuses)) << scenario;
        EXPECT_EQ(numInstalls, expectedInstalls) << scenario;
        EXPECT_EQ(numRuns, expectedRuns) << scenario;
    };

    // Get items statuses fails: recovery is aborted, the items are not finalized

    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_)).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).Times(0);

    checkRecovery("get items statuses fails", 0, 0);

    Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);
    Mock::VerifyAndClearExpectations(&mLauncherMock);

    // Get instances statuses fails: recovery is aborted, the items are not finalized

    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(*itemsStatuses), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillOnce(Return(ErrorEnum::eFailed));

    checkRecovery("get instances statuses fails", 0, 0);

    Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);
    Mock::VerifyAndClearExpectations(&mLauncherMock);

    // Allocation fails: allocations order is items to recover, items statuses, instances statuses

    for (auto failAt : {1, 2}) {
        mAllocator.FailAt(failAt);

        checkRecovery(failAt == 1 ? "items statuses allocation fails" : "instances statuses allocation fails", 0, 0);

        mAllocator.Reset();
    }

    // Item is not installed anymore: it is not recovered, the instances are launched again

    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_)).WillRepeatedly(Return(ErrorEnum::eNone));

    checkRecovery("item is not installed", 0, 1);

    Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);

    // All succeeds: the item is installed again and the instances are launched

    checkRecovery("no errors", 1, 1);

    // The item is never set to failed with the integrity error

    EXPECT_EQ(numSetItemFails, 0);

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, CheckInstancesUpdateRequired)
{
    auto updateManager = CreateAndStartUpdateManager();

    auto itemsStatuses = std::make_unique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems>>();

    itemsStatuses->EmplaceBack(UpdateItemStatus {
        "item1", UpdateItemTypeEnum::eComponent, "1.0.0", false, ItemStateEnum::eInstalled, ErrorEnum::eNone});

    std::mutex mutex;
    auto       instancesStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    ON_CALL(mItemStatusProviderMock, GetItemsStatuses(_)).WillByDefault(Invoke([&](Array<UpdateItemStatus>& statuses) {
        std::lock_guard lock {mutex};

        statuses = *itemsStatuses;

        return ErrorEnum::eNone;
    }));
    ON_CALL(mLauncherMock, GetInstancesStatuses(_)).WillByDefault(Invoke([&](Array<InstanceStatus>& statuses) {
        std::lock_guard lock {mutex};

        statuses = *instancesStatuses;

        return ErrorEnum::eNone;
    }));

    std::atomic_int numSetCerts {0};

    ON_CALL(mImageManagerMock, SetCerts(_, _)).WillByDefault(InvokeWithoutArgs([&]() {
        numSetCerts++;

        return ErrorEnum::eNone;
    }));

    auto desiredStatus = std::make_unique<DesiredStatus>();

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eComponent, "1.0.0");

    struct TestCase {
        std::vector<DesiredInstanceInfo> mDesiredInstances;
        std::vector<InstanceStatus>      mInstances;
        bool                             mUpdateRequired;
    };

    std::vector<TestCase> testCases {
        {{{"item1", "subject1", 0, 0, {}}}, {}, false},
        {{{"item1", "subject1", 0, 1, {}}}, {}, true},
        {{},
            {CreateInstanceStatus(
                "item2", "subject1", 0, UpdateItemTypeEnum::eService, "1.0.0", InstanceStateEnum::eActive)},
            true},
        {{},
            {CreateInstanceStatus(
                "item2", "subject1", 0, UpdateItemTypeEnum::eComponent, "1.0.0", InstanceStateEnum::eActive)},
            false},
        {{{"item1", "subject1", 0, 1, {}}},
            {CreateInstanceStatus(
                "item1", "subject1", 0, UpdateItemTypeEnum::eComponent, "2.0.0", InstanceStateEnum::eActive)},
            true},
        {{{"item1", "subject1", 0, 1, {}}},
            {CreateInstanceStatus(
                "item1", "subject1", 0, UpdateItemTypeEnum::eComponent, "1.0.0", InstanceStateEnum::eFailed)},
            true},
        {{{"item1", "subject1", 0, 1, {}}},
            {CreateInstanceStatus(
                "item1", "subject1", 0, UpdateItemTypeEnum::eComponent, "1.0.0", InstanceStateEnum::eActive)},
            false},
    };

    for (size_t i = 0; i < testCases.size(); i++) {
        const auto& testCase = testCases[i];

        {
            std::lock_guard lock {mutex};

            instancesStatuses->Clear();

            for (const auto& instance : testCase.mInstances) {
                instancesStatuses->PushBack(instance);
            }
        }

        desiredStatus->mInstances.Clear();

        for (const auto& instance : testCase.mDesiredInstances) {
            desiredStatus->mInstances.PushBack(instance);
        }

        numSetCerts = 0;

        ProcessAndWait(*updateManager, *desiredStatus);

        EXPECT_EQ(numSetCerts, testCase.mUpdateRequired ? 1 : 0) << "test case: " << i;
    }

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, InstallUpdateItemsErrors)
{
    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    desiredStatus->mNodes.EmplaceBack(DesiredNodeStateInfo {"node1", DesiredNodeStateEnum::eProvisioned});
    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");

    EXPECT_CALL(mImageManagerMock, InstallItem(_)).Times(0);
    EXPECT_CALL(mLauncherMock, RunInstances(_, _)).Times(0);

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eFailed));

    ProcessAndWait(*updateManager, *desiredStatus);

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_)).WillOnce(Return(ErrorEnum::eFailed));

    ProcessAndWait(*updateManager, *desiredStatus);

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, LaunchInstancesErrors)
{
    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    // item1 has empty index digest, item2 is not in desired update items, subject2 is not in desired subjects

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mUpdateItems.Back().mIndexDigest.Clear();
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject2", 0, 1, {}});
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item2", "subject1", 0, 1, {}});
    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});

    auto launchedStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    launchedStatuses->EmplaceBack(CreateInstanceStatus(
        "item1", "subject2", 0, UpdateItemTypeEnum::eService, "1.0.0", InstanceStateEnum::eFailed, ErrorEnum::eFailed));

    std::unique_ptr<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>> runRequest;

    // Launched instance fails: item is set failed, set item fail error is ignored

    EXPECT_CALL(mLauncherMock, RunInstances(_, _))
        .WillOnce(Invoke([&](const Array<launcher::RunInstanceRequest>& request, Array<InstanceStatus>& statuses) {
            runRequest  = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();
            *runRequest = request;
            statuses    = *launchedStatuses;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(*launchedStatuses), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item1"), String("1.0.0"), _))
        .WillOnce(Return(ErrorEnum::eFailed));

    ProcessAndWait(*updateManager, *desiredStatus);

    ASSERT_TRUE(runRequest);
    ASSERT_EQ(runRequest->Size(), 2);
    EXPECT_EQ((*runRequest)[0].mVersion, "1.0.0");
    EXPECT_EQ((*runRequest)[0].mSubjectInfo.mSubjectID, "subject2");
    EXPECT_TRUE((*runRequest)[1].mVersion.IsEmpty());
    EXPECT_EQ((*runRequest)[1].mSubjectInfo.mSubjectID, "subject1");

    Mock::VerifyAndClearExpectations(&mLauncherMock);
    Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);

    // Run instances fails: update is finished without waiting instances

    desiredStatus->mNodes.EmplaceBack(DesiredNodeStateInfo {"node1", DesiredNodeStateEnum::eProvisioned});

    EXPECT_CALL(mLauncherMock, RunInstances(_, _)).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).Times(0);

    ProcessAndWait(*updateManager, *desiredStatus);

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, WaitInstancesActive)
{
    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    desiredStatus->mNodes.EmplaceBack(DesiredNodeStateInfo {"node1", DesiredNodeStateEnum::eProvisioned});

    auto activatingStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    activatingStatuses->EmplaceBack(CreateInstanceStatus(
        "item1", "subject1", 0, UpdateItemTypeEnum::eService, "1.0.0", InstanceStateEnum::eActivating));

    // Get instances statuses fails: finalize is not performed

    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_)).Times(1);

    ProcessAndWait(*updateManager, *desiredStatus);

    Mock::VerifyAndClearExpectations(&mLauncherMock);
    Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);

    std::atomic_int numGetInstances {0};

    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_))
        .WillRepeatedly(Invoke([&](Array<InstanceStatus>& statuses) -> Error {
            if (++numGetInstances == 1) {
                statuses = *activatingStatuses;
            } else {
                statuses.Clear();
            }

            return ErrorEnum::eNone;
        }));

    auto err = updateManager->ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    // Instances statuses change with the integrity error wakes up waiting for active instances. It is repeated until
    // the waiting is woken up, as the change made before the waiting starts doesn't wake it up.

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    statuses->EmplaceBack(CreateInstanceStatus("item1", "subject1", 0, UpdateItemTypeEnum::eService, "1.0.0",
        InstanceStateEnum::eFailed, ErrorEnum::eIntegrity));

    EXPECT_TRUE(WaitForCondition([&]() {
        if (numGetInstances >= 2) {
            return true;
        }

        DesiredStatusInstanceListener()->OnInstancesStatusesChanged(*statuses);

        return false;
    }));

    // The change made during the update starts the recovery once after the update is finished

    EXPECT_TRUE(WaitForCondition(
        [&]() { return mStorage.GetNumRecoveries() == 1 && mStorage.GetState() == UpdateStateEnum::eNone; }));

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, NewDesiredStatusDuringUpdate)
{
    constexpr auto cMaxConcurrentItems = static_cast<size_t>(cMaxNumConcurrentItems);

    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    for (size_t i = 0; i < cMaxConcurrentItems + 1; i++) {
        CreateUpdateItemInfo(
            *desiredStatus, ("item" + std::to_string(i)).c_str(), UpdateItemTypeEnum::eService, "1.0.0");
    }

    std::mutex              mutex;
    std::condition_variable cv;
    size_t                  numStarted = 0;
    bool                    released   = false;
    std::set<std::string>   installedItems;

    EXPECT_CALL(mImageManagerMock, InstallItem(_)).WillRepeatedly(Invoke([&](const UpdateItemInfo& item) -> Error {
        std::unique_lock lock {mutex};

        numStarted++;
        cv.notify_all();

        EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return released; }));

        installedItems.insert(item.mItemID.CStr());

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mImageManagerMock, Cancel()).WillOnce(Return(ErrorEnum::eFailed));

    auto err = updateManager->ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    {
        std::unique_lock lock {mutex};

        EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return numStarted == cMaxConcurrentItems; }));
    }

    EXPECT_TRUE(updateManager->ProcessDesiredStatus(*desiredStatus).IsNone());

    // Each next desired status differs by one field and cancels current update

    desiredStatus->mNodes.EmplaceBack(DesiredNodeStateInfo {"node1", DesiredNodeStateEnum::eProvisioned});
    EXPECT_TRUE(updateManager->ProcessDesiredStatus(*desiredStatus).IsNone());

    desiredStatus->mUnitConfig.EmplaceValue();
    desiredStatus->mUnitConfig->mVersion = "1.0.0";
    EXPECT_TRUE(updateManager->ProcessDesiredStatus(*desiredStatus).IsNone());

    CreateUpdateItemInfo(*desiredStatus, "itemNew", UpdateItemTypeEnum::eService, "1.0.0");
    EXPECT_TRUE(updateManager->ProcessDesiredStatus(*desiredStatus).IsNone());

    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"itemNew", "subject1", 0, 1, {}});
    EXPECT_TRUE(updateManager->ProcessDesiredStatus(*desiredStatus).IsNone());

    desiredStatus->mSubjects.EmplaceBack(SubjectInfo {"subject1", SubjectTypeEnum::eUser});
    EXPECT_TRUE(updateManager->ProcessDesiredStatus(*desiredStatus).IsNone());

    {
        std::lock_guard lock {mutex};

        released = true;
        cv.notify_all();
    }

    EXPECT_TRUE(WaitUpdateFinished());

    {
        std::lock_guard lock {mutex};

        // Queued item of the canceled update is skipped, the last desired status installs all items

        EXPECT_EQ(numStarted, cMaxConcurrentItems + desiredStatus->mUpdateItems.Size());
        EXPECT_EQ(installedItems.size(), desiredStatus->mUpdateItems.Size());
    }

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, StopFailsOnCancelError)
{
    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");

    std::mutex              mutex;
    std::condition_variable cv;
    bool                    started  = false;
    bool                    canceled = false;

    EXPECT_CALL(mImageManagerMock, InstallItem(_)).WillOnce(InvokeWithoutArgs([&]() -> Error {
        std::unique_lock lock {mutex};

        started = true;
        cv.notify_all();

        EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return canceled; }));

        return ErrorEnum::eCanceled;
    }));
    EXPECT_CALL(mImageManagerMock, Cancel()).WillOnce(InvokeWithoutArgs([&]() -> Error {
        std::lock_guard lock {mutex};

        canceled = true;
        cv.notify_all();

        return ErrorEnum::eFailed;
    }));

    auto err = updateManager->ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    {
        std::unique_lock lock {mutex};

        EXPECT_TRUE(cv.wait_for(lock, cCVTimeout, [&]() { return started; }));
    }

    err = updateManager->Stop();
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << "Unexpected error: " << tests::utils::ErrorToStr(err);
}

TEST_F(UpdateManagerErrorTest, SendFullUnitStatusErrors)
{
    enum class FailedStep {
        eUnitConfig,
        eNodeIDs,
        eNodeInfo,
        eItemsStatuses,
        eInstancesStatuses,
        eSubjects,
        eSend,
        eItemsAllocation,
        eInstancesAllocation
    };

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIDs;

    nodeIDs.EmplaceBack("node1");

    for (auto failed : {FailedStep::eUnitConfig, FailedStep::eNodeIDs, FailedStep::eNodeInfo,
             FailedStep::eItemsStatuses, FailedStep::eInstancesStatuses, FailedStep::eSubjects, FailedStep::eSend,
             FailedStep::eItemsAllocation, FailedStep::eInstancesAllocation}) {
        auto updateManager = CreateAndStartUpdateManager();

        EXPECT_CALL(mUnitConfigMock, GetUnitConfigStatus(_))
            .WillRepeatedly(Return(failed == FailedStep::eUnitConfig ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mNodeInfoProviderMock, GetAllNodeIDs(_))
            .WillRepeatedly(DoAll(SetArgReferee<0>(nodeIDs),
                Return(failed == FailedStep::eNodeIDs ? ErrorEnum::eFailed : ErrorEnum::eNone)));
        EXPECT_CALL(mNodeInfoProviderMock, GetNodeInfo(_, _))
            .WillRepeatedly(Return(failed == FailedStep::eNodeInfo ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
            .WillRepeatedly(Return(failed == FailedStep::eItemsStatuses ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_))
            .WillRepeatedly(Return(failed == FailedStep::eInstancesStatuses ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mIdentProviderMock, GetSubjects(_))
            .WillRepeatedly(Return(failed == FailedStep::eSubjects ? ErrorEnum::eFailed : ErrorEnum::eNone));
        EXPECT_CALL(mSenderMock, SendUnitStatus(_))
            .Times(failed == FailedStep::eSend ? 1 : 0)
            .WillRepeatedly(Return(ErrorEnum::eFailed));

        if (failed == FailedStep::eItemsAllocation) {
            mAllocator.FailAt(0);
        } else if (failed == FailedStep::eInstancesAllocation) {
            mAllocator.FailAt(1);
        }

        mConnectionListener->OnConnect();

        mAllocator.Reset();

        // Failed full unit status is not resent on reconnect

        mConnectionListener->OnConnect();

        EXPECT_TRUE(updateManager->Stop().IsNone());

        Mock::VerifyAndClearExpectations(&mUnitConfigMock);
        Mock::VerifyAndClearExpectations(&mNodeInfoProviderMock);
        Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);
        Mock::VerifyAndClearExpectations(&mLauncherMock);
        Mock::VerifyAndClearExpectations(&mIdentProviderMock);
        Mock::VerifyAndClearExpectations(&mSenderMock);
    }
}

TEST_F(UpdateManagerErrorTest, UnitConfigStatusAfterFailedFullStatus)
{
    auto updateManager = CreateAndStartUpdateManager();

    EXPECT_CALL(mNodeInfoProviderMock, GetAllNodeIDs(_)).WillRepeatedly(Return(ErrorEnum::eFailed));

    mConnectionListener->OnConnect();

    auto desiredStatus = std::make_unique<DesiredStatus>();

    desiredStatus->mUnitConfig.EmplaceValue();
    desiredStatus->mUnitConfig->mVersion = "2.0.0";

    EXPECT_CALL(mUnitConfigMock, UpdateUnitConfig(_)).WillOnce(Return(ErrorEnum::eNone));

    ProcessAndWait(*updateManager, *desiredStatus);

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, ListenersWhenCloudDisconnected)
{
    auto updateManager = CreateAndStartUpdateManager();

    mConnectionListener->OnConnect();
    mConnectionListener->OnDisconnect();

    // No delta unit status is sent when cloud is disconnected

    EXPECT_CALL(mSenderMock, SendUnitStatus(_)).Times(0);

    UnitNodeInfo nodeInfo;

    nodeInfo.mNodeID = "node1";

    mNodeInfoListener->OnNodeInfoChanged(nodeInfo);
    mItemStatusListener->OnItemStatusChanged(UpdateItemStatus {
        "item1", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eInstalled, ErrorEnum::eNone});
    mItemStatusListener->OnItemRemovedFromCache("item1", "1.0.0");

    StaticArray<StaticString<cIDLen>, 1> subjects;

    subjects.EmplaceBack("subject1");

    mSubjectsListener->SubjectsChanged(subjects);

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    statuses->EmplaceBack(CreateInstanceStatus(
        "item1", "subject1", 0, UpdateItemTypeEnum::eService, "1.0.0", InstanceStateEnum::eActive));

    UnitStatusInstanceListener()->OnInstancesStatusesChanged(*statuses);

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, DeltaUnitStatusWithPreinstalledInstances)
{
    auto updateManager = CreateAndStartUpdateManager();

    mConnectionListener->OnConnect();

    std::mutex                               mutex;
    std::condition_variable                  cv;
    std::vector<std::unique_ptr<UnitStatus>> sentStatuses;

    EXPECT_CALL(mSenderMock, SendUnitStatus(_)).WillRepeatedly(Invoke([&](const UnitStatus& unitStatus) {
        std::lock_guard lock {mutex};

        sentStatuses.push_back(std::make_unique<UnitStatus>(unitStatus));
        cv.notify_all();

        return ErrorEnum::eFailed;
    }));

    auto waitStatuses = [&](size_t count) {
        std::unique_lock lock {mutex};

        return cv.wait_for(lock, cCVTimeout, [&]() { return sentStatuses.size() >= count; });
    };

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    statuses->EmplaceBack(CreateInstanceStatus("item1", "subject1", 0, UpdateItemTypeEnum::eService, "1.0.0",
        InstanceStateEnum::eActive, ErrorEnum::eNone, true));

    // Delta with preinstalled instance only: preinstalled item is added

    UnitStatusInstanceListener()->OnInstancesStatusesChanged(*statuses);

    ASSERT_TRUE(waitStatuses(1));

    {
        std::lock_guard lock {mutex};

        const auto& unitStatus = *sentStatuses[0];

        EXPECT_TRUE(unitStatus.mIsDeltaInfo);
        ASSERT_TRUE(unitStatus.mUpdateItems.HasValue());
        ASSERT_EQ(unitStatus.mUpdateItems->Size(), 1);
        EXPECT_EQ(unitStatus.mUpdateItems.GetValue()[0],
            (UpdateItemStatus {
                "item1", UpdateItemTypeEnum::eService, "1.0.0", true, ItemStateEnum::eInstalled, ErrorEnum::eNone}));
    }

    // Delta with item status and preinstalled instance: item is marked as preinstalled

    mItemStatusListener->OnItemStatusChanged(UpdateItemStatus {
        "item1", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eInstalled, ErrorEnum::eNone});
    UnitStatusInstanceListener()->OnInstancesStatusesChanged(*statuses);

    ASSERT_TRUE(waitStatuses(2));

    {
        std::lock_guard lock {mutex};

        const auto& unitStatus = *sentStatuses[1];

        EXPECT_TRUE(unitStatus.mIsDeltaInfo);
        ASSERT_TRUE(unitStatus.mUpdateItems.HasValue());
        ASSERT_EQ(unitStatus.mUpdateItems->Size(), 1);
        EXPECT_TRUE(unitStatus.mUpdateItems.GetValue()[0].mPreinstalled);
    }

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, StartTwiceFails)
{
    auto updateManager = CreateAndStartUpdateManager();

    auto err = updateManager->Start();
    EXPECT_TRUE(err.Is(ErrorEnum::eWrongState)) << "Unexpected error: " << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, CheckUpdateRequiredGetStatusesErrors)
{
    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    std::atomic_int numSetCerts {0};

    ON_CALL(mImageManagerMock, SetCerts(_, _)).WillByDefault(InvokeWithoutArgs([&]() {
        numSetCerts++;

        return ErrorEnum::eNone;
    }));

    // Get items statuses fails: update is required

    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillOnce(Return(ErrorEnum::eFailed))
        .WillRepeatedly(Return(ErrorEnum::eNone));

    ProcessAndWait(*updateManager, *desiredStatus);

    EXPECT_EQ(numSetCerts, 1);

    Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);

    // Get instances statuses fails: update is required

    numSetCerts = 0;

    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_))
        .WillOnce(Return(ErrorEnum::eFailed))
        .WillRepeatedly(Return(ErrorEnum::eNone));

    ProcessAndWait(*updateManager, *desiredStatus);

    EXPECT_EQ(numSetCerts, 1);

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, RecoverItemsErrors)
{
    auto updateManager = CreateAndStartUpdateManager();
    auto desiredStatus = std::make_unique<DesiredStatus>();

    CreateUpdateItemInfo(*desiredStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0");
    desiredStatus->mInstances.EmplaceBack(DesiredInstanceInfo {"item1", "subject1", 0, 1, {}});

    auto itemsStatuses = std::make_unique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems>>();

    itemsStatuses->EmplaceBack(UpdateItemStatus {
        "item1", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eInstalled, ErrorEnum::eNone});

    auto instancesStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    instancesStatuses->EmplaceBack(CreateInstanceStatus("item1", "subject1", 0, UpdateItemTypeEnum::eService, "1.0.0",
        InstanceStateEnum::eFailed, ErrorEnum::eIntegrity));

    std::atomic_int numRuns {0};

    ON_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillByDefault(DoAll(SetArgReferee<0>(*itemsStatuses), Return(ErrorEnum::eNone)));
    ON_CALL(mLauncherMock, GetInstancesStatuses(_))
        .WillByDefault(DoAll(SetArgReferee<0>(*instancesStatuses), Return(ErrorEnum::eNone)));
    ON_CALL(mLauncherMock, RunInstances(_, _)).WillByDefault(InvokeWithoutArgs([&]() {
        numRuns++;

        return ErrorEnum::eNone;
    }));

    // Desired status requires no update: it becomes the current one

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(0);

    ProcessAndWait(*updateManager, *desiredStatus);

    Mock::VerifyAndClearExpectations(&mImageManagerMock);

    // Set certs fails: recovery is aborted before looking for the items to recover

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_)).Times(0);
    EXPECT_CALL(mImageManagerMock, InstallItem(_)).Times(0);

    EXPECT_TRUE(RecoverAndWait(*instancesStatuses));
    EXPECT_EQ(numRuns, 0);

    Mock::VerifyAndClearExpectations(&mImageManagerMock);
    Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);

    // Items to recover allocation fails: recovery is aborted before looking for the items to recover

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_)).Times(0);
    EXPECT_CALL(mImageManagerMock, InstallItem(_)).Times(0);

    mAllocator.FailAt(0);

    EXPECT_TRUE(RecoverAndWait(*instancesStatuses));
    EXPECT_EQ(numRuns, 0);

    mAllocator.Reset();

    Mock::VerifyAndClearExpectations(&mImageManagerMock);
    Mock::VerifyAndClearExpectations(&mItemStatusProviderMock);

    // Install item fails: error is ignored, the instances are launched and the update is finalized

    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0])).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgReferee<0>(*itemsStatuses), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).Times(0);

    EXPECT_TRUE(RecoverAndWait(*instancesStatuses));
    EXPECT_EQ(numRuns, 1);

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

TEST_F(UpdateManagerErrorTest, FullUnitStatusSkipsRemovedItems)
{
    auto updateManager = CreateAndStartUpdateManager();
    auto itemsStatuses = std::make_unique<StaticArray<UpdateItemStatus, cMaxNumUpdateItems>>();

    itemsStatuses->EmplaceBack(UpdateItemStatus {
        "item1", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eRemoved, ErrorEnum::eNone});
    itemsStatuses->EmplaceBack(UpdateItemStatus {
        "item2", UpdateItemTypeEnum::eService, "1.0.0", false, ItemStateEnum::eInstalled, ErrorEnum::eNone});

    std::unique_ptr<UnitStatus> sentStatus;

    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillOnce(DoAll(SetArgReferee<0>(*itemsStatuses), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mSenderMock, SendUnitStatus(_)).WillOnce(Invoke([&](const UnitStatus& unitStatus) {
        sentStatus = std::make_unique<UnitStatus>(unitStatus);

        return ErrorEnum::eNone;
    }));

    mConnectionListener->OnConnect();

    ASSERT_TRUE(sentStatus);
    ASSERT_TRUE(sentStatus->mUpdateItems.HasValue());
    ASSERT_EQ(sentStatus->mUpdateItems->Size(), 1);
    EXPECT_EQ(sentStatus->mUpdateItems.GetValue()[0].mItemID, "item2");

    EXPECT_TRUE(updateManager->Stop().IsNone());
}

} // namespace aos::cm::updatemanager
