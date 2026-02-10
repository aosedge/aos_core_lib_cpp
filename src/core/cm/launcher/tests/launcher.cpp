/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <string>
#include <vector>

#include <core/cm/launcher/launcher.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

#include "stubs/alertsproviderstub.hpp"
#include "stubs/identproviderstub.hpp"
#include "stubs/imagestorestub.hpp"
#include "stubs/instancerunnerstub.hpp"
#include "stubs/instancestatusproviderstub.hpp"
#include "stubs/monitoringproviderstub.hpp"
#include "stubs/networkmanagerstub.hpp"
#include "stubs/nodeinfoproviderstub.hpp"
#include "stubs/resourcemanagerstub.hpp"
#include "stubs/storagestatestub.hpp"
#include "stubs/storagestub.hpp"

using namespace testing;

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/

constexpr auto cMagicSum          = storagestate::StorageStateStub::cMagicSum;
constexpr auto cNodeRunners       = "NodeRunners";
constexpr auto cRunnerRunc        = "runc";
constexpr auto cRunnerRunx        = "runx";
constexpr auto cRunnerRootfs      = "rootfs";
constexpr auto cStoragePartition  = "storages";
constexpr auto cStatePartition    = "states";
constexpr auto cNodeIDLocalSM     = "localSM";
constexpr auto cNodeIDRemoteSM1   = "remoteSM1";
constexpr auto cNodeIDRemoteSM2   = "remoteSM2";
constexpr auto cNodeIDRunxSM      = "runxSM";
constexpr auto cNodeTypeVM        = "vm";
constexpr auto cSubject1          = "subject1";
constexpr auto cService1          = "service1";
constexpr auto cService1LocalURL  = "service1LocalURL";
constexpr auto cService1RemoteURL = "service1RemoteURL";
constexpr auto cService2          = "service2";
constexpr auto cService2LocalURL  = "service2LocalURL";
constexpr auto cService2RemoteURL = "service2RemoteURL";
constexpr auto cService3          = "service3";
constexpr auto cComponent1        = "component1";
constexpr auto cService3LocalURL  = "service3LocalURL";
constexpr auto cService3RemoteURL = "service3RemoteURL";
constexpr auto cLayer1            = "layer1";
constexpr auto cLayer1LocalURL    = "layer1LocalURL";
constexpr auto cLayer1RemoteURL   = "layer1RemoteURL";
constexpr auto cLayer2            = "layer2";
constexpr auto cLayer2LocalURL    = "layer2LocalURL";
constexpr auto cLayer2RemoteURL   = "layer2RemoteURL";
constexpr auto cImageID1          = "image1";
constexpr auto cRootfsImageID     = "rootfs";

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class InstanceStatusListenerStub : public instancestatusprovider::ListenerItf {
public:
    void OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses) override
    {
        std::lock_guard lock {mMutex};

        *mLastStatuses = statuses;
        ++mNotifyCount;

        LOG_DBG() << "Instance statuses received" << Log::Field("count", mNotifyCount);

        mNotifyCondVar.notify_all();
    }

    const Array<InstanceStatus>& GetLastStatuses() const
    {
        std::lock_guard lock {mMutex};

        return *mLastStatuses;
    }

    size_t GetNotifyCount() const
    {
        std::lock_guard lock {mMutex};

        return mNotifyCount;
    }

    bool WaitForNotifyCount(size_t expectedCount, std::chrono::milliseconds timeout) const
    {
        std::unique_lock<std::mutex> lock {mMutex};

        return mNotifyCondVar.wait_for(lock, timeout, [&]() { return mNotifyCount >= expectedCount; });
    }

private:
    std::unique_ptr<StaticArray<InstanceStatus, cMaxNumInstances>> mLastStatuses
        = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    mutable std::mutex              mMutex;
    mutable std::condition_variable mNotifyCondVar;
    size_t                          mNotifyCount {};
};

class CMLauncherTest : public testing::Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        LOG_INF() << "Launcher size: size=" << sizeof(Launcher);

        ASSERT_TRUE(mIdentProvider.SetSubjects({cSubject1}).IsNone());
    }

    void TearDown() override { }

protected:
    void AddItem(const std::string& id, const std::string& imageID, const oci::ItemConfig& itemConfig,
        const oci::ImageConfig& imageConfig, const std::string& version = "")
    {
        mImageStore.AddItem(id.c_str(), imageID.c_str(), itemConfig, imageConfig, version.c_str());
    }

    StaticString<oci::cDigestLen> GetManifestDigest(const std::string& id, const std::string& imageID = "") const
    {
        return mImageStore.GetManifestDigest(id.c_str(), imageID.c_str());
    }

    // Stub objects
    alerts::AlertsProviderStub             mAlertsProvider;
    imagemanager::ImageStoreStub           mImageStore;
    iamclient::IdentProviderStub           mIdentProvider;
    networkmanager::NetworkManagerStub     mNetworkManager;
    nodeinfoprovider::NodeInfoProviderStub mNodeInfoProvider;
    NiceMock<launcher::InstanceRunnerStub> mInstanceRunner;
    launcher::InstanceStatusProviderStub   mInstanceStatusProvider;
    launcher::MonitoringProviderStub       mMonitoringProvider;
    resourcemanager::ResourceManagerStub   mResourceManager;
    StorageStub                            mStorage;
    storagestate::StorageStateStub         mStorageState;

    Launcher mLauncher;
};

bool ValidateGID(size_t gid)
{
    (void)gid;

    return true;
}

bool ValidateUID(size_t uid)
{
    (void)uid;

    return true;
}

uint32_t GenerateUID()
{
    static uint32_t curUID = 5000;

    return curUID++;
}

InstanceInfo CreateInstanceInfo(const InstanceIdent& instance, StaticString<oci::cDigestLen> manifestDigest = {},
    const String& runtimeID = "runc", const String& nodeID = "",
    InstanceState instanceState = InstanceStateEnum::eActive, uint32_t uid = 0, gid_t gid = 0, Time timestamp = {},
    const String& version = "", bool isUnitSubject = false, const String& ownerID = "",
    SubjectTypeEnum subjectType = SubjectTypeEnum::eGroup, size_t priority = 0)
{
    InstanceInfo result;

    result.mInstanceIdent  = instance;
    result.mManifestDigest = manifestDigest;
    result.mRuntimeID      = runtimeID;
    result.mNodeID         = nodeID;
    result.mPrevNodeID     = "";
    result.mUID            = uid != 0 ? uid : GenerateUID();
    result.mGID            = gid;
    result.mTimestamp      = timestamp;
    result.mState          = instanceState;
    result.mVersion        = version;
    result.mIsUnitSubject  = isUnitSubject;
    result.mOwnerID        = ownerID;
    result.mSubjectType    = subjectType;
    result.mPriority       = priority;

    return result;
}

InstanceStatus CreateInstanceStatus(const InstanceIdent& instance, const std::string& nodeID,
    const std::string& runtimeID, aos::InstanceState state = aos::InstanceStateEnum::eFailed,
    const Error& error = ErrorEnum::eTimeout, const std::string& version = "", bool preinstalled = false,
    const std::string& manifestDigest = "")
{
    InstanceStatus result;

    static_cast<InstanceIdent&>(result) = instance;
    result.mNodeID                      = nodeID.c_str();
    result.mRuntimeID                   = runtimeID.c_str();
    result.mState                       = state;
    result.mError                       = error;
    result.mVersion                     = version.c_str();
    result.mPreinstalled                = preinstalled;
    result.mManifestDigest              = manifestDigest.c_str();

    if (!error.IsNone()) {
        result.mState = aos::InstanceStateEnum::eFailed;
    }

    return result;
}

InstanceIdent CreateInstanceIdent(const std::string& itemID, const std::string& subjectID = "", uint64_t instance = 0,
    UpdateItemType updateItemType = UpdateItemTypeEnum::eService)
{
    return InstanceIdent {itemID.c_str(), subjectID.c_str(), instance, updateItemType};
}

