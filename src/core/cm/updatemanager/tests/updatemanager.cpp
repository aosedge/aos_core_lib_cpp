/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
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
const auto                cPollPeriod            = std::chrono::milliseconds(10);

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

    // Expect calls: item1 failed with the integrity error is installed again between the launches, item2 failed with
    // another error is not

    std::atomic_int  numRuns {0};
    std::vector<int> item1InstallRuns;

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0]))
        .Times(2)
        .WillRepeatedly(InvokeWithoutArgs([&]() {
            item1InstallRuns.push_back(numRuns);

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[1])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item1"), String("1.0.0"), _))
        .WillOnce(Invoke([](const String&, const String&, const Error& error) {
            EXPECT_TRUE(error.Is(ErrorEnum::eIntegrity));

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item2"), String("1.0.0"), _))
        .WillOnce(Invoke([](const String&, const String&, const Error& error) {
            EXPECT_TRUE(error.Is(ErrorEnum::eFailed));

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(2).WillRepeatedly(InvokeWithoutArgs([&]() {
        numRuns++;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (numRuns > 0) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            if (numRuns == 1) {
                statuses[0].mState = ItemStateEnum::eFailed;
                statuses[0].mError = ErrorEnum::eIntegrity;
                statuses[1].mState = ItemStateEnum::eFailed;
                statuses[1].mError = ErrorEnum::eFailed;
            }

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, GetInstancesStatuses(_)).WillRepeatedly(Invoke([&](Array<InstanceStatus>& instances) {
        ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);

        if (numRuns == 1) {
            // The first item1 instance error is overridden by the integrity error of the second one

            SetInstancesFailed(instances, "item1", ErrorEnum::eIntegrity);
            instances[0].mError = ErrorEnum::eFailed;

            SetInstancesFailed(instances, "item2", ErrorEnum::eFailed);
        }

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
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

    // Expect calls: item1 has one instance failed with a non integrity error, so it is not set to failed. item2 has one
    // instance failed with the integrity error, so it is set to failed and installed again.

    std::atomic_int numRuns {0};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0])).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[1]))
        .Times(2)
        .WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(_, _, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item2"), String("1.0.0"), _))
        .WillOnce(Invoke([](const String&, const String&, const Error& error) {
            EXPECT_TRUE(error.Is(ErrorEnum::eIntegrity));

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(2).WillRepeatedly(InvokeWithoutArgs([&]() {
        numRuns++;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            if (numRuns > 0) {
                statuses = expectedUnitStatus->mUpdateItems.GetValue();
            }

            if (numRuns == 1) {
                statuses[1].mState = ItemStateEnum::eFailed;
                statuses[1].mError = ErrorEnum::eIntegrity;
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

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
}

TEST_F(UpdateManagerTest, RecoverItemsFailsAgainFinishesUpdate)
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

    // Set expected statuses: the item and the instance failed states and errors are reported

    CreateUpdateItemStatus(*expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eFailed);

    expectedUnitStatus->mUpdateItems->Back().mError = ErrorEnum::eIntegrity;

    CreateInstancesStatuses(
        *expectedUnitStatus, "item1", UpdateItemTypeEnum::eService, "subject1", "1.0.0", 1, InstanceStateEnum::eFailed);

    sInstanceStatusStorage.Back().mError = ErrorEnum::eIntegrity;

    auto runRequest = std::make_unique<StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>>();

    CreateRunRequest(*desiredStatus, *runRequest);

    // Expect calls: the item is installed again and the instances are launched again only once

    std::atomic_bool instancesLaunched {false};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0]))
        .Times(2)
        .WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item1"), String("1.0.0"), _))
        .Times(2)
        .WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(2).WillRepeatedly(InvokeWithoutArgs([&]() {
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

    // Set expected statuses

    CreateUpdateItemStatus(*failedUnitStatus, "item1", UpdateItemTypeEnum::eService, "1.0.0", ItemStateEnum::eFailed);

    failedUnitStatus->mUpdateItems->Back().mError = ErrorEnum::eIntegrity;

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

TEST_F(UpdateManagerTest, RecoverItemsOnDesiredStatusWithIntegrityError)
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
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item1"), String("1.0.0"), _))
        .WillOnce(Invoke([&](const String&, const String&, const Error& error) {
            EXPECT_TRUE(error.Is(ErrorEnum::eIntegrity));

            expectedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eFailed;
            expectedUnitStatus->mUpdateItems->Back().mError = error;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(2).WillRepeatedly(InvokeWithoutArgs([&]() {
        sInstanceStatusStorage.Back().mState = InstanceStateEnum::eActive;
        sInstanceStatusStorage.Back().mError = ErrorEnum::eNone;

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
        if (numRuns > 0) {
            ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);
        }

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);

    // The instance fails with the integrity error while no update is running

    sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
    sInstanceStatusStorage.Back().mError = Error(ErrorEnum::eIntegrity, EIO);

    // The same desired status recovers the item

    err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(numRuns, 2);
    EXPECT_EQ(installStates, std::vector<UpdateState>({UpdateStateEnum::eDownloading, UpdateStateEnum::eRecovering}));
}

TEST_F(UpdateManagerTest, RecoverItemsOnDesiredStatusFailsAgain)
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

    // Expect calls: the instances launched by the recovery fail with the integrity error again

    const auto integrityErr = Error(ErrorEnum::eIntegrity, EIO);

    std::atomic_int numRuns {0};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0]))
        .Times(2)
        .WillRepeatedly(InvokeWithoutArgs([&]() {
            expectedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eInstalled;
            expectedUnitStatus->mUpdateItems->Back().mError = ErrorEnum::eNone;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item1"), String("1.0.0"), _))
        .Times(2)
        .WillRepeatedly(Invoke([&](const String&, const String&, const Error& error) {
            EXPECT_TRUE(error.Is(ErrorEnum::eIntegrity));

            expectedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eFailed;
            expectedUnitStatus->mUpdateItems->Back().mError = error;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(2).WillRepeatedly(InvokeWithoutArgs([&]() {
        if (++numRuns == 1) {
            sInstanceStatusStorage.Back().mState = InstanceStateEnum::eActive;
            sInstanceStatusStorage.Back().mError = ErrorEnum::eNone;
        } else {
            sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
            sInstanceStatusStorage.Back().mError = integrityErr;
        }

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

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // The instance fails with the integrity error while no update is running

    sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
    sInstanceStatusStorage.Back().mError = integrityErr;

    // The same desired status recovers the item, the recovery fails and the item failed state is reported

    auto failedUnitStatus = std::make_unique<UnitStatus>(*expectedUnitStatus);

    failedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eFailed;
    failedUnitStatus->mUpdateItems->Back().mError = integrityErr;

    err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *failedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(expectedUnitStatus->mUpdateItems->Back().mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(expectedUnitStatus->mUpdateItems->Back().mError.Is(ErrorEnum::eIntegrity));

    // The same desired status doesn't recover the failed item again

    err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(numRuns, 2);
}

TEST_F(UpdateManagerTest, OtherErrorInstanceDoesNotRecoverItemsOnDesiredStatus)
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

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);

    // The instance fails with a non integrity error while no update is running

    sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
    sInstanceStatusStorage.Back().mError = Error(ErrorEnum::eFailed, EIO);

    // The same desired status doesn't recover the item

    err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

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
    std::atomic_int          numGetInstances {0};
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
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item1"), String("1.0.0"), _))
        .WillOnce(Invoke([&](const String&, const String&, const Error& error) {
            EXPECT_TRUE(error.Is(ErrorEnum::eIntegrity));

            expectedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eFailed;
            expectedUnitStatus->mUpdateItems->Back().mError = error;

            return ErrorEnum::eNone;
        }));
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
        numGetInstances++;

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
    std::atomic_int          numGetInstances {0};
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
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item1"), String("1.0.0"), _))
        .WillOnce(Invoke([&](const String&, const String&, const Error& error) {
            EXPECT_TRUE(error.Is(ErrorEnum::eIntegrity));

            expectedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eFailed;
            expectedUnitStatus->mUpdateItems->Back().mError = error;

            return ErrorEnum::eNone;
        }));
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
        numGetInstances++;

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

    // Expect calls: the instances launched by the recovery fail with the integrity error again

    const auto integrityErr = Error(ErrorEnum::eIntegrity, EIO);

    std::atomic_int numRuns {0};
    std::atomic_int numGetInstances {0};

    EXPECT_CALL(mImageManagerMock, SetCerts(_, _)).Times(2).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManagerMock, InstallItem(desiredStatus->mUpdateItems[0]))
        .Times(2)
        .WillRepeatedly(InvokeWithoutArgs([&]() {
            expectedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eInstalled;
            expectedUnitStatus->mUpdateItems->Back().mError = ErrorEnum::eNone;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mImageManagerMock, RemoveItem(_, _)).Times(0);
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item1"), String("1.0.0"), _))
        .Times(2)
        .WillRepeatedly(Invoke([&](const String&, const String&, const Error& error) {
            EXPECT_TRUE(error.Is(ErrorEnum::eIntegrity));

            expectedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eFailed;
            expectedUnitStatus->mUpdateItems->Back().mError = error;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mLauncherMock, RunInstances(*runRequest, _)).Times(2).WillRepeatedly(InvokeWithoutArgs([&]() {
        if (++numRuns == 1) {
            sInstanceStatusStorage.Back().mState = InstanceStateEnum::eActive;
            sInstanceStatusStorage.Back().mError = ErrorEnum::eNone;
        } else {
            sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
            sInstanceStatusStorage.Back().mError = integrityErr;
        }

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
        numGetInstances++;

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

    // The recovery fails and the item failed state is reported

    auto failedUnitStatus = std::make_unique<UnitStatus>(*expectedUnitStatus);

    failedUnitStatus->mUpdateItems->Back().mState = ItemStateEnum::eFailed;
    failedUnitStatus->mUpdateItems->Back().mError = integrityErr;

    EXPECT_EQ(*WaitSendFullUnitStatus(), *failedUnitStatus);
    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(expectedUnitStatus->mUpdateItems->Back().mState, ItemStateEnum::eFailed);
    EXPECT_TRUE(expectedUnitStatus->mUpdateItems->Back().mError.Is(ErrorEnum::eIntegrity));

    // The same instances statuses and the same desired status don't recover the failed item again

    auto prevNumGetInstances = numGetInstances.load();

    NotifyInstancesStatusesChanged(*expectedUnitStatus);

    EXPECT_TRUE(WaitForCondition([&]() { return numGetInstances > prevNumGetInstances; }));

    err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mStorageStub.GetUpdateState().mValue, UpdateStateEnum::eNone);
    EXPECT_EQ(numRuns, 2);

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
    std::atomic_int  numGetInstances {0};

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
        numGetInstances++;

        if (instancesLaunched) {
            ConvertInstancesStatuses(expectedUnitStatus->mInstances.GetValue(), instances);
        }

        return ErrorEnum::eNone;
    }));

    auto err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(*WaitSendFullUnitStatus(), *expectedUnitStatus);

    // The instance fails with a non integrity error while no update is running and the launcher reports it

    sInstanceStatusStorage.Back().mState = InstanceStateEnum::eFailed;
    sInstanceStatusStorage.Back().mError = Error(ErrorEnum::eFailed, EIO);

    auto prevNumGetInstances = numGetInstances.load();

    NotifyInstancesStatusesChanged(*expectedUnitStatus);

    EXPECT_TRUE(WaitForCondition([&]() { return numGetInstances > prevNumGetInstances; }));

    err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

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
    EXPECT_CALL(mItemStatusProviderMock, SetItemFail(String("item1"), String("1.0.0"), _))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mLauncherMock, RunInstances(_, _)).Times(2).WillRepeatedly(InvokeWithoutArgs([&]() {
        numRuns++;

        return ErrorEnum::eNone;
    }));
    EXPECT_CALL(mItemStatusProviderMock, GetItemsStatuses(_))
        .WillRepeatedly(Invoke([&](Array<UpdateItemStatus>& statuses) {
            std::unique_lock lock(mutex);

            if (numInstalls == 0) {
                return ErrorEnum::eNone;
            }

            statuses = expectedUnitStatus->mUpdateItems.GetValue();

            if (numRuns == 1 && !canceled) {
                statuses[0].mState = ItemStateEnum::eFailed;
                statuses[0].mError = ErrorEnum::eIntegrity;
            }

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

    // Send new desired status to cancel recovering

    desiredStatus->mInstances[0].mNumInstances = 2;

    err = mUpdateManager.ProcessDesiredStatus(*desiredStatus);
    EXPECT_TRUE(err.IsNone()) << "Failed to process desired status: " << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mSenderStub.WaitSendUnitStatus(), *expectedUnitStatus);
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

} // namespace aos::cm::updatemanager