RunInstanceRequest CreateRunRequest(const std::string& itemID, const std::string& subjectID = "", uint64_t priority = 0,
    size_t numInstances = 1, const std::string& ownerID = "", const std::vector<std::string>& labels = {},
    UpdateItemType updateItemType = UpdateItemTypeEnum::eService, const std::string& version = "",
    SubjectTypeEnum subjectType = SubjectTypeEnum::eGroup)
{
    RunInstanceRequest request;

    request.mItemID                     = itemID.c_str();
    request.mUpdateItemType             = updateItemType;
    request.mVersion                    = version.c_str();
    request.mOwnerID                    = ownerID.c_str();
    request.mSubjectInfo.mSubjectID     = subjectID.c_str();
    request.mSubjectInfo.mSubjectType   = subjectType;
    request.mSubjectInfo.mIsUnitSubject = true;
    request.mPriority                   = priority;
    request.mNumInstances               = numInstances;

    for (const auto& label : labels) {
        request.mLabels.PushBack(label.c_str());
    }

    return request;
}

StaticString<oci::cDigestLen> BuildManifestDigest(const std::string& itemID, const std::string& imageID = cImageID1)
{
    return imagemanager::ImageStoreStub::BuildManifestDigest(itemID.c_str(), imageID.c_str());
}

aos::InstanceInfo CreateServiceRunInfo(const InstanceIdent& id, const std::string& imageID,
    const std::string& runtimeID, uint32_t uid, gid_t gid, const String& ip, uint64_t priority,
    const std::string& version = "", const Optional<AlertRules>& alertRules = {},
    SubjectTypeEnum subjectType = SubjectTypeEnum::eGroup, const std::string& ownerID = "")
{
    aos::InstanceInfo result;

    static_cast<InstanceIdent&>(result) = id;

    result.mVersion        = version.c_str();
    result.mManifestDigest = BuildManifestDigest(id.mItemID.CStr(), imageID);
    result.mRuntimeID      = runtimeID.c_str();
    result.mOwnerID        = ownerID.c_str();
    result.mUID            = uid;
    result.mGID            = gid;
    result.mPriority       = priority;
    result.mSubjectType    = subjectType;
    result.mStoragePath    = "storage_path";
    result.mStatePath      = "state_path";

    if (!ip.IsEmpty()) {
        result.mNetworkParameters.EmplaceValue();
        result.mNetworkParameters->mIP     = (std::string("172.17.0.") + ip.CStr()).c_str();
        result.mNetworkParameters->mSubnet = "172.17.0.0/16";
    }

    result.mMonitoringParams.EmplaceValue();
    if (alertRules.HasValue()) {
        result.mMonitoringParams.GetValue().mAlertRules = alertRules;
    }

    return result;
}

aos::InstanceInfo CreateComponentRunInfo(const InstanceIdent& id, const std::string& imageID,
    const std::string& runtimeID, uint64_t priority, const std::string& version = "",
    SubjectTypeEnum subjectType = SubjectTypeEnum::eGroup)
{
    aos::InstanceInfo result;

    static_cast<InstanceIdent&>(result) = id;

    result.mVersion        = version.c_str();
    result.mManifestDigest = BuildManifestDigest(id.mItemID.CStr(), imageID);
    result.mRuntimeID      = runtimeID.c_str();
    result.mPriority       = priority;
    result.mSubjectType    = subjectType;

    return result;
}

aos::InstanceInfo CreateAosStopInstanceInfo(const InstanceIdent& id, const std::string& runtimeID)
{
    aos::InstanceInfo result;

    static_cast<InstanceIdent&>(result) = id;
    result.mRuntimeID                   = runtimeID.c_str();

    return result;
}

monitoring::InstanceMonitoringData CreateInstanceMonitoring(const InstanceIdent& instance, double cpuUsage)
{
    monitoring::InstanceMonitoringData monitoring(instance);

    monitoring.mMonitoringData.mCPU = cpuUsage;

    return monitoring;
}

void CreateNodeMonitoring(monitoring::NodeMonitoringData& nodeMonitoring, const std::string& nodeID,
    double totalCPUUsage, const std::vector<monitoring::InstanceMonitoringData>& instanceMonitoring = {})
{
    nodeMonitoring.mNodeID              = nodeID.c_str();
    nodeMonitoring.mMonitoringData.mCPU = totalCPUUsage;

    for (const auto& inst : instanceMonitoring) {
        nodeMonitoring.mInstances.PushBack(inst);
    }
}

oci::ServiceQuotas CreateServiceQuotas(uint64_t storage, uint64_t state, uint64_t cpu, uint64_t ram)
{
    oci::ServiceQuotas quotas;

    quotas.mStorageLimit  = storage;
    quotas.mStateLimit    = state;
    quotas.mCPUDMIPSLimit = cpu;
    quotas.mRAMLimit      = ram;

    return quotas;
}

oci::RequestedResources CreateRequestedResources(uint64_t storage, uint64_t state, uint64_t cpu, uint64_t ram)
{
    oci::RequestedResources resources;

    resources.mStorage = storage;
    resources.mState   = state;
    resources.mCPU     = cpu;
    resources.mRAM     = ram;

    return resources;
}

AlertRules CreateAlertRules(double cpuRule, double ramRule)
{
    AlertRules rules;

    if (cpuRule != 0.0) {
        rules.mCPU = AlertRulePercents {Time::cMilliseconds, cpuRule, cpuRule};
    }

    if (ramRule != 0.0) {
        rules.mRAM = AlertRulePercents {Time::cMilliseconds, ramRule, ramRule};
    }

    return rules;
}

void CreateItemConfig(oci::ItemConfig& config, const std::vector<std::string>& runtimes = {"linux"},
    const oci::BalancingPolicy& balancingPolicy = oci::BalancingPolicyEnum::eNone,
    const oci::ServiceQuotas& quotas = {}, const oci::RequestedResources& requestedResources = {},
    const Optional<AlertRules>& alertRules = {}, const std::vector<std::string>& allowedConnections = {},
    const std::vector<std::string>& resources = {})
{
    for (const auto& runtime : runtimes) {
        config.mRuntimes.PushBack(runtime.c_str());
    }

    config.mBalancingPolicy    = balancingPolicy;
    config.mQuotas             = quotas;
    config.mRequestedResources = requestedResources;
    config.mAlertRules         = alertRules;

    for (const auto& connection : allowedConnections) {
        config.mAllowedConnections.PushBack(connection.c_str());
    }

    for (const auto& resource : resources) {
        config.mResources.PushBack(resource.c_str());
    }
}

oci::ImageConfig CreateImageConfig(std::string architecture = "x86_64", std::string variant = "generic",
    std::string os = "linux", std::string osVersion = "5.4.0", std::string osFeature = "feature1")
{
    oci::ImageConfig config;

    config.mArchitecture = architecture.c_str();
    config.mVariant      = variant.c_str();
    config.mOS           = os.c_str();
    config.mOSVersion    = osVersion.c_str();
    config.mOSFeatures.PushBack(osFeature.c_str());

    return config;
}

void CreateNodeConfig(NodeConfig& config, const std::string& nodeID = "", uint64_t priority = 0,
    const std::vector<std::string>& labels = {}, const ResourceRatios& resourceRatios = {},
    const AlertRules& alertRules = {})
{
    config.mNodeID   = nodeID.c_str();
    config.mPriority = priority;

    for (const auto& label : labels) {
        config.mLabels.PushBack(label.c_str());
    }

    if (resourceRatios.mCPU.HasValue() || resourceRatios.mRAM.HasValue() || resourceRatios.mStorage.HasValue()) {
        config.mResourceRatios.SetValue(resourceRatios);
    }

    if (alertRules.mCPU.HasValue() || alertRules.mRAM.HasValue()) {
        config.mAlertRules.SetValue(alertRules);
    }
}

NodeAttribute CreateNodeAttribute(const std::string& name, const std::string& value)
{
    NodeAttribute attribute;

    attribute.mName  = name.c_str();
    attribute.mValue = value.c_str();

    return attribute;
}

ResourceInfo CreateResource(const std::string& name, size_t sharedCount = 1)
{
    ResourceInfo resourceInfo;

    resourceInfo.mName        = name.c_str();
    resourceInfo.mSharedCount = sharedCount;

    return resourceInfo;
}

PartitionInfo CreatePartitionInfo(
    const std::string& name, const std::string& path, size_t totalSize, size_t usedSize = 0)
{
    (void)usedSize;

    PartitionInfo partitionInfo;

    partitionInfo.mName      = name.c_str();
    partitionInfo.mPath      = path.c_str();
    partitionInfo.mTotalSize = totalSize;

    return partitionInfo;
}

PlatformInfo CreatePlatform(const std::string& architecture = "x86_64", const std::string& variant = "generic",
    const std::string& os = "linux", const std::string& osVersion = "5.4.0", const std::string& osFeature = "feature1")
{
    PlatformInfo platformInfo;

    platformInfo.mArchInfo.mArchitecture = architecture.c_str();
    platformInfo.mArchInfo.mVariant.SetValue(variant.c_str());
    platformInfo.mOSInfo.mOS = os.c_str();
    platformInfo.mOSInfo.mVersion.SetValue(osVersion.c_str());
    platformInfo.mOSInfo.mFeatures.PushBack(osFeature.c_str());

    return platformInfo;
}

RuntimeInfo CreateRuntime(
    const std::string& runtimeID, size_t maxInstances = 0, const PlatformInfo& platform = CreatePlatform())
{
    RuntimeInfo runtimeInfo;

    runtimeInfo.mRuntimeID    = runtimeID.c_str();
    runtimeInfo.mRuntimeType  = runtimeID.c_str();
    runtimeInfo.mMaxInstances = maxInstances;

    static_cast<PlatformInfo&>(runtimeInfo) = platform;

    return runtimeInfo;
}

UnitNodeInfo CreateNodeInfo(const std::string& nodeID, size_t maxDMIPS, size_t totalRAM,
    const std::vector<RuntimeInfo>& runtimes = {}, const std::vector<ResourceInfo>& resources = {},
    NodeState state = NodeStateEnum::eProvisioned, bool isConnected = true, Error error = ErrorEnum::eNone)
{
    UnitNodeInfo nodeInfo;

    nodeInfo.mNodeID     = nodeID.c_str();
    nodeInfo.mNodeType   = cNodeTypeVM;
    nodeInfo.mMaxDMIPS   = maxDMIPS;
    nodeInfo.mTotalRAM   = totalRAM;
    nodeInfo.mOSInfo.mOS = "linux";
    nodeInfo.mOSInfo.mVersion.SetValue("5.4.0");
    nodeInfo.mCPUs.Clear();
    nodeInfo.mPartitions.Clear();

    for (const auto& runtime : runtimes) {
        nodeInfo.mRuntimes.PushBack(runtime);
    }

    for (const auto& resource : resources) {
        nodeInfo.mResources.PushBack(resource);
    }

    nodeInfo.mState       = state;
    nodeInfo.mIsConnected = isConnected;
    nodeInfo.mError       = error;

    return nodeInfo;
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CMLauncherTest, InstancesWithInvalidImageAreRemovedOnStart)
{
    Config cfg;

    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;
    cfg.mInstanceTTL            = 1 * Time::cSeconds;

    // Store instance with non-empty manifest digest so image validity check is executed.

    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1), "sha256:invalid", "runc", "",
                        InstanceStateEnum::eActive, 0, 0, Time::Now(), "1.0.0"))
                    .IsNone());

    mInstanceRunner.Init(mLauncher);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    StaticArray<InstanceInfo, cMaxNumInstances> instances;

    ASSERT_TRUE(mStorage.GetActiveInstances(instances).IsNone());
    EXPECT_EQ(instances.Size(), 0);

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, InstancesWithOutdatedTTLRemovedOnStart)
{
    Config cfg;

    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;
    cfg.mInstanceTTL            = 1 * Time::cHours;

    // Add services to the image provider.
    oci::ItemConfig itemConfig1;
    oci::ItemConfig itemConfig2;

    CreateItemConfig(itemConfig1);
    CreateItemConfig(itemConfig2);
    const std::string emptyImage = "";

    AddItem(cService1, emptyImage, itemConfig1, CreateImageConfig());
    AddItem(cService2, emptyImage, itemConfig2, CreateImageConfig());
    auto manifestService1 = GetManifestDigest(cService1, emptyImage);
    auto manifestService2 = GetManifestDigest(cService2, emptyImage);

    // Add outdated TTL.
    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1), manifestService1, "1.0.0", "",
                        InstanceStateEnum::eCached, 5000, 0, Time::Now().Add(-25 * Time::cHours)))
                    .IsNone());

    // Add instance with current timestamp.
    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService2), manifestService2, "1.0.0", "",
                        InstanceStateEnum::eCached, 5001, 0, Time::Now()))
                    .IsNone());

    mInstanceRunner.Init(mLauncher);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    ASSERT_EQ(mStorageState.GetRemovedInstances().size(), 1);
    ASSERT_EQ(mStorageState.GetRemovedInstances()[0], CreateInstanceIdent(cService1));

    StaticArray<InstanceInfo, cMaxNumInstances> instances;

    ASSERT_TRUE(mStorage.GetActiveInstances(instances).IsNone());
    ASSERT_EQ(instances.Size(), 1);
    ASSERT_EQ(instances[0].mInstanceIdent, CreateInstanceIdent(cService2));

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, CacheInstances)
{
    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)},
        {CreateResource("resource1", 2), CreateResource("resource3", 2)});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRunc)},
        {CreateResource("resource1", 2), CreateResource("resource2", 2)});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    auto nodeInfoRemoteSM2 = CreateNodeInfo(cNodeIDRemoteSM2, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM2, nodeInfoRemoteSM2);

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2}) {
        auto nodeConfig = std::make_unique<NodeConfig>();

        CreateNodeConfig(*nodeConfig, nodeID);
        mResourceManager.SetNodeConfig(nodeID, cNodeTypeVM, *nodeConfig);
    }

    // Set up configs
    for (const auto& serviceID : {cService1, cService2, cService3}) {
        auto itemConfig = std::make_unique<oci::ItemConfig>();

        CreateItemConfig(*itemConfig, {cRunnerRunc});
        AddItem(serviceID, cImageID1, *itemConfig, CreateImageConfig());
    }

    mInstanceRunner.Init(mLauncher);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    // Run instances 1
    auto runRequest1 = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();

    runRequest1->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1, "", {}, UpdateItemTypeEnum::eService));
    runRequest1->PushBack(CreateRunRequest(cService2, cSubject1, 50, 1, "", {}, UpdateItemTypeEnum::eService));
    runRequest1->PushBack(CreateRunRequest(cService3, cSubject1, 50, 1, "", {}, UpdateItemTypeEnum::eService));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    ASSERT_TRUE(mLauncher.RunInstances(*runRequest1, *runStatuses).IsNone());

    auto instances = std::make_unique<StaticArray<InstanceInfo, cMaxNumInstances>>();

    EXPECT_TRUE(mStorage.GetActiveInstances(*instances).IsNone());
    ASSERT_EQ(instances->Size(), 3);
    EXPECT_EQ((*instances)[0].mInstanceIdent, CreateInstanceIdent(cService1, cSubject1, 0));
    EXPECT_EQ((*instances)[1].mInstanceIdent, CreateInstanceIdent(cService2, cSubject1, 0));
    EXPECT_EQ((*instances)[2].mInstanceIdent, CreateInstanceIdent(cService3, cSubject1, 0));
    EXPECT_NE((*instances)[0].mState, InstanceStateEnum::eCached);
    EXPECT_NE((*instances)[1].mState, InstanceStateEnum::eCached);
    EXPECT_NE((*instances)[2].mState, InstanceStateEnum::eCached);

    // Run instances 2
    auto runRequest2 = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();

    runRequest2->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1, "", {}, UpdateItemTypeEnum::eService));

    ASSERT_TRUE(mLauncher.RunInstances(*runRequest2, *runStatuses).IsNone());

    EXPECT_TRUE(mStorage.GetActiveInstances(*instances).IsNone());
    ASSERT_EQ(instances->Size(), 3);

    EXPECT_EQ((*instances)[0].mInstanceIdent, CreateInstanceIdent(cService1, cSubject1, 0));
    EXPECT_EQ((*instances)[1].mInstanceIdent, CreateInstanceIdent(cService2, cSubject1, 0));
    EXPECT_EQ((*instances)[2].mInstanceIdent, CreateInstanceIdent(cService3, cSubject1, 0));
    EXPECT_NE((*instances)[0].mState, InstanceStateEnum::eCached);
    EXPECT_EQ((*instances)[1].mState, InstanceStateEnum::eCached);
    EXPECT_EQ((*instances)[2].mState, InstanceStateEnum::eCached);

    // Stop launcher
    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, Components)
{
    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    // Create node info
    mNodeInfoProvider.Init();
    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)},
        {CreateResource("resource1", 2), CreateResource("resource3", 2)});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRootfs, 1)},
        {CreateResource("resource1", 2), CreateResource("resource2", 2)});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    // Create node config
    auto nodeConfig = std::make_unique<NodeConfig>();

    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    CreateNodeConfig(*nodeConfig, cNodeIDRemoteSM1, 0, {"rootfs"});
    mResourceManager.SetNodeConfig(cNodeIDRemoteSM1, cNodeTypeVM, *nodeConfig);

    // Create component config
    auto componentConfig = std::make_unique<oci::ItemConfig>();

    CreateItemConfig(*componentConfig, {cRunnerRootfs});
    AddItem(cComponent1, cRootfsImageID, *componentConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    auto instanceStatusListener = std::make_unique<InstanceStatusListenerStub>();

    mLauncher.SubscribeListener(*instanceStatusListener);

    // Run instances
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();

    runRequest->PushBack(
        CreateRunRequest(cComponent1, cSubject1, 50, 1, "", {"rootfs"}, UpdateItemTypeEnum::eComponent));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());
    ASSERT_TRUE(mLauncher.Stop().IsNone());

    // Check sent run requests
    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests;

    auto expectedStartInstances
        = CreateComponentRunInfo(CreateInstanceIdent(cComponent1, cSubject1, 0, UpdateItemTypeEnum::eComponent),
            cRootfsImageID, cRunnerRootfs, 50);

    InstanceRunnerStub::NodeRunRequest remoteRunRequest = {{}, {expectedStartInstances}};
    expectedRunRequests[cNodeIDLocalSM]                 = {};
    expectedRunRequests[cNodeIDRemoteSM1]               = remoteRunRequest;

    ASSERT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);

    // Check run status
    auto expectedRunStatus = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    auto manifestDigest = BuildManifestDigest(cComponent1, cRootfsImageID);

    expectedRunStatus->PushBack(CreateInstanceStatus(
        CreateInstanceIdent(cComponent1, cSubject1, 0, UpdateItemTypeEnum::eComponent), cNodeIDRemoteSM1, cRunnerRootfs,
        aos::InstanceStateEnum::eActivating, ErrorEnum::eNone, "", false, manifestDigest.CStr()));

    Array<InstanceStatus> componentStatuses(expectedRunStatus->begin(), expectedRunStatus->Size());

    ASSERT_EQ(instanceStatusListener->GetLastStatuses(), componentStatuses);
}

/***********************************************************************************************************************
 * Balancing tests
 **********************************************************************************************************************/

struct TestData {
    const char*                                       mTestCaseName;
    std::map<std::string, NodeConfig>                 mNodeConfigs;
    std::map<std::string, oci::ItemConfig>            mItemConfigs;
    StaticArray<InstanceInfo, cMaxNumInstances>       mStoredInstances;
    StaticArray<RunInstanceRequest, cMaxNumInstances> mRunRequests;

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> mExpectedRunRequests;
    StaticArray<InstanceStatus, cMaxNumInstances>             mExpectedRunStatus;
    std::map<std::string, monitoring::NodeMonitoringData>     mMonitoring;
    bool                                                      mRebalancing;
};

using TestDataPtr = std::shared_ptr<TestData>;

TestDataPtr TestItemNodePriority()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "node priority";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM1], cNodeIDRemoteSM1, 50);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM2], cNodeIDRemoteSM2, 0);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRunxSM], cNodeIDRunxSM, 0);

    // Item configs
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc});
    CreateItemConfig(testData->mItemConfigs[cService2], {cRunnerRunc});
    CreateItemConfig(testData->mItemConfigs[cService3], {cRunnerRunx});

    // Desired instances
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService1, cSubject1, 100, 2, "", {}, UpdateItemTypeEnum::eService));
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService2, cSubject1, 50, 2, "", {}, UpdateItemTypeEnum::eService));
    testData->mRunRequests.PushBack(CreateRunRequest(cService3, cSubject1, 0, 2, "", {}, UpdateItemTypeEnum::eService));

    // Expected run requests
    std::vector<aos::InstanceInfo> localSMRequests;
    localSMRequests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, "2", 100));
    localSMRequests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, 5000, "3", 100));
    localSMRequests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5001, "4", 50));
    localSMRequests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService2, cSubject1, 1), cImageID1, cRunnerRunc, 5003, 5001, "5", 50));
    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = std::vector<aos::InstanceInfo>();

    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = std::vector<aos::InstanceInfo>();

    std::vector<aos::InstanceInfo> runxSMRequests;
    runxSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunx, 5004, 5002, "6", 0));
    runxSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 1), cImageID1, cRunnerRunx, 5005, 5002, "7", 0));
    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances = runxSMRequests;

    // Expected run status
    auto digest1 = BuildManifestDigest(cService1, cImageID1);
    auto digest2 = BuildManifestDigest(cService2, cImageID1);
    auto digest3 = BuildManifestDigest(cService3, cImageID1);

    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 1),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 0),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 1),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService3, cSubject1, 0),
        cNodeIDRunxSM, cRunnerRunx, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest3.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService3, cSubject1, 1),
        cNodeIDRunxSM, cRunnerRunx, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest3.CStr()));

    return testData;
}

TestDataPtr TestItemLabels()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "labels";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100, {"label1"});
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM1], cNodeIDRemoteSM1, 50, {"label2"});
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM2], cNodeIDRemoteSM2, 0);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRunxSM], cNodeIDRunxSM, 0);

    // Item configs
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc});
    CreateItemConfig(testData->mItemConfigs[cService2], {cRunnerRunc});
    CreateItemConfig(testData->mItemConfigs[cService3], {cRunnerRunx});

    // Desired instances
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService1, cSubject1, 100, 2, "", {"label2"}, UpdateItemTypeEnum::eService));
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService2, cSubject1, 50, 2, "", {"label1"}, UpdateItemTypeEnum::eService));
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService3, cSubject1, 0, 2, "", {"label1"}, UpdateItemTypeEnum::eService));
    // Expected run requests
    std::vector<aos::InstanceInfo> localSMRequests;
    localSMRequests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5001, "4", 50));
    localSMRequests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService2, cSubject1, 1), cImageID1, cRunnerRunc, 5003, 5001, "5", 50));
    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    std::vector<aos::InstanceInfo> remoteSM1Requests;
    remoteSM1Requests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, "2", 100));
    remoteSM1Requests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, 5000, "3", 100));
    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = remoteSM1Requests;

    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = std::vector<aos::InstanceInfo>();

    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances = std::vector<aos::InstanceInfo>();

    // Expected run status
    auto digest1 = BuildManifestDigest(cService1, cImageID1);
    auto digest2 = BuildManifestDigest(cService2, cImageID1);

    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 1),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 0),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 1),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService3, cSubject1, 0), "", "",
        aos::InstanceStateEnum::eFailed, Error(ErrorEnum::eNotFound, "no nodes with instance labels")));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService3, cSubject1, 1), "", "",
        aos::InstanceStateEnum::eFailed, Error(ErrorEnum::eNotFound, "no nodes with instance labels")));

    return testData;
}

TestDataPtr TestItemResources()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "resources";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM1], cNodeIDRemoteSM1, 50);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM2], cNodeIDRemoteSM2, 0);

    // Item configs
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, {}, {}, {}, {}, {}, {"resource1", "resource2"});
    CreateItemConfig(testData->mItemConfigs[cService2], {cRunnerRunc}, {}, {}, {}, {}, {}, {"resource1"});
    CreateItemConfig(testData->mItemConfigs[cService3], {cRunnerRunc}, {}, {}, {}, {}, {}, {"resource3"});
    // Desired instances
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService1, cSubject1, 100, 2, "", {}, UpdateItemTypeEnum::eService));
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService2, cSubject1, 50, 2, "", {}, UpdateItemTypeEnum::eService));
    testData->mRunRequests.PushBack(CreateRunRequest(cService3, cSubject1, 0, 2, "", {}, UpdateItemTypeEnum::eService));

    // Expected run requests
    std::vector<aos::InstanceInfo> localSMRequests;
    std::vector<aos::InstanceInfo> remoteSMRequests;

    localSMRequests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5001, "4", 50));
    localSMRequests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService2, cSubject1, 1), cImageID1, cRunnerRunc, 5003, 5001, "5", 50));
    localSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5004, 5002, "6", 0));
    localSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 1), cImageID1, cRunnerRunc, 5005, 5002, "7", 0));
    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    remoteSMRequests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, "2", 100));
    remoteSMRequests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, 5000, "3", 100));
    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = remoteSMRequests;

    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = std::vector<aos::InstanceInfo>();

    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances = std::vector<aos::InstanceInfo>();

    // Expected run status
    auto digest1 = BuildManifestDigest(cService1, cImageID1);
    auto digest2 = BuildManifestDigest(cService2, cImageID1);
    auto digest3 = BuildManifestDigest(cService3, cImageID1);

    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 1),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 0),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 1),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService3, cSubject1, 0),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest3.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService3, cSubject1, 1),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest3.CStr()));

    return testData;
}

TestDataPtr TestItemStorageRatio()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "storage ratio";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100);

    // Item configs
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, {}, CreateServiceQuotas(500, 0, 0, 0),
        CreateRequestedResources(300, 0, 0, 0));

    // Desired instances
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 5));

    // Expected run requests - only successfully scheduled instances (0, 1, 2) are sent to nodes
    // Instances 3 and 4 fail before being sent due to storage quota limits
    static const char* cIpSuffixes[]   = {"2", "3", "4"};
    auto&              localSMRequests = testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances;
    auto               digest1         = BuildManifestDigest(cService1, cImageID1);

    for (size_t i = 0; i < 3; ++i) {
        localSMRequests.push_back(CreateServiceRunInfo(
            CreateInstanceIdent(cService1, cSubject1, i), cImageID1, cRunnerRunc, 5000 + i, 5000, cIpSuffixes[i], 100));
        testData->mExpectedRunStatus.PushBack(
            CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, static_cast<uint64_t>(i)), cNodeIDLocalSM,
                cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    }

    // Failed instances (3, 4) - not sent to nodes, fail immediately
    for (size_t i = 3; i < 5; ++i) {
        testData->mExpectedRunStatus.PushBack(
            CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, static_cast<uint64_t>(i)), "", "",
                aos::InstanceStateEnum::eFailed, Error(ErrorEnum::eNoMemory)));
    }

    // Initialize empty requests for other nodes
    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = std::vector<aos::InstanceInfo>();
    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = std::vector<aos::InstanceInfo>();
    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances    = std::vector<aos::InstanceInfo>();

    return testData;
}

TestDataPtr TestItemStateRatio()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "state ratio";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100);

    // Item configs
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, {}, CreateServiceQuotas(0, 500, 0, 0),
        CreateRequestedResources(0, 300, 0, 0));

    // Desired instances
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 5));

    // Expected run requests - only successfully scheduled instances (0, 1, 2) are sent to nodes
    // Instances 3 and 4 fail before being sent due to state quota limits
    static const char* cStateIpSuffixes[] = {"2", "3", "4"};
    auto&              stateLocalRequests = testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances;
    auto               digest1            = BuildManifestDigest(cService1, cImageID1);

    for (size_t i = 0; i < 3; ++i) {
        stateLocalRequests.push_back(CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, i), cImageID1,
            cRunnerRunc, 5000 + i, 5000, cStateIpSuffixes[i], 100));
        testData->mExpectedRunStatus.PushBack(
            CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, static_cast<uint64_t>(i)), cNodeIDLocalSM,
                cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    }

    // Failed instances (3, 4) - not sent to nodes, fail immediately
    for (size_t i = 3; i < 5; ++i) {
        testData->mExpectedRunStatus.PushBack(
            CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, static_cast<uint64_t>(i)), "", "",
                aos::InstanceStateEnum::eFailed, Error(ErrorEnum::eNoMemory)));
    }

    // Initialize empty requests for other nodes
    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = std::vector<aos::InstanceInfo>();
    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = std::vector<aos::InstanceInfo>();
    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances    = std::vector<aos::InstanceInfo>();

    return testData;
}

TestDataPtr TestItemCpuRatio()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "cpu ratio";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100);

    // Item configs
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, {}, CreateServiceQuotas(0, 0, 500, 0),
        CreateRequestedResources(0, 0, 300, 0));

    // Desired instances
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 5));

    // Expected run requests - all 5 instances are scheduled and distributed across nodes
    static const char* cCpuIpSuffixes[] = {"2", "3", "4", "5", "6"};
    auto&              cpuLocalRequests = testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances;
    auto               digest1          = BuildManifestDigest(cService1, cImageID1);

    // Instances 0, 1, 2 on localSM
    for (size_t i = 0; i < 3; ++i) {
        cpuLocalRequests.push_back(CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, i), cImageID1,
            cRunnerRunc, 5000 + i, 5000, cCpuIpSuffixes[i], 100));
        testData->mExpectedRunStatus.PushBack(
            CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, static_cast<uint64_t>(i)), cNodeIDLocalSM,
                cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    }

    // Instance 3 on remoteSM1
    auto& cpuRemoteSM1Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances;
    cpuRemoteSM1Requests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 3), cImageID1, cRunnerRunc, 5003, 5000, cCpuIpSuffixes[3], 100));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 3),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));

    // Instance 4 on remoteSM2
    auto& cpuRemoteSM2Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances;
    cpuRemoteSM2Requests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 4), cImageID1, cRunnerRunc, 5004, 5000, cCpuIpSuffixes[4], 100));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 4),
        cNodeIDRemoteSM2, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));

    // Initialize empty requests for runxSM
    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances = std::vector<aos::InstanceInfo>();

    return testData;
}

TestDataPtr TestItemRamRatio()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "ram ratio";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100);

    // Item configs
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, {}, CreateServiceQuotas(0, 0, 0, 500),
        CreateRequestedResources(0, 0, 0, 300));

    // Desired instances
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 5));

    // Expected run requests - instances distributed across nodes
    static const char* cRamIpSuffixes[] = {"2", "3", "4", "5", "6"};
    auto&              ramLocalRequests = testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances;
    auto               digest1          = BuildManifestDigest(cService1, cImageID1);

    // Instances 0, 1, 2 on localSM
    for (size_t i = 0; i < 3; ++i) {
        ramLocalRequests.push_back(CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, i), cImageID1,
            cRunnerRunc, 5000 + i, 5000, cRamIpSuffixes[i], 100));
        testData->mExpectedRunStatus.PushBack(
            CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, static_cast<uint64_t>(i)), cNodeIDLocalSM,
                cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    }

    // Instance 3 on remoteSM1
    auto& ramRemoteSM1Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances;
    ramRemoteSM1Requests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 3), cImageID1, cRunnerRunc, 5003, 5000, cRamIpSuffixes[3], 100));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 3),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));

    // Instance 4 on remoteSM2
    auto& ramRemoteSM2Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances;
    ramRemoteSM2Requests.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 4), cImageID1, cRunnerRunc, 5004, 5000, cRamIpSuffixes[4], 100));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 4),
        cNodeIDRemoteSM2, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));

    // runxSM is present in the test environment; expect an empty request for it
    testData->mExpectedRunRequests[cNodeIDRunxSM] = {};

    return testData;
}

TestDataPtr TestItemRebalancing()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "rebalancing";
    testData->mRebalancing  = true;

    // Node configs
    auto alertRules = CreateAlertRules(75.0, 85.0);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100, {}, {}, alertRules);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM1], cNodeIDRemoteSM1, 50, {}, {}, alertRules);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM2], cNodeIDRemoteSM2, 50, {}, {}, alertRules);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRunxSM], cNodeIDRunxSM, 0, {}, {}, alertRules);

    // Item configs
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, oci::BalancingPolicyEnum::eNone,
        CreateServiceQuotas(0, 0, 1000, 0));
    CreateItemConfig(testData->mItemConfigs[cService2], {cRunnerRunc}, oci::BalancingPolicyEnum::eNone,
        CreateServiceQuotas(0, 0, 1000, 0));
    CreateItemConfig(testData->mItemConfigs[cService3], {cRunnerRunc}, oci::BalancingPolicyEnum::eNone,
        CreateServiceQuotas(0, 0, 1000, 0));

    // Desired instances with priorities
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 1));
    testData->mRunRequests.PushBack(CreateRunRequest(cService2, cSubject1, 50, 1));
    testData->mRunRequests.PushBack(CreateRunRequest(cService3, cSubject1, 50, 1));

    // Expected run requests - final state after rebalancing
    // After rebalancing: service1 on localSM, service2 on remoteSM1, service3 on remoteSM2
    // Initial: service1 and service2 on localSM, service3 on remoteSM1
    // During rebalancing: service2 moves from localSM to remoteSM1, service3 moves from remoteSM1 to remoteSM2

    // localSM: starts service1, stops service2 (which was initially scheduled there)
    // stopInstances come from mSentInstances which now have mManifestDigest from GetInfo()
    auto& rebalancingLocalRequests = testData->mExpectedRunRequests[cNodeIDLocalSM];
    rebalancingLocalRequests.mStartInstances.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, "5", 100));
    rebalancingLocalRequests.mStopInstances.push_back(
        CreateAosStopInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cRunnerRunc));

    // remoteSM1: starts service2, stops service3 (which was initially scheduled there)
    auto& rebalancingRemoteSM1Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM1];
    rebalancingRemoteSM1Requests.mStartInstances.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5001, 5001, "6", 50));
    rebalancingRemoteSM1Requests.mStopInstances.push_back(
        CreateAosStopInstanceInfo(CreateInstanceIdent(cService3, cSubject1, 0), cRunnerRunc));

    // remoteSM2: starts service3, no stops
    auto& rebalancingRemoteSM2Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM2];
    rebalancingRemoteSM2Requests.mStartInstances.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5002, "7", 50));
    testData->mExpectedRunRequests[cNodeIDRunxSM] = {};

    // Expected run status
    auto digest1 = BuildManifestDigest(cService1, cImageID1);
    auto digest2 = BuildManifestDigest(cService2, cImageID1);
    auto digest3 = BuildManifestDigest(cService3, cImageID1);

    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 0),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService3, cSubject1, 0),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest3.CStr()));

    // Monitoring data
    CreateNodeMonitoring(testData->mMonitoring[cNodeIDLocalSM], cNodeIDLocalSM, 1000,
        {CreateInstanceMonitoring(CreateInstanceIdent(cService1, cSubject1, 0), 500),
            CreateInstanceMonitoring(CreateInstanceIdent(cService2, cSubject1, 0), 500)});

    return testData;
}

TestDataPtr TestItemRebalancingPolicy()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "rebalancing policy";
    testData->mRebalancing  = true;

    // Node configs
    auto alertRules = CreateAlertRules(75.0, 85.0);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100, {}, {}, alertRules);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM1], cNodeIDRemoteSM1, 50, {}, {}, alertRules);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM2], cNodeIDRemoteSM2, 50, {}, {}, alertRules);

    // Service configs
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, oci::BalancingPolicyEnum::eNone,
        CreateServiceQuotas(0, 0, 1000, 0), {}, {});
    CreateItemConfig(testData->mItemConfigs[cService2], {cRunnerRunc}, oci::BalancingPolicyEnum::eNone,
        CreateServiceQuotas(0, 0, 1000, 0));
    CreateItemConfig(testData->mItemConfigs[cService3], {cRunnerRunc}, oci::BalancingPolicyEnum::eBalancingDisabled,
        CreateServiceQuotas(0, 0, 1000, 0));

    // Desired instances with priorities
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 1));
    testData->mRunRequests.PushBack(CreateRunRequest(cService2, cSubject1, 50, 1));
    testData->mRunRequests.PushBack(CreateRunRequest(cService3, cSubject1, 50, 1));

    // Expected run requests - final state after rebalancing
    // Initial: service1 and service2 on localSM, service3 on remoteSM1
    // After rebalancing: service1 on localSM, service2 on remoteSM2, service3 on remoteSM1(policy is applied)

    // localSM: starts service1, stops service2 (which was initially scheduled there)
    auto& policyLocalRequests = testData->mExpectedRunRequests[cNodeIDLocalSM];
    policyLocalRequests.mStartInstances.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, "6", 100));
    policyLocalRequests.mStopInstances.push_back(
        CreateAosStopInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cRunnerRunc));

    // remoteSM1: starts service3 (stays there, no stops since service3 has BalancingDisabled)
    auto& policyRemoteSM1Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM1];
    policyRemoteSM1Requests.mStartInstances.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5002, "5", 50));

    // remoteSM2: starts service2, no stops
    auto& policyRemoteSM2Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM2];
    policyRemoteSM2Requests.mStartInstances.push_back(CreateServiceRunInfo(
        CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5001, 5001, "7", 50));
    testData->mExpectedRunRequests[cNodeIDRunxSM] = {};

    // Expected run status (sorted by priority desc, then itemID asc: service1(100), service2(50), service3(50))
    // Initial state after RunInstances() (before rebalancing): service1 and service2 on localSM, service3 on remoteSM1
    auto digest1 = BuildManifestDigest(cService1, cImageID1);
    auto digest2 = BuildManifestDigest(cService2, cImageID1);
    auto digest3 = BuildManifestDigest(cService3, cImageID1);

    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 0),
        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService3, cSubject1, 0),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest3.CStr()));

    // Monitoring data
    CreateNodeMonitoring(testData->mMonitoring[cNodeIDLocalSM], cNodeIDLocalSM, 1000,
        {CreateInstanceMonitoring(CreateInstanceIdent(cService1, cSubject1, 0), 500),
            CreateInstanceMonitoring(CreateInstanceIdent(cService2, cSubject1, 0), 500)});

    return testData;
}

TEST_F(CMLauncherTest, Balancing)
{
    Config cfg;

    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    const std::vector<const char*> nodeIDs = {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2, cNodeIDRunxSM};

    std::vector<TestDataPtr> testItems = {
        TestItemNodePriority(),
        TestItemLabels(),
        TestItemResources(),
        TestItemStorageRatio(),
        TestItemStateRatio(),
        TestItemCpuRatio(),
        TestItemRamRatio(),
        TestItemRebalancing(),
        TestItemRebalancingPolicy(),
    };

    for (size_t i = 0; i < testItems.size(); ++i) {
        auto& testItem = *testItems[i];

        LOG_INF();
        LOG_INF() << "Test case: " << testItem.mTestCaseName;

        // Initialize all stubs
        mStorageState.Init();
        mStorageState.SetTotalStateSize(1024);
        mStorageState.SetTotalStorageSize(1024);

        mNodeInfoProvider.Init();
        auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)},
            {CreateResource("resource1", 2), CreateResource("resource3", 2)});
        mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

        auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRunc)},
            {CreateResource("resource1", 2), CreateResource("resource2", 2)});
        mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

        auto nodeInfoRemoteSM2 = CreateNodeInfo(cNodeIDRemoteSM2, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
        mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM2, nodeInfoRemoteSM2);

        auto nodeInfoRunxSM = CreateNodeInfo(cNodeIDRunxSM, 1000, 1024, {CreateRuntime(cRunnerRunx)}, {});
        mNodeInfoProvider.AddNodeInfo(cNodeIDRunxSM, nodeInfoRunxSM);

        mImageStore.Init();
        mNetworkManager.Init();
        mInstanceRunner.Init(mLauncher);
        mInstanceStatusProvider.Init();
        mMonitoringProvider.Init();
        mAlertsProvider.Init();
        mResourceManager.Init();
        mStorage.Init(testItem.mStoredInstances);

        // Set up configs
        for (const auto& [itemID, config] : testItem.mItemConfigs) {
            AddItem(itemID, cImageID1, config, CreateImageConfig());
        }

        for (const auto& nodeID : nodeIDs) {
            auto nodeConfigIt = testItem.mNodeConfigs.find(nodeID);
            if (nodeConfigIt != testItem.mNodeConfigs.end()) {
                mResourceManager.SetNodeConfig(nodeID, cNodeTypeVM, nodeConfigIt->second);
                continue;
            }

            auto nodeConfig = std::make_shared<NodeConfig>();

            CreateNodeConfig(*nodeConfig, nodeID);
            mResourceManager.SetNodeConfig(nodeID, cNodeTypeVM, *nodeConfig);
        }

        mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

        // Init launcher
        ASSERT_TRUE(mLauncher
                        .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                            mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                            ValidateGID, ValidateUID, mStorage)
                        .IsNone());

        InstanceStatusListenerStub instanceStatusListener;
        mLauncher.SubscribeListener(instanceStatusListener);

        ASSERT_TRUE(mLauncher.Start().IsNone());

        // Run instances
        auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
        ASSERT_TRUE(mLauncher.RunInstances(testItem.mRunRequests, *runStatuses).IsNone());

        ASSERT_EQ(*runStatuses, testItem.mExpectedRunStatus);
        ASSERT_EQ(instanceStatusListener.GetLastStatuses(), testItem.mExpectedRunStatus);

        // Rebalance
        if (testItem.mRebalancing) {
            for (const auto& [nodeID, monitoring] : testItem.mMonitoring) {
                mMonitoringProvider.SetAverageMonitoring(nodeID.c_str(), monitoring);
            }

            // Get current notification count before triggering alert
            size_t currentNotifyCount = instanceStatusListener.GetNotifyCount();

            mAlertsProvider.TriggerSystemQuotaAlert();

            // Wait for rebalancing to complete (expect at least 2 more notifications:
            // one from rebalancing and one from status updates after rebalancing)
            using namespace std::chrono_literals;
            ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(currentNotifyCount + 3, 2000ms));
        }

        ASSERT_TRUE(mLauncher.Stop().IsNone());
        mLauncher.UnsubscribeListener(instanceStatusListener);

        // Check sent run requests
        ASSERT_EQ(mInstanceRunner.GetRunRequests(), testItem.mExpectedRunRequests);
    }
}

TEST_F(CMLauncherTest, PlatformFiltering)
{
    Config cfg;

    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mNetworkManager.Init();
    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();

    // Node 1: arm64/linux runtime
    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024,
        {CreateRuntime(cRunnerRunc, 0, CreatePlatform("arm64", "generic", "linux", "5.4.0", "feature1"))},
        {CreateResource("resource1", 2), CreateResource("resource3", 2)});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    // Node 2: x86_64/windows runtime
    auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024,
        {CreateRuntime(cRunnerRunc, 0, CreatePlatform("x86_64", "generic", "windows", "5.4.0", "feature1"))}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    // Node 3: x86_64/linux runtime (should match service3)
    auto nodeInfoRemoteSM2 = CreateNodeInfo(cNodeIDRemoteSM2, 1000, 1024,
        {CreateRuntime(cRunnerRunc, 0, CreatePlatform("x86_64", "generic", "linux", "5.4.0", "feature1"))}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM2, nodeInfoRemoteSM2);

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2}) {
        auto nodeConfig = std::make_unique<NodeConfig>();

        CreateNodeConfig(*nodeConfig, nodeID);
        mResourceManager.SetNodeConfig(nodeID, cNodeTypeVM, *nodeConfig);
    }

    // Service1 requires arm32/linux - rejected (no arm32 runtime)
    auto itemConfig1 = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig1, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig1, CreateImageConfig("arm32", "generic", "linux", "5.4.0", "feature1"));

    // Service2 requires x86_64/macos - rejected (no macos OS)
    auto itemConfig2 = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig2, {cRunnerRunc});
    AddItem(cService2, cImageID1, *itemConfig2, CreateImageConfig("x86_64", "generic", "macos", "5.4.0", "feature1"));

    // Service3 requires x86_64/linux - matches remoteSM2
    auto itemConfig3 = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig3, {cRunnerRunc});
    AddItem(cService3, cImageID1, *itemConfig3, CreateImageConfig("x86_64", "generic", "linux", "5.4.0", "feature1"));

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    // Run instances
    auto runRequests = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequests->PushBack(CreateRunRequest(cService1, cSubject1, 100, 1));
    runRequests->PushBack(CreateRunRequest(cService2, cSubject1, 50, 1));
    runRequests->PushBack(CreateRunRequest(cService3, cSubject1, 25, 1));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequests, *runStatuses).IsNone());
    ASSERT_TRUE(mLauncher.Stop().IsNone());

    // Check sent run requests - only service3 should be scheduled
    InstanceRunnerStub::NodeRunRequest remoteSM2Request = {{},
        {CreateServiceRunInfo(
            CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5002, "2", 25)}};

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests;

    expectedRunRequests[cNodeIDLocalSM]   = {};
    expectedRunRequests[cNodeIDRemoteSM1] = {};
    expectedRunRequests[cNodeIDRemoteSM2] = remoteSM2Request;

    ASSERT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);

    // Check run status - service1 and service2 should fail, service3 should succeed
    auto expectedRunStatus = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    auto manifestDigest    = BuildManifestDigest(cService3, cImageID1);

    expectedRunStatus->PushBack(CreateInstanceStatus(
        CreateInstanceIdent(cService1, cSubject1, 0), "", "", aos::InstanceStateEnum::eFailed, ErrorEnum::eNotFound));
    expectedRunStatus->PushBack(CreateInstanceStatus(
        CreateInstanceIdent(cService2, cSubject1, 0), "", "", aos::InstanceStateEnum::eFailed, ErrorEnum::eNotFound));
    expectedRunStatus->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService3, cSubject1, 0), cNodeIDRemoteSM2,
        cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, manifestDigest.CStr()));

    ASSERT_EQ(instanceStatusListener.GetLastStatuses(), *expectedRunStatus);
}

TEST_F(CMLauncherTest, ResendInstancesOnMismatchedNodeStatus)
{
    using namespace std::chrono_literals;

    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    // Initialize stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mNetworkManager.Init();
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    // Item config
    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher, false, aos::InstanceStateEnum::eActive);

    // First request: send empty statuses (auto-update disabled => empty statuses).
    // After first request is prepared, enable auto-update so the next request sends correct statuses from
    // startInstances.
    EXPECT_CALL(mInstanceRunner, OnRunRequest()).Times(2).WillRepeatedly([&]() {
        mInstanceRunner.SetAutoUpdateStatuses(true);
    });

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    // Run a single instance on a single node.
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    // Expect 3 status notifications:
    // - 1st: from OnNodeInstancesStatusesReceived() for the initial (wrong/empty) node status update
    // - 2nd: from Launcher::RunInstances() completion notification
    // - 3rd: from OnNodeInstancesStatusesReceived() after resend with correct statuses
    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(3, 2000ms));

    // Stop launcher.
    ASSERT_TRUE(mLauncher.Stop().IsNone());

    // Verify latest instance statuses are correct (after resend).
    auto                        manifestDigest   = BuildManifestDigest(cService1, cImageID1);
    std::vector<InstanceStatus> expectedStatuses = {
        CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), cNodeIDLocalSM, cRunnerRunc,
            aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, manifestDigest.CStr()),
    };

    ASSERT_EQ(instanceStatusListener.GetLastStatuses(),
        Array<InstanceStatus>(expectedStatuses.data(), expectedStatuses.size()));
}

TEST_F(CMLauncherTest, SubjectChanged)
{
    using namespace std::chrono_literals;

    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    // Initialize stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mNetworkManager.Init();
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    // Item config
    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Start().IsNone());

    // 1) Run a single instance with a single subject.
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    // Wait until we receive notification after run instances.
    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(2, 2000ms));

    // 2) Change subjects (remove all of them).
    ASSERT_TRUE(mIdentProvider.SetSubjects({}).IsNone());

    // Wait until we receive notification caused by rebalance after subjects update.
    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(3, 2000ms));
    ASSERT_EQ(instanceStatusListener.GetLastStatuses(), Array<InstanceStatus>());

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, PrepareNetworkParamsFails)
{
    using namespace std::chrono_literals;

    Config cfg;

    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    // Initialize stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mNetworkManager.Init();
    mNetworkManager.SetFailOnPrepare(true);
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    // Item config
    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Start().IsNone());

    // Run a single instance.
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(1, 2000ms));

    // Verify that instance failed because of PrepareNetworkParams error.
    const auto& lastStatuses = instanceStatusListener.GetLastStatuses();
    ASSERT_EQ(lastStatuses.Size(), 1U);

    const auto& status = lastStatuses[0];

    ASSERT_EQ(static_cast<const InstanceIdent&>(status), CreateInstanceIdent(cService1, cSubject1, 0));
    ASSERT_EQ(status.mState, aos::InstanceStateEnum::eFailed);
    ASSERT_FALSE(status.mError.IsNone());

    // Stop launcher and unsubscribe listener.
    ASSERT_TRUE(mLauncher.Stop().IsNone());
    ASSERT_TRUE(mLauncher.UnsubscribeListener(instanceStatusListener).IsNone());
}

TEST_F(CMLauncherTest, TestSentInstanceInfo)
{
    const std::string version = "1.2.3";
    const std::string ownerID = "owner123";

    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    // Set up item config with alert rules and quotas
    auto alertRules = CreateAlertRules(75.0, 85.0);
    auto itemConfig = std::make_unique<oci::ItemConfig>();

    CreateItemConfig(*itemConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eNone, CreateServiceQuotas(500, 300, 0, 0),
        CreateRequestedResources(100, 50, 0, 0), alertRules);

    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig(), version);

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    // Run instance with version and ownerID
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(
        cService1, cSubject1, 100, 1, ownerID, {}, UpdateItemTypeEnum::eService, version, SubjectTypeEnum::eUser));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(1, std::chrono::seconds(2)));

    // Verify sent instance info is correct
    auto expectedInstanceInfo = CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1,
        cRunnerRunc, 5000, 5000, "2", 100, version, alertRules, SubjectTypeEnum::eUser, ownerID);

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests = {
        {cNodeIDLocalSM, {{}, {expectedInstanceInfo}}},
    };
    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);

    auto storedInstanceInfo = std::make_unique<InstanceInfo>();
    ASSERT_TRUE(mStorage.GetInstance(CreateInstanceIdent(cService1, cSubject1, 0), *storedInstanceInfo).IsNone());

    // Verify stored instance
    auto expectedStoredInstanceInfo = CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0),
        BuildManifestDigest(cService1, cImageID1), cRunnerRunc, cNodeIDLocalSM, InstanceStateEnum::eActive, 5000, 5000,
        storedInstanceInfo->mTimestamp, version.c_str(), true, ownerID.c_str(), SubjectTypeEnum::eUser, 100);

    EXPECT_EQ(*storedInstanceInfo, expectedStoredInstanceInfo);

    // Stop launcher
    mLauncher.UnsubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, PreinstalledComponents)
{
    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    // Set up item config for regular instance
    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    // Set preinstalled component that will be included in status updates
    auto preinstalledStatus
        = CreateInstanceStatus(CreateInstanceIdent(cComponent1, cSubject1, 0, UpdateItemTypeEnum::eComponent),
            cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "1.0.0", true);
    mInstanceRunner.SetPreinstalledComponents({preinstalledStatus});

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    // Run instance
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 100, 1));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(1, std::chrono::seconds(2)));

    // Verify both preinstalled component and regular instance appear in instance statuses
    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.GetInstancesStatuses(*statuses).IsNone());

    auto           manifestDigest = BuildManifestDigest(cService1, cImageID1);
    InstanceStatus expectedRegularStatus
        = CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), cNodeIDLocalSM, cRunnerRunc,
            aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, manifestDigest.CStr());

    std::vector<InstanceStatus> expectedStatuses = {expectedRegularStatus, preinstalledStatus};

    EXPECT_EQ(*statuses, Array<InstanceStatus>(expectedStatuses.data(), expectedStatuses.size()));

    // Stop launcher
    mLauncher.UnsubscribeListener(instanceStatusListener);
    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, SetStatusOnStart)
{
    using namespace std::chrono_literals;

    Config cfg;

    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    // Initialize stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mNetworkManager.Init();
    mNetworkManager.SetFailOnPrepare(true);
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    // Item config
    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig(), "1.0.0");

    mInstanceRunner.Init(mLauncher);

    // Add two instances to storage
    auto manifestDigest = BuildManifestDigest(cService1, cImageID1);
    auto instance1
        = CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), manifestDigest, cRunnerRunc, cNodeIDLocalSM,
            InstanceStateEnum::eActive, 5001, 0, Time::Now(), "1.0.0", false, "", SubjectTypeEnum::eGroup, 100);

    auto instance2
        = CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), manifestDigest, cRunnerRunc, cNodeIDLocalSM,
            InstanceStateEnum::eActive, 5001, 0, Time::Now(), "1.0.0", false, "", SubjectTypeEnum::eGroup, 100);

    ASSERT_TRUE(mStorage.AddInstance(instance1).IsNone());
    ASSERT_TRUE(mStorage.AddInstance(instance2).IsNone());

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage)
                    .IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Start().IsNone());

    // Verify that both instances are activating
    InstanceStatus expectedStatus1 = CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), cNodeIDLocalSM,
        cRunnerRunc, aos::InstanceStateEnum::eActivating, ErrorEnum::eNone, "1.0.0", false, manifestDigest.CStr());

    InstanceStatus expectedStatus2 = CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 1), cNodeIDLocalSM,
        cRunnerRunc, aos::InstanceStateEnum::eActivating, ErrorEnum::eNone, "1.0.0", false, manifestDigest.CStr());

    std::vector<InstanceStatus> expectedStatuses = {expectedStatus1, expectedStatus2};

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    ASSERT_TRUE(mLauncher.GetInstancesStatuses(*statuses).IsNone());
    EXPECT_EQ(*statuses, Array<InstanceStatus>(expectedStatuses.data(), expectedStatuses.size()));

    // Stop launcher and unsubscribe listener
    ASSERT_TRUE(mLauncher.Stop().IsNone());
    ASSERT_TRUE(mLauncher.UnsubscribeListener(instanceStatusListener).IsNone());
}

} // namespace aos::cm::launcher
