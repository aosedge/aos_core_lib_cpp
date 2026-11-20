/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <core/cm/launcher/launcher.hpp>
#include <core/cm/tests/mocks/nodeinfoprovidermock.hpp>
#include <core/common/tests/mocks/identprovidermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>

#include "stubs/alertsproviderstub.hpp"
#include "stubs/identproviderstub.hpp"
#include "stubs/imagestorestub.hpp"
#include "stubs/instancerunnerstub.hpp"
#include "stubs/instancestatusproviderstub.hpp"
#include "stubs/monitoringproviderstub.hpp"
#include "stubs/nodeinfoproviderstub.hpp"
#include "stubs/resourcemanagerstub.hpp"
#include "stubs/senderstub.hpp"
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

        LOG_INF() << "Launcher size" << Log::Field("size", sizeof(Launcher));

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

    // Allocator must be declared before any object allocated from it to ensure correct destruction order.
    HeapAllocator mAllocator;

    // Stub objects
    alerts::AlertsProviderStub             mAlertsProvider;
    imagemanager::ImageStoreStub           mImageStore;
    iamclient::IdentProviderStub           mIdentProvider;
    nodeinfoprovider::NodeInfoProviderStub mNodeInfoProvider;
    NiceMock<launcher::InstanceRunnerStub> mInstanceRunner;
    launcher::InstanceStatusProviderStub   mInstanceStatusProvider;
    launcher::MonitoringProviderStub       mMonitoringProvider;
    resourcemanager::ResourceManagerStub   mResourceManager;
    StorageStub                            mStorage;
    storagestate::StorageStateStub         mStorageState;
    SenderStub                             mSender;

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

    result.mInstanceIdent = instance;

    if (!version.IsEmpty()) {
        result.mInstanceIdent.mVersion = version;
    }

    result.mManifestDigest = manifestDigest;
    result.mRuntimeID      = runtimeID;
    result.mNodeID         = nodeID;
    result.mPrevNodeID     = "";
    result.mUID            = uid != 0 ? uid : GenerateUID();
    result.mGID            = gid;
    result.mTimestamp      = timestamp;
    result.mState          = instanceState;
    result.mIndexDigest
        = imagemanager::ImageStoreStub::BuildIndexDigest(instance.mItemID, result.mInstanceIdent.mVersion);
    result.mIsUnitSubject = isUnitSubject;
    result.mOwnerID       = ownerID;
    result.mSubjectType   = subjectType;
    result.mPriority      = priority;

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
    result.mPreinstalled                = preinstalled;

    if (!version.empty()) {
        result.mVersion = version.c_str();
    }

    result.mManifestDigest = manifestDigest.c_str();

    if (!error.IsNone()) {
        result.mState = aos::InstanceStateEnum::eFailed;
    }

    return result;
}

InstanceIdent CreateInstanceIdent(const std::string& itemID, const std::string& subjectID = "", uint64_t instance = 0,
    UpdateItemType updateItemType = UpdateItemTypeEnum::eService)
{
    return InstanceIdent {itemID.c_str(), "", subjectID.c_str(), instance, updateItemType};
}

RunInstanceRequest CreateRunRequest(const std::string& itemID, const std::string& subjectID = "", uint64_t priority = 0,
    size_t numInstances = 1, const std::string& ownerID = "", const std::vector<std::string>& labels = {},
    UpdateItemType updateItemType = UpdateItemTypeEnum::eService, const std::string& version = "",
    SubjectTypeEnum subjectType = SubjectTypeEnum::eGroup)
{
    RunInstanceRequest request;

    request.mItemID                   = itemID.c_str();
    request.mUpdateItemType           = updateItemType;
    request.mVersion                  = version.c_str();
    request.mIndexDigest              = imagemanager::ImageStoreStub::BuildIndexDigest(itemID.c_str(), version.c_str());
    request.mOwnerID                  = ownerID.c_str();
    request.mSubjectInfo.mSubjectID   = subjectID.c_str();
    request.mSubjectInfo.mSubjectType = subjectType;
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
    const std::string& runtimeID, uint32_t uid, gid_t gid, uint64_t priority, const std::string& version = "",
    const Optional<AlertRules>& alertRules = {}, SubjectTypeEnum subjectType = SubjectTypeEnum::eGroup,
    const std::string& ownerID = "", const EnvVarArray& envVars = {})
{
    aos::InstanceInfo result;

    static_cast<InstanceIdent&>(result) = id;

    if (!version.empty()) {
        result.mVersion = version.c_str();
    }

    result.mManifestDigest = BuildManifestDigest(id.mItemID.CStr(), imageID);
    result.mRuntimeID      = runtimeID.c_str();
    result.mOwnerID        = ownerID.c_str();
    result.mUID            = uid;
    result.mGID            = gid;
    result.mPriority       = priority;
    result.mSubjectType    = subjectType;
    result.mStoragePath    = "storage_path";
    result.mStatePath      = "state_path";
    result.mEnvVars        = envVars;

    result.mMonitoringParams.EmplaceValue();
    if (alertRules.HasValue()) {
        result.mMonitoringParams.GetValue().mAlertRules = alertRules;
    }

    return result;
}

aos::InstanceInfo CreateRunInfoForStoppedService(const InstanceIdent& id, const std::string& imageID,
    const std::string& runtimeID, uint32_t uid, gid_t gid, uint64_t priority, const std::string& version = "",
    const Optional<AlertRules>& alertRules = {}, SubjectTypeEnum subjectType = SubjectTypeEnum::eGroup,
    const std::string& ownerID = "", const EnvVarArray& envVars = {})
{
    aos::InstanceInfo result = CreateServiceRunInfo(
        id, imageID, runtimeID, uid, gid, priority, version, alertRules, subjectType, ownerID, envVars);

    result.mStoragePath    = "";
    result.mStatePath      = "";
    result.mManifestDigest = "";
    result.mMonitoringParams.Reset();

    return result;
}

aos::InstanceInfo CreateComponentRunInfo(const InstanceIdent& id, const std::string& imageID,
    const std::string& runtimeID, uint64_t priority, const std::string& version = "",
    SubjectTypeEnum subjectType = SubjectTypeEnum::eGroup)
{
    aos::InstanceInfo result;

    static_cast<InstanceIdent&>(result) = id;

    if (!version.empty()) {
        result.mVersion = version.c_str();
    }

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
    const oci::BalancingPolicy& balancingPolicy = oci::BalancingPolicyEnum::eEnabled,
    const oci::ServiceQuotas& quotas = {}, const oci::RequestedResources& requestedResources = {},
    const Optional<AlertRules>& alertRules = {}, const std::vector<std::string>& allowedConnections = {},
    const std::vector<oci::ResourceInfo>& resources = {})
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
        config.mResources.PushBack(resource);
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

Config CreateConfig()
{
    Config cfg;

    cfg.mNodesConnectionTimeout     = 1 * Time::cMinutes;
    cfg.mInstanceTTL                = 1 * Time::cSeconds;
    cfg.mCheckOverrideEnvVarsPeriod = 1 * Time::cMinutes;

    return cfg;
}

EnvVar CreateEnvVar(const std::string& name, const std::string& value)
{
    EnvVar var;

    var.mName  = name.c_str();
    var.mValue = value.c_str();

    return var;
}

EnvVarsInstanceStatus CreateEnvVarsInstanceStatus(const InstanceIdent& ident, const std::vector<EnvVarStatus>& statuses)
{
    EnvVarsInstanceStatus result;

    static_cast<InstanceIdent&>(result) = ident;

    for (const auto& status : statuses) {
        result.mStatuses.PushBack(status);
    }

    return result;
}

OverrideEnvVarsStatuses CreateOverrideEnvVarsStatuses(const std::vector<EnvVarsInstanceStatus>& statuses)
{
    OverrideEnvVarsStatuses result;

    for (const auto& status : statuses) {
        result.mStatuses.PushBack(status);
    }

    return result;
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CMLauncherTest, InstancesWithInvalidImageAreRemovedOnStart)
{
    // Store instance with non-empty manifest digest so image validity check is executed.

    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1), "sha256:invalid", "runc", "",
                        InstanceStateEnum::eActive, 0, 0, Time::Now(), "1.0.0"))
                    .IsNone());

    mInstanceRunner.Init(mLauncher);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    StaticArray<InstanceInfo, cMaxNumInstances> instances;

    ASSERT_TRUE(mStorage.LoadActiveInstances(instances).IsNone());
    EXPECT_EQ(instances.Size(), 0);

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, InstancesWithOutdatedTTLRemovedOnStart)
{
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
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    ASSERT_EQ(mStorageState.GetRemovedInstances().size(), 1);
    ASSERT_EQ(mStorageState.GetRemovedInstances()[0], CreateInstanceIdent(cService1));

    StaticArray<InstanceInfo, cMaxNumInstances> instances;

    ASSERT_TRUE(mStorage.LoadActiveInstances(instances).IsNone());
    ASSERT_EQ(instances.Size(), 1);
    ASSERT_EQ(instances[0].mInstanceIdent, CreateInstanceIdent(cService2));

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, CacheInstances)
{
    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mMonitoringProvider.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)},
        {CreateResource("resource1", 2), CreateResource("resource3", 2)});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRunc)},
        {CreateResource("resource1", 2), CreateResource("resource2", 2)});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    auto nodeInfoRemoteSM2 = CreateNodeInfo(cNodeIDRemoteSM2, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM2, nodeInfoRemoteSM2);

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2}) {
        auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
        CreateNodeMonitoring(*nodeMonitoring, nodeID, 0.0);
        mMonitoringProvider.SetAverageMonitoring(nodeID, *nodeMonitoring);
    }

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
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2}) {
        mInstanceRunner.SendInitialStatuses(nodeID);
    }

    // Run instances 1
    auto runRequest1 = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();

    runRequest1->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1, "", {}, UpdateItemTypeEnum::eService));
    runRequest1->PushBack(CreateRunRequest(cService2, cSubject1, 50, 1, "", {}, UpdateItemTypeEnum::eService));
    runRequest1->PushBack(CreateRunRequest(cService3, cSubject1, 50, 1, "", {}, UpdateItemTypeEnum::eService));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    ASSERT_TRUE(mLauncher.RunInstances(*runRequest1, *runStatuses).IsNone());

    auto instances = std::make_unique<StaticArray<InstanceInfo, cMaxNumInstances>>();

    EXPECT_TRUE(mStorage.LoadActiveInstances(*instances).IsNone());
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

    EXPECT_TRUE(mStorage.LoadActiveInstances(*instances).IsNone());
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
    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    // Create node info
    mNodeInfoProvider.Init();
    mMonitoringProvider.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)},
        {CreateResource("resource1", 2), CreateResource("resource3", 2)});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRootfs, 1)},
        {CreateResource("resource1", 2), CreateResource("resource2", 2)});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1}) {
        auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
        CreateNodeMonitoring(*nodeMonitoring, nodeID, 0.0);
        mMonitoringProvider.SetAverageMonitoring(nodeID, *nodeMonitoring);
    }

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
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1}) {
        mInstanceRunner.SendInitialStatuses(nodeID);
    }

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
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 50);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM1], cNodeIDRemoteSM1, 100);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRemoteSM2], cNodeIDRemoteSM2, 100);
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDRunxSM], cNodeIDRunxSM, 0);

    // Item configs
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, {}, {}, CreateRequestedResources(0, 0, 100, 0));
    CreateItemConfig(testData->mItemConfigs[cService2], {cRunnerRunc}, {}, {}, CreateRequestedResources(0, 0, 100, 0));
    CreateItemConfig(testData->mItemConfigs[cService3], {cRunnerRunx}, {}, {}, CreateRequestedResources(0, 0, 100, 0));

    // Desired instances
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService1, cSubject1, 100, 2, "", {}, UpdateItemTypeEnum::eService));
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService2, cSubject1, 50, 2, "", {}, UpdateItemTypeEnum::eService));
    testData->mRunRequests.PushBack(CreateRunRequest(cService3, cSubject1, 0, 2, "", {}, UpdateItemTypeEnum::eService));

    // Expected run requests
    std::vector<aos::InstanceInfo> remoteSM1Requests;
    remoteSM1Requests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, 100));
    remoteSM1Requests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5001, 50));

    std::vector<aos::InstanceInfo> remoteSM2Requests;
    remoteSM2Requests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, 5000, 100));
    remoteSM2Requests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService2, cSubject1, 1), cImageID1, cRunnerRunc, 5003, 5001, 50));

    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances   = std::vector<aos::InstanceInfo>();
    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = remoteSM1Requests;
    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = remoteSM2Requests;

    std::vector<aos::InstanceInfo> runxSMRequests;
    runxSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunx, 5004, 5002, 0));
    runxSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 1), cImageID1, cRunnerRunx, 5005, 5002, 0));
    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances = runxSMRequests;

    // Expected run status
    auto digest1 = BuildManifestDigest(cService1, cImageID1);
    auto digest2 = BuildManifestDigest(cService2, cImageID1);
    auto digest3 = BuildManifestDigest(cService3, cImageID1);

    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 1),
        cNodeIDRemoteSM2, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 0),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 1),
        cNodeIDRemoteSM2, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));
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
        CreateRunRequest(cService3, cSubject1, 0, 2, "", {"label3"}, UpdateItemTypeEnum::eService));
    // Expected run requests
    std::vector<aos::InstanceInfo> localSMRequests;
    localSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5001, 50));
    localSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService2, cSubject1, 1), cImageID1, cRunnerRunc, 5003, 5001, 50));
    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    std::vector<aos::InstanceInfo> remoteSM1Requests;
    remoteSM1Requests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, 100));
    remoteSM1Requests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, 5000, 100));
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
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, {}, {}, {}, {}, {},
        {{"resource1", "rw"}, {"resource2", "rw"}});
    CreateItemConfig(testData->mItemConfigs[cService2], {cRunnerRunc}, {}, {}, {}, {}, {}, {{"resource1", "rw"}});
    CreateItemConfig(testData->mItemConfigs[cService3], {cRunnerRunc}, {}, {}, {}, {}, {}, {{"resource3", "rw"}});
    // Desired instances
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService1, cSubject1, 100, 2, "", {}, UpdateItemTypeEnum::eService));
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService2, cSubject1, 50, 2, "", {}, UpdateItemTypeEnum::eService));
    testData->mRunRequests.PushBack(CreateRunRequest(cService3, cSubject1, 0, 2, "", {}, UpdateItemTypeEnum::eService));

    // Expected run requests
    std::vector<aos::InstanceInfo> localSMRequests;
    std::vector<aos::InstanceInfo> remoteSMRequests;

    localSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5001, 50));
    localSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService2, cSubject1, 1), cImageID1, cRunnerRunc, 5003, 5001, 50));
    localSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5004, 5002, 0));
    localSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 1), cImageID1, cRunnerRunc, 5005, 5002, 0));
    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    remoteSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, 100));
    remoteSMRequests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, 5000, 100));
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
    auto& localSMRequests = testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances;
    auto  digest1         = BuildManifestDigest(cService1, cImageID1);

    for (size_t i = 0; i < 3; ++i) {
        localSMRequests.push_back(CreateServiceRunInfo(
            CreateInstanceIdent(cService1, cSubject1, i), cImageID1, cRunnerRunc, 5000 + i, 5000, 100));
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
    auto& stateLocalRequests = testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances;
    auto  digest1            = BuildManifestDigest(cService1, cImageID1);

    for (size_t i = 0; i < 3; ++i) {
        stateLocalRequests.push_back(CreateServiceRunInfo(
            CreateInstanceIdent(cService1, cSubject1, i), cImageID1, cRunnerRunc, 5000 + i, 5000, 100));
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
    auto& cpuLocalRequests = testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances;
    auto  digest1          = BuildManifestDigest(cService1, cImageID1);

    // Instances 0, 1, 2 on localSM
    for (size_t i = 0; i < 3; ++i) {
        cpuLocalRequests.push_back(CreateServiceRunInfo(
            CreateInstanceIdent(cService1, cSubject1, i), cImageID1, cRunnerRunc, 5000 + i, 5000, 100));
        testData->mExpectedRunStatus.PushBack(
            CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, static_cast<uint64_t>(i)), cNodeIDLocalSM,
                cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    }

    // Instance 3 on remoteSM1
    auto& cpuRemoteSM1Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances;
    cpuRemoteSM1Requests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 3), cImageID1, cRunnerRunc, 5003, 5000, 100));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 3),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));

    // Instance 4 on remoteSM2
    auto& cpuRemoteSM2Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances;
    cpuRemoteSM2Requests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 4), cImageID1, cRunnerRunc, 5004, 5000, 100));
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
    auto& ramLocalRequests = testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances;
    auto  digest1          = BuildManifestDigest(cService1, cImageID1);

    // Instances 0, 1, 2 on localSM
    for (size_t i = 0; i < 3; ++i) {
        ramLocalRequests.push_back(CreateServiceRunInfo(
            CreateInstanceIdent(cService1, cSubject1, i), cImageID1, cRunnerRunc, 5000 + i, 5000, 100));
        testData->mExpectedRunStatus.PushBack(
            CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, static_cast<uint64_t>(i)), cNodeIDLocalSM,
                cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    }

    // Instance 3 on remoteSM1
    auto& ramRemoteSM1Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances;
    ramRemoteSM1Requests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 3), cImageID1, cRunnerRunc, 5003, 5000, 100));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 3),
        cNodeIDRemoteSM1, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));

    // Instance 4 on remoteSM2
    auto& ramRemoteSM2Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances;
    ramRemoteSM2Requests.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 4), cImageID1, cRunnerRunc, 5004, 5000, 100));
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
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled,
        CreateServiceQuotas(0, 0, 1000, 0));
    CreateItemConfig(testData->mItemConfigs[cService2], {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled,
        CreateServiceQuotas(0, 0, 1000, 0));
    CreateItemConfig(testData->mItemConfigs[cService3], {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled,
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
    rebalancingLocalRequests.mStartInstances.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, 100));
    rebalancingLocalRequests.mStopInstances.push_back(
        CreateAosStopInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cRunnerRunc));

    // remoteSM1: starts service2, stops service3 (which was initially scheduled there)
    auto& rebalancingRemoteSM1Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM1];
    rebalancingRemoteSM1Requests.mStartInstances.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5001, 5001, 50));
    rebalancingRemoteSM1Requests.mStopInstances.push_back(
        CreateAosStopInstanceInfo(CreateInstanceIdent(cService3, cSubject1, 0), cRunnerRunc));

    // remoteSM2: starts service3, no stops
    auto& rebalancingRemoteSM2Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM2];
    rebalancingRemoteSM2Requests.mStartInstances.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5002, 50));
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
    CreateItemConfig(testData->mItemConfigs[cService1], {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled,
        CreateServiceQuotas(0, 0, 1000, 0), {}, {});
    CreateItemConfig(testData->mItemConfigs[cService2], {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled,
        CreateServiceQuotas(0, 0, 1000, 0));
    CreateItemConfig(testData->mItemConfigs[cService3], {cRunnerRunc}, oci::BalancingPolicyEnum::eDisabled,
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
    policyLocalRequests.mStartInstances.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, 100));
    policyLocalRequests.mStopInstances.push_back(
        CreateAosStopInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cRunnerRunc));

    // remoteSM1: starts service3 (stays there, no stops since service3 has BalancingDisabled)
    auto& policyRemoteSM1Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM1];
    policyRemoteSM1Requests.mStartInstances.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5002, 50));

    // remoteSM2: starts service2, no stops
    auto& policyRemoteSM2Requests = testData->mExpectedRunRequests[cNodeIDRemoteSM2];
    policyRemoteSM2Requests.mStartInstances.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5001, 5001, 50));
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

        for (const auto& nodeID : nodeIDs) {
            auto monitoringIt = testItem.mMonitoring.find(nodeID);
            if (monitoringIt != testItem.mMonitoring.end()) {
                mMonitoringProvider.SetAverageMonitoring(nodeID, monitoringIt->second);
            } else {
                auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
                CreateNodeMonitoring(*nodeMonitoring, nodeID, 0.0);
                mMonitoringProvider.SetAverageMonitoring(nodeID, *nodeMonitoring);
            }
        }

        // Init launcher
        ASSERT_TRUE(mLauncher
                        .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                            mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                            ValidateGID, ValidateUID, mStorage, mSender)
                        .IsNone());

        InstanceStatusListenerStub instanceStatusListener;
        mLauncher.SubscribeListener(instanceStatusListener);

        ASSERT_TRUE(mLauncher.Start().IsNone());

        for (const auto& nodeID : nodeIDs) {
            mInstanceRunner.SendInitialStatuses(nodeID);
        }

        // Run instances
        auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
        ASSERT_TRUE(mLauncher.RunInstances(testItem.mRunRequests, *runStatuses).IsNone());

        ASSERT_EQ(*runStatuses, testItem.mExpectedRunStatus);

        // Rebalance
        if (testItem.mRebalancing) {
            for (const auto& [nodeID, monitoring] : testItem.mMonitoring) {
                mMonitoringProvider.SetAverageMonitoring(nodeID.c_str(), monitoring);
            }

            mAlertsProvider.TriggerSystemQuotaAlert();

            // Wait for rebalancing to send the expected run requests
            using namespace std::chrono_literals;
            ASSERT_TRUE(mInstanceRunner.WaitForRunRequests(testItem.mExpectedRunRequests, 2000ms));
        }

        ASSERT_TRUE(mLauncher.Stop().IsNone());
        mLauncher.UnsubscribeListener(instanceStatusListener);

        // Check sent run requests
        ASSERT_EQ(mInstanceRunner.GetRunRequests(), testItem.mExpectedRunRequests);
    }
}

TEST_F(CMLauncherTest, PlatformFiltering)
{
    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
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

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2}) {
        auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
        CreateNodeMonitoring(*nodeMonitoring, nodeID, 0.0);
        mMonitoringProvider.SetAverageMonitoring(nodeID, *nodeMonitoring);
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
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2}) {
        mInstanceRunner.SendInitialStatuses(nodeID);
    }

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
        {CreateServiceRunInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5002, 5002, 25)}};

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

    ASSERT_EQ(*runStatuses, *expectedRunStatus);
}

TEST_F(CMLauncherTest, ResendInstancesOnMismatchedNodeStatus)
{
    using namespace std::chrono_literals;

    // Initialize stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

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
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

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

    // Initialize stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    // Item config
    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    // 1) Run a single instance with a single subject.
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    // Wait until we receive notification after run instances.
    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(2, 2000ms));

    // 2) Change subjects (remove all of them).
    ASSERT_TRUE(mIdentProvider.SetSubjects({}).IsNone());

    // Expect to receive 0 statuses after subjects update.
    std::this_thread::sleep_for(300ms);

    StaticArray<InstanceStatus, 1> statuses;

    ASSERT_TRUE(mLauncher.GetInstancesStatuses(statuses).IsNone());
    ASSERT_TRUE(statuses.IsEmpty());

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, TestSentInstanceInfo)
{
    const std::string version = "1.2.3";
    const std::string ownerID = "owner123";

    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mMonitoringProvider.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    // Set up item config with alert rules and quotas
    auto alertRules = CreateAlertRules(75.0, 85.0);
    auto itemConfig = std::make_unique<oci::ItemConfig>();

    CreateItemConfig(*itemConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled,
        CreateServiceQuotas(500, 300, 0, 0), CreateRequestedResources(100, 50, 0, 0), alertRules);

    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig(), version);

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

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
        cRunnerRunc, 5000, 5000, 100, version, alertRules, SubjectTypeEnum::eUser, ownerID);

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests = {
        {cNodeIDLocalSM, {{}, {expectedInstanceInfo}}},
    };
    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);

    // Verify stored instance
    auto storedInstances = std::make_unique<StaticArray<InstanceInfo, cMaxNumInstances>>();
    ASSERT_TRUE(mStorage.LoadActiveInstances(*storedInstances).IsNone());

    ASSERT_EQ(storedInstances->Size(), 1);
    const auto& storedInstance = (*storedInstances)[0];

    auto expectedStoredInstance = CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0),
        BuildManifestDigest(cService1, cImageID1), cRunnerRunc, cNodeIDLocalSM, InstanceStateEnum::eActive, 5000, 5000,
        storedInstance.mTimestamp, version.c_str(), true, ownerID.c_str(), SubjectTypeEnum::eUser, 100);

    EXPECT_EQ((*storedInstances)[0], expectedStoredInstance);

    // Stop launcher
    mLauncher.UnsubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, PreinstalledComponents)
{
    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mMonitoringProvider.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

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
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

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

    // Initialize stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    // Item config
    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig(), "1.0.0");

    mInstanceRunner.Init(mLauncher);

    // Add two instances to storage
    auto manifestDigest = BuildManifestDigest(cService1, cImageID1);
    auto instance1
        = CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), manifestDigest, cRunnerRunc, cNodeIDLocalSM,
            InstanceStateEnum::eActive, 5000, 0, Time::Now(), "1.0.0", false, "", SubjectTypeEnum::eGroup, 100);

    auto instance2
        = CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), manifestDigest, cRunnerRunc, cNodeIDLocalSM,
            InstanceStateEnum::eActive, 5001, 0, Time::Now(), "1.0.0", false, "", SubjectTypeEnum::eGroup, 100);

    ASSERT_TRUE(mStorage.AddInstance(instance1).IsNone());
    ASSERT_TRUE(mStorage.AddInstance(instance2).IsNone());

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

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

TEST_F(CMLauncherTest, OverrideEnvVars)
{
    using namespace std::chrono_literals;

    // Initialize stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    // Service config
    oci::ItemConfig itemConfig;
    CreateItemConfig(itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, itemConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Start().IsNone());

    // Mark node as connected so RunInstances doesn't hang in WaitAllNodesConnected
    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    // 1) Run a single instance
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    // Wait until we have at least some statuses recorded
    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(1, 2s));

    // 2) Change override env vars with different TTLs
    EnvVarsInstanceInfo envVarsInfo;
    EnvVarInfo          envVar1, envVar2, envVar3;

    envVarsInfo.mItemID.EmplaceValue(cService1);

    // Env var 1: with expired TTL
    static_cast<EnvVar&>(envVar1) = CreateEnvVar("OVERRIDE_VAR1", "override_value1");
    envVar1.mTTL.SetValue(Time::Now().Add(-1 * Time::cHours));
    envVarsInfo.mVariables.PushBack(envVar1);

    // Env var 2: with non-expired TTL
    static_cast<EnvVar&>(envVar2) = CreateEnvVar("OVERRIDE_VAR2", "override_value2");
    envVar2.mTTL.SetValue(Time::Now().Add(1 * Time::cHours));
    envVarsInfo.mVariables.PushBack(envVar2);

    // Env var 3: without TTL
    static_cast<EnvVar&>(envVar3) = CreateEnvVar("OVERRIDE_VAR3", "override_value3");
    envVarsInfo.mVariables.PushBack(envVar3);

    OverrideEnvVarsRequest overrideRequest;

    overrideRequest.mItems.PushBack(envVarsInfo);

    ASSERT_TRUE(mLauncher.OverrideEnvVars(overrideRequest).IsNone());

    // 3) Wait for resend finish
    // Expect one more notification caused by resend after override env vars update
    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(2, 2s));

    // 4) Check result - non-expired and no-TTL env vars are sent to instance runner
    // (envVar1 with expired TTL should be filtered out)
    auto expectedEnvVarsList
        = {CreateEnvVar("OVERRIDE_VAR2", "override_value2"), CreateEnvVar("OVERRIDE_VAR3", "override_value3")};
    auto stopInstance
        = CreateRunInfoForStoppedService(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 0, 0, 0);
    auto startInstance = CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc,
        5000, 5000, 50, "", {}, SubjectTypeEnum::eGroup, "", tests::utils::ConvertToArray(expectedEnvVarsList));

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests
        = {{cNodeIDLocalSM, {{stopInstance}, {startInstance}}}};

    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);

    // 5) Check override env vars statuses are reported back to the sender.
    ASSERT_TRUE(mSender.WaitForSendCount(1, 2s));

    auto expectedInstanceStatus = CreateEnvVarsInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0),
        {EnvVarStatus {"OVERRIDE_VAR2", ErrorEnum::eNone}, EnvVarStatus {"OVERRIDE_VAR3", ErrorEnum::eNone}});

    EXPECT_EQ(mSender.GetOverrideEnvVarsStatuses(), CreateOverrideEnvVarsStatuses({expectedInstanceStatus}));

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, MultiNodeInstance)
{
    // Initialize all stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mInstanceRunner.Init(mLauncher);
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();

    // Create node info
    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    auto nodeInfoRemoteSM2 = CreateNodeInfo(cNodeIDRemoteSM2, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM2, nodeInfoRemoteSM2);

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2}) {
        auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
        CreateNodeMonitoring(*nodeMonitoring, nodeID, 0.0);
        mMonitoringProvider.SetAverageMonitoring(nodeID, *nodeMonitoring);
    }

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2}) {
        auto nodeConfig = std::make_unique<NodeConfig>();

        CreateNodeConfig(*nodeConfig, nodeID);
        mResourceManager.SetNodeConfig(nodeID, cNodeTypeVM, *nodeConfig);
    }

    // Create component config
    auto componentConfig = std::make_unique<oci::ItemConfig>();

    CreateItemConfig(*componentConfig, {cRunnerRunc});
    AddItem(cComponent1, cImageID1, *componentConfig, CreateImageConfig());

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2}) {
        mInstanceRunner.SendInitialStatuses(nodeID);
    }

    auto instanceStatusListener = std::make_unique<InstanceStatusListenerStub>();

    mLauncher.SubscribeListener(*instanceStatusListener);

    // Run component instance with numInstances = 0 (should create multi-node instance)
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();

    runRequest->PushBack(CreateRunRequest(cComponent1, cSubject1, 50, 0, "", {}, UpdateItemTypeEnum::eComponent));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    // Check expectations
    auto manifestDigest    = BuildManifestDigest(cComponent1, cImageID1);
    auto expectedRunStatus = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    auto instanceIdent0 = CreateInstanceIdent(cComponent1, cSubject1, 0, UpdateItemTypeEnum::eComponent);
    auto instanceIdent1 = CreateInstanceIdent(cComponent1, cSubject1, 1, UpdateItemTypeEnum::eComponent);
    auto instanceIdent2 = CreateInstanceIdent(cComponent1, cSubject1, 2, UpdateItemTypeEnum::eComponent);

    expectedRunStatus->PushBack(CreateInstanceStatus(instanceIdent0, cNodeIDLocalSM, cRunnerRunc,
        aos::InstanceStateEnum::eActivating, ErrorEnum::eNone, "", false, manifestDigest.CStr()));
    expectedRunStatus->PushBack(CreateInstanceStatus(instanceIdent1, cNodeIDRemoteSM1, cRunnerRunc,
        aos::InstanceStateEnum::eActivating, ErrorEnum::eNone, "", false, manifestDigest.CStr()));
    expectedRunStatus->PushBack(CreateInstanceStatus(instanceIdent2, cNodeIDRemoteSM2, cRunnerRunc,
        aos::InstanceStateEnum::eActivating, ErrorEnum::eNone, "", false, manifestDigest.CStr()));

    EXPECT_EQ(*runStatuses, *expectedRunStatus);

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests;

    expectedRunRequests[cNodeIDLocalSM]   = {{}, {CreateComponentRunInfo(instanceIdent0, cImageID1, cRunnerRunc, 50)}};
    expectedRunRequests[cNodeIDRemoteSM1] = {{}, {CreateComponentRunInfo(instanceIdent1, cImageID1, cRunnerRunc, 50)}};
    expectedRunRequests[cNodeIDRemoteSM2] = {{}, {CreateComponentRunInfo(instanceIdent2, cImageID1, cRunnerRunc, 50)}};

    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);

    mLauncher.UnsubscribeListener(*instanceStatusListener);
    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, RebalancingWithStoredNotScheduledInstances)
{
    using namespace std::chrono_literals;

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

    mImageStore.Init();
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mAlertsProvider.Init();
    mResourceManager.Init();

    // Prepare stored instances: one scheduled and one not scheduled.
    auto digest1 = BuildManifestDigest(cService1, cImageID1);
    auto digest2 = BuildManifestDigest(cService2, cImageID1);

    auto storedInstances = std::make_unique<StaticArray<InstanceInfo, cMaxNumInstances>>();
    storedInstances->PushBack(
        CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), digest1, cRunnerRunc, cNodeIDLocalSM,
            InstanceStateEnum::eActive, 5000, 5000, Time::Now(), "", false, "", SubjectTypeEnum::eGroup, 100));
    storedInstances->PushBack(CreateInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), digest2, "", "",
        InstanceStateEnum::eActive, 5001, 5001, Time::Now(), "", false, "", SubjectTypeEnum::eGroup, 50));

    auto storedRunRequests = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    storedRunRequests->PushBack(CreateRunRequest(cService1, cSubject1, 100, 1));
    storedRunRequests->PushBack(CreateRunRequest(cService2, cSubject1, 50, 1));
    mStorage.Init(*storedInstances, *storedRunRequests);

    // Configure services.
    auto itemConfigService1 = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(
        *itemConfigService1, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled, CreateServiceQuotas(0, 0, 1000, 0));
    AddItem(cService1, cImageID1, *itemConfigService1, CreateImageConfig());

    auto itemConfigService2 = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(
        *itemConfigService2, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled, CreateServiceQuotas(0, 0, 1000, 0));
    AddItem(cService2, cImageID1, *itemConfigService2, CreateImageConfig());

    // Configure nodes and balancing policy.
    auto alertRules = CreateAlertRules(75.0, 85.0);

    NodeConfig localNodeConfig;
    CreateNodeConfig(localNodeConfig, cNodeIDLocalSM, 100, {}, {}, alertRules);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, localNodeConfig);

    NodeConfig remoteNodeConfig;
    CreateNodeConfig(remoteNodeConfig, cNodeIDRemoteSM1, 50, {}, {}, alertRules);
    mResourceManager.SetNodeConfig(cNodeIDRemoteSM1, cNodeTypeVM, remoteNodeConfig);

    // Configure monitoring.
    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    monitoring::NodeMonitoringData localMonitoring;
    CreateNodeMonitoring(localMonitoring, cNodeIDLocalSM, 1000,
        {CreateInstanceMonitoring(CreateInstanceIdent(cService1, cSubject1, 0), 500),
            CreateInstanceMonitoring(CreateInstanceIdent(cService2, cSubject1, 0), 500)});
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, localMonitoring);

    auto remoteMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*remoteMonitoring, cNodeIDRemoteSM1, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDRemoteSM1, *remoteMonitoring);

    // Init launcher.
    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);
    mInstanceRunner.SendInitialStatuses(cNodeIDRemoteSM1);

    // Wait until initial statuses are processed and rebalance is completed.
    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(3, 2s));

    ASSERT_TRUE(mLauncher.Stop().IsNone());
    mLauncher.UnsubscribeListener(instanceStatusListener);

    // Check sent run requests.
    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests;

    expectedRunRequests[cNodeIDLocalSM]   = {{},
          {CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, 100)}};
    expectedRunRequests[cNodeIDRemoteSM1] = {{},
        {CreateServiceRunInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5001, 5001, 50)}};

    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);
}

TEST_F(CMLauncherTest, CpuAlertRebalancingMovesLowerPriorityService)
{
    using namespace std::chrono_literals;

    // 1) Two services with different priorities (service1 = higher, service2 = lower)
    //    that request CPU as the required resource.
    // 2) Two nodes with different priorities (localSM = higher, remoteSM1 = lower).
    //    Each node has a node-level CPU alert rule.
    // 3) Initially both services must be scheduled on the higher-priority node (localSM).
    // 4) Monitoring later reports CPU usage that breaches the alert rule on localSM
    //    and a system quota alert is delivered to the launcher.
    // 5) After rebalancing, the lower-priority service (service2) must migrate to
    //    the lower-priority node (remoteSM1), while the higher-priority service
    //    (service1) stays on the higher-priority node (localSM).

    // Initialize all stubs.
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    mImageStore.Init();
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mAlertsProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    // Node configs: priorities differ; CPU-only alert rule on both nodes.
    auto alertRules = CreateAlertRules(75.0, 0.0);

    NodeConfig localNodeConfig;
    CreateNodeConfig(localNodeConfig, cNodeIDLocalSM, 100, {}, {}, alertRules);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, localNodeConfig);

    NodeConfig remoteNodeConfig;
    CreateNodeConfig(remoteNodeConfig, cNodeIDRemoteSM1, 50, {}, {}, alertRules);
    mResourceManager.SetNodeConfig(cNodeIDRemoteSM1, cNodeTypeVM, remoteNodeConfig);

    // Service configs: CPU is the required resource.
    auto itemConfigService1 = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(
        *itemConfigService1, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled, CreateServiceQuotas(0, 0, 1000, 0));
    AddItem(cService1, cImageID1, *itemConfigService1, CreateImageConfig());

    auto itemConfigService2 = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(
        *itemConfigService2, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled, CreateServiceQuotas(0, 0, 1000, 0));
    AddItem(cService2, cImageID1, *itemConfigService2, CreateImageConfig());

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    // Initial monitoring: no load on either node.
    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1}) {
        auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
        CreateNodeMonitoring(*nodeMonitoring, nodeID, 0.0);
        mMonitoringProvider.SetAverageMonitoring(nodeID, *nodeMonitoring);
    }

    // Init launcher.
    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Start().IsNone());

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1}) {
        mInstanceRunner.SendInitialStatuses(nodeID);
    }

    // Run two services with different priorities.
    auto runRequests = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequests->PushBack(CreateRunRequest(cService1, cSubject1, 100, 1));
    runRequests->PushBack(CreateRunRequest(cService2, cSubject1, 50, 1));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequests, *runStatuses).IsNone());

    // Both services are expected to be placed on the higher-priority node (localSM) initially,
    // because at this point monitoring reports no load and both services fit there.
    auto digest1 = BuildManifestDigest(cService1, cImageID1);
    auto digest2 = BuildManifestDigest(cService2, cImageID1);

    auto expectedInitialStatus = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    expectedInitialStatus->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), cNodeIDLocalSM,
        cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest1.CStr()));
    expectedInitialStatus->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 0), cNodeIDLocalSM,
        cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest2.CStr()));

    ASSERT_EQ(*runStatuses, *expectedInitialStatus);

    // Simulate CPU usage on localSM that exceeds the alert threshold.
    monitoring::NodeMonitoringData localMonitoring;
    CreateNodeMonitoring(localMonitoring, cNodeIDLocalSM, 1000,
        {CreateInstanceMonitoring(CreateInstanceIdent(cService1, cSubject1, 0), 500),
            CreateInstanceMonitoring(CreateInstanceIdent(cService2, cSubject1, 0), 500)});
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, localMonitoring);

    // Trigger system quota alert that will kick off rebalancing.
    size_t currentNotifyCount = instanceStatusListener.GetNotifyCount();
    mAlertsProvider.TriggerSystemQuotaAlert();

    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(currentNotifyCount + 2, 2000ms));

    ASSERT_TRUE(mLauncher.Stop().IsNone());
    mLauncher.UnsubscribeListener(instanceStatusListener);

    // After rebalancing:
    //   - service1 (higher priority) stays on the higher-priority node (localSM).
    //   - service2 (lower priority) migrates to the lower-priority node (remoteSM1)
    //     because localSM available CPU (reduced by the alert min threshold) is no
    //     longer enough to fit service2.
    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests;
    expectedRunRequests[cNodeIDLocalSM].mStartInstances.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, 100));
    expectedRunRequests[cNodeIDLocalSM].mStopInstances.push_back(
        CreateAosStopInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cRunnerRunc));
    expectedRunRequests[cNodeIDRemoteSM1].mStartInstances.push_back(
        CreateServiceRunInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5001, 5001, 50));

    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);
}

TEST_F(CMLauncherTest, ServiceUpdate)
{
    using namespace std::chrono_literals;

    // Initialize stubs
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    // Service config: register two image versions for the same service
    oci::ItemConfig itemConfig;
    CreateItemConfig(itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, itemConfig, CreateImageConfig(), "1.0.0");
    AddItem(cService1, cImageID1, itemConfig, CreateImageConfig(), "1.0.1");

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Start().IsNone());

    // Mark node as connected so RunInstances doesn't hang in WaitAllNodesConnected
    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    // 1) Run service with two instances at version 1.0.0
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 2, "", {}, UpdateItemTypeEnum::eService, "1.0.0"));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    ASSERT_TRUE(instanceStatusListener.WaitForNotifyCount(1, 2s));

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedAfterV100 = {{cNodeIDLocalSM,
        {{},
            {CreateServiceRunInfo(
                 CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, 50, "1.0.0"),
                CreateServiceRunInfo(
                    CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, 5000, 50, "1.0.0")}}}};

    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedAfterV100);

    // 2) Run the same service with two instances at version 1.0.1
    runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 2, "", {}, UpdateItemTypeEnum::eService, "1.0.1"));

    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    EXPECT_TRUE(instanceStatusListener.WaitForNotifyCount(2, 2s));

    // New version replaces the old one on the node: SM is told to stop 1.0.0 instances, then start 1.0.1.
    // Cached v1.0.0 instances still hold UID pool references; v1.0.1 reuses the same UIDs per InstanceIdent.
    auto stopV100Inst0     = CreateAosStopInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), cRunnerRunc);
    stopV100Inst0.mVersion = "1.0.0";
    auto stopV100Inst1     = CreateAosStopInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), cRunnerRunc);
    stopV100Inst1.mVersion = "1.0.0";

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedAfterV101 = {{cNodeIDLocalSM,
        {{stopV100Inst0, stopV100Inst1},
            {CreateServiceRunInfo(
                 CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, 5000, 50, "1.0.1"),
                CreateServiceRunInfo(
                    CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, 5000, 50, "1.0.1")}}}};

    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedAfterV101);

    // Stop launcher
    mLauncher.UnsubscribeListener(instanceStatusListener);
    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, UnlimitedSharedResource)
{
    using namespace std::chrono_literals;

    // Initialize stubs.
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mInstanceStatusProvider.Init();
    mMonitoringProvider.Init();
    mAlertsProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    // One node with a single shared resource whose mSharedCount is 0 (= unlimited).
    const std::string cUnlimitedResource = "camera";
    auto              nodeInfoLocalSM    = CreateNodeInfo(
        cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {CreateResource(cUnlimitedResource, 0)});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    NodeConfig nodeConfig;
    CreateNodeConfig(nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    // One service that requires the shared resource.
    oci::ItemConfig itemConfig;
    CreateItemConfig(itemConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled, {}, {}, {}, {},
        {{cUnlimitedResource.c_str(), "rw"}});
    AddItem(cService1, cImageID1, itemConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    // Run a service with multiple instances.
    constexpr uint64_t cNumInstances = 3;

    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, cNumInstances));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    // Check every instance is active.
    auto digest = BuildManifestDigest(cService1, cImageID1);

    auto expectedRunStatus = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    for (uint64_t i = 0; i < cNumInstances; ++i) {
        expectedRunStatus->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, i), cNodeIDLocalSM,
            cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", false, digest.CStr()));
    }

    EXPECT_EQ(*runStatuses, *expectedRunStatus);

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, RunInstanceWithUnknownIndexDigest)
{
    constexpr auto cVersion = "1.0.0";

    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig(), cVersion);

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    // Run request refers to an image index which is not provided by the image store.
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1, "", {}, UpdateItemTypeEnum::eService, cVersion));
    (*runRequest)[0].mIndexDigest = "sha256:unknown";

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());
    ASSERT_TRUE(mLauncher.Stop().IsNone());

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests = {{cNodeIDLocalSM, {}}};
    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);

    auto expectedRunStatus = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    expectedRunStatus->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), "", "",
        aos::InstanceStateEnum::eFailed, ErrorEnum::eNotFound, cVersion));

    ASSERT_EQ(*runStatuses, *expectedRunStatus);
    EXPECT_TRUE((*runStatuses)[0].mError.Is(ErrorEnum::eNotFound));
}

TEST_F(CMLauncherTest, SchedulingErrorIsKeptAfterNodeStopStatus)
{
    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    const auto instanceIdent = CreateInstanceIdent(cService1, cSubject1, 0);

    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    ASSERT_EQ(runStatuses->Size(), 1);
    EXPECT_EQ((*runStatuses)[0].mState, aos::InstanceStateEnum::eActive);
    EXPECT_EQ((*runStatuses)[0].mNodeID, cNodeIDLocalSM);

    mImageStore.SetBlobError(
        imagemanager::ImageStoreStub::BuildIndexDigest(cService1, ""), Error(ErrorEnum::eIntegrity, EIO));

    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());

    ASSERT_EQ(runStatuses->Size(), 1);
    EXPECT_EQ(static_cast<const InstanceIdent&>((*runStatuses)[0]), instanceIdent);
    EXPECT_EQ((*runStatuses)[0].mState, aos::InstanceStateEnum::eFailed);
    EXPECT_TRUE((*runStatuses)[0].mError.Is(ErrorEnum::eIntegrity));
    EXPECT_TRUE((*runStatuses)[0].mNodeID.IsEmpty());

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests
        = {{cNodeIDLocalSM, {{CreateAosStopInstanceInfo(instanceIdent, cRunnerRunc)}, {}}}};
    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);

    const auto notifyCount = instanceStatusListener.GetNotifyCount();

    std::vector<InstanceStatus> nodeStatuses
        = {CreateInstanceStatus(instanceIdent, cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eInactive,
            ErrorEnum::eNone, "", false, BuildManifestDigest(cService1, cImageID1).CStr())};

    InstanceStatusReceiverItf& statusReceiver = mLauncher;

    ASSERT_TRUE(statusReceiver
                    .OnNodeInstancesStatusesReceived(
                        cNodeIDLocalSM, Array<InstanceStatus>(nodeStatuses.data(), nodeStatuses.size()))
                    .IsNone());

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.GetInstancesStatuses(*statuses).IsNone());

    ASSERT_EQ(statuses->Size(), 1);
    EXPECT_EQ(static_cast<const InstanceIdent&>((*statuses)[0]), instanceIdent);
    EXPECT_EQ((*statuses)[0].mState, aos::InstanceStateEnum::eFailed);
    EXPECT_TRUE((*statuses)[0].mError.Is(ErrorEnum::eIntegrity));
    EXPECT_TRUE((*statuses)[0].mNodeID.IsEmpty());

    EXPECT_EQ(instanceStatusListener.GetNotifyCount(), notifyCount);

    for (const auto& status : instanceStatusListener.GetLastStatuses()) {
        EXPECT_NE(status.mState, aos::InstanceStateEnum::eInactive);
    }

    mLauncher.UnsubscribeListener(instanceStatusListener);
    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, RunInstanceWithEmptyIndexDigest)
{
    constexpr auto cVersion = "1.0.0";

    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig(), cVersion);

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    // Run request has no image index digest.
    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1, "", {}, UpdateItemTypeEnum::eService, cVersion));
    (*runRequest)[0].mIndexDigest = "";

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());
    ASSERT_TRUE(mLauncher.Stop().IsNone());

    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests = {{cNodeIDLocalSM, {}}};
    EXPECT_EQ(mInstanceRunner.GetRunRequests(), expectedRunRequests);

    auto expectedRunStatus = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    expectedRunStatus->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), "", "",
        aos::InstanceStateEnum::eFailed, ErrorEnum::eInvalidArgument, cVersion));

    ASSERT_EQ(*runStatuses, *expectedRunStatus);
    EXPECT_TRUE((*runStatuses)[0].mError.Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CMLauncherTest, ReusedInstanceUpdatesIndexDigest)
{
    constexpr auto cVersion = "1.0.0";
    constexpr auto cUID     = 5000;

    mStorageState.Init();
    mStorageState.SetTotalStateSize(1024);
    mStorageState.SetTotalStorageSize(1024);

    mNodeInfoProvider.Init();
    mImageStore.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mStorage.Init();

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeConfig = std::make_unique<NodeConfig>();
    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();
    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    auto itemConfig = std::make_unique<oci::ItemConfig>();
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig(), cVersion);

    // Store instance with the same ident and version but with stale index digest.
    auto storedInfo         = CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), {}, cRunnerRunc, "",
                InstanceStateEnum::eActive, cUID, 0, Time::Now(), cVersion, true);
    storedInfo.mIndexDigest = "sha256:stale";

    ASSERT_TRUE(mStorage.AddInstance(storedInfo).IsNone());

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    auto runRequest = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    runRequest->PushBack(CreateRunRequest(cService1, cSubject1, 50, 1, "", {}, UpdateItemTypeEnum::eService, cVersion));

    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    ASSERT_TRUE(mLauncher.RunInstances(*runRequest, *runStatuses).IsNone());
    ASSERT_TRUE(mLauncher.Stop().IsNone());

    auto expectedRunStatus = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    expectedRunStatus->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), cNodeIDLocalSM,
        cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, cVersion, false,
        BuildManifestDigest(cService1, cImageID1).CStr()));

    ASSERT_EQ(*runStatuses, *expectedRunStatus);

    auto storedInstances = std::make_unique<StaticArray<InstanceInfo, cMaxNumInstances>>();
    ASSERT_TRUE(mStorage.LoadActiveInstances(*storedInstances).IsNone());

    ASSERT_EQ(storedInstances->Size(), 1);
    auto expectedIdent     = CreateInstanceIdent(cService1, cSubject1, 0);
    expectedIdent.mVersion = cVersion;

    EXPECT_EQ((*storedInstances)[0].mInstanceIdent, expectedIdent);
    EXPECT_EQ((*storedInstances)[0].mUID, cUID);
    EXPECT_EQ((*storedInstances)[0].mIndexDigest, imagemanager::ImageStoreStub::BuildIndexDigest(cService1, cVersion));
}

/***********************************************************************************************************************
 * Unit test helpers
 **********************************************************************************************************************/

class FaultyStorageStub : public StorageStub {
public:
    Error AddInstance(const InstanceInfo& info) override
    {
        return mAddErr.IsNone() ? StorageStub::AddInstance(info) : mAddErr;
    }

    Error UpdateInstance(const InstanceInfo& info) override
    {
        auto err = mUpdateErr.IsNone() ? StorageStub::UpdateInstance(info) : mUpdateErr;

        std::lock_guard lock {mMutex};

        ++mUpdateCount;
        mCondVar.notify_all();

        return err;
    }

    Error SaveOverrideEnvVars(const OverrideEnvVarsRequest& envVars) override
    {
        auto err = mSaveEnvVarsErr.IsNone() ? StorageStub::SaveOverrideEnvVars(envVars) : mSaveEnvVarsErr;

        std::lock_guard lock {mMutex};

        ++mSaveEnvVarsCount;
        mCondVar.notify_all();

        return err;
    }

    bool WaitForUpdateCount(size_t count)
    {
        std::unique_lock lock {mMutex};

        return mCondVar.wait_for(lock, std::chrono::seconds(5), [&]() { return mUpdateCount >= count; });
    }

    bool WaitForSaveEnvVarsCount(size_t count)
    {
        std::unique_lock lock {mMutex};

        return mCondVar.wait_for(lock, std::chrono::seconds(5), [&]() { return mSaveEnvVarsCount >= count; });
    }

    size_t GetSaveEnvVarsCount()
    {
        std::lock_guard lock {mMutex};

        return mSaveEnvVarsCount;
    }

    Error RemoveInstance(const InstanceIdent& instanceID) override
    {
        return mRemoveErr.IsNone() ? StorageStub::RemoveInstance(instanceID) : mRemoveErr;
    }

    Error LoadActiveInstances(Array<InstanceInfo>& instances) const override
    {
        return mLoadInstancesErr.IsNone() ? StorageStub::LoadActiveInstances(instances) : mLoadInstancesErr;
    }

    Error LoadOverrideEnvVars(OverrideEnvVarsRequest& envVars) const override
    {
        return mLoadEnvVarsErr.IsNone() ? StorageStub::LoadOverrideEnvVars(envVars) : mLoadEnvVarsErr;
    }

    Error LoadRunRequests(Array<RunInstanceRequest>& requests) const override
    {
        return mLoadRunRequestsErr.IsNone() ? StorageStub::LoadRunRequests(requests) : mLoadRunRequestsErr;
    }

    Error SaveRunRequests(const Array<RunInstanceRequest>& requests) override
    {
        return mSaveRunRequestsErr.IsNone() ? StorageStub::SaveRunRequests(requests) : mSaveRunRequestsErr;
    }

    Error mAddErr;
    Error mUpdateErr;
    Error mRemoveErr;
    Error mLoadInstancesErr;
    Error mLoadEnvVarsErr;
    Error mLoadRunRequestsErr;
    Error mSaveRunRequestsErr;
    Error mSaveEnvVarsErr;

private:
    std::mutex              mMutex;
    std::condition_variable mCondVar;
    size_t                  mUpdateCount {};
    size_t                  mSaveEnvVarsCount {};
};

class FailingAllocator : public AllocatorItf {
public:
    static constexpr size_t cAnySize = 0;

    void Fail(size_t size, size_t skip = 0, size_t count = 1)
    {
        std::lock_guard lock {mMutex};

        mFailSize  = size;
        mSkip      = skip;
        mFailCount = count;
    }

    void* Allocate(size_t size) override
    {
        std::lock_guard lock {mMutex};

        if (mFailCount != 0 && (mFailSize == cAnySize || mFailSize == size)) {
            if (mSkip != 0) {
                mSkip--;
            } else {
                mFailCount--;

                return nullptr;
            }
        }

        return std::malloc(size);
    }

    void Free(void* data) override { std::free(data); }

private:
    std::mutex mMutex;
    size_t     mFailSize {};
    size_t     mSkip {};
    size_t     mFailCount {};
};

class FaultyImageStoreStub : public imagemanager::ImageStoreStub {
public:
    Error LoadImageIndex(const String& path, oci::ImageIndex& index) override
    {
        return mIndexErr.IsNone() ? ImageStoreStub::LoadImageIndex(path, index) : mIndexErr;
    }

    Error LoadImageManifest(const String& path, oci::ImageManifest& manifest) override
    {
        if (!mManifestErr.IsNone()) {
            return mManifestErr;
        }

        auto err = ImageStoreStub::LoadImageManifest(path, manifest);

        if (mNoItemConfig) {
            manifest.mItemConfig.Reset();
        }

        return err;
    }

    Error LoadImageConfig(const String& path, oci::ImageConfig& imageConfig) override
    {
        return mImageConfigErr.IsNone() ? ImageStoreStub::LoadImageConfig(path, imageConfig) : mImageConfigErr;
    }

    Error LoadItemConfig(const String& path, oci::ItemConfig& itemConfig) override
    {
        return mItemConfigErr.IsNone() ? ImageStoreStub::LoadItemConfig(path, itemConfig) : mItemConfigErr;
    }

    Error mIndexErr;
    Error mManifestErr;
    Error mImageConfigErr;
    Error mItemConfigErr;
    bool  mNoItemConfig {};
};

class CountingMonitoringProvider : public MonitoringProviderStub {
public:
    Error GetAverageMonitoring(const String& nodeID, monitoring::NodeMonitoringData& monitoring) override
    {
        auto err = MonitoringProviderStub::GetAverageMonitoring(nodeID, monitoring);

        std::lock_guard lock {mMutex};

        ++mRequestCount;
        mCondVar.notify_all();

        return err;
    }

    bool WaitForRequestCount(size_t count)
    {
        std::unique_lock lock {mMutex};

        return mCondVar.wait_for(lock, std::chrono::seconds(5), [&]() { return mRequestCount >= count; });
    }

private:
    std::mutex              mMutex;
    std::condition_variable mCondVar;
    size_t                  mRequestCount {};
};

class FaultySenderStub : public SenderStub {
public:
    Error SendOverrideEnvsStatuses(const OverrideEnvVarsStatuses& statuses) override
    {
        if (!mSendErr.IsNone()) {
            return mSendErr;
        }

        ++mSendCount;

        return SenderStub::SendOverrideEnvsStatuses(statuses);
    }

    Error  mSendErr;
    size_t mSendCount {};
};

class FaultyStorageStateStub : public storagestate::StorageStateStub {
public:
    Error Setup(const InstanceIdent& instanceIdent, const storagestate::SetupParams& setupParams, String& storagePath,
        String& statePath) override
    {
        return mSetupErr.IsNone() ? StorageStateStub::Setup(instanceIdent, setupParams, storagePath, statePath)
                                  : mSetupErr;
    }

    Error Cleanup(const InstanceIdent& instanceIdent) override
    {
        return mCleanupErr.IsNone() ? StorageStateStub::Cleanup(instanceIdent) : mCleanupErr;
    }

    Error Remove(const InstanceIdent& instanceIdent) override
    {
        return mRemoveErr.IsNone() ? StorageStateStub::Remove(instanceIdent) : mRemoveErr;
    }

    RetWithError<size_t> GetTotalStateSize() const override
    {
        if (!mStateSizeErr.IsNone()) {
            return {0, mStateSizeErr};
        }

        return StorageStateStub::GetTotalStateSize();
    }

    RetWithError<size_t> GetTotalStorageSize() const override
    {
        if (!mStorageSizeErr.IsNone()) {
            return {0, mStorageSizeErr};
        }

        return StorageStateStub::GetTotalStorageSize();
    }

    bool IsSamePartition() const override { return mSamePartition; }

    Error mSetupErr;
    Error mCleanupErr;
    Error mRemoveErr;
    Error mStateSizeErr;
    Error mStorageSizeErr;
    bool  mSamePartition {true};
};

class InstanceRunnerMock : public InstanceRunnerItf {
public:
    MOCK_METHOD(Error, UpdateInstances,
        (const String& nodeID, const Array<aos::InstanceInfo>& stopInstances,
            const Array<aos::InstanceInfo>& startInstances),
        (override));
};

class AlertsProviderMock : public alerts::AlertsProviderItf {
public:
    MOCK_METHOD(
        Error, SubscribeListener, (const Array<AlertTag>& tags, alerts::AlertsListenerItf& listener), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (alerts::AlertsListenerItf & listener), (override));
};

class OverrideEnvVarsListenerStub : public OverrideEnvVarsListenerItf {
public:
    void OnOverrideEnvVarsChanged() override { }
};

class TestNode : public NodeItf {
public:
    explicit TestNode(const char* nodeID = cNodeIDLocalSM) { mConfig.mNodeID = nodeID; }

    void SetNeedBalancing(bool needBalancing) { mNeedBalancing = needBalancing; }
    void SetResourceRatios(const ResourceRatios& ratios) { mConfig.mResourceRatios.SetValue(ratios); }
    void SetReserveError(const Error& err) { mReserveErr = err; }

    Error ReserveResources(const InstanceIdent& instanceIdent, const String& runtimeID, size_t reqCPU, size_t reqRAM,
        const Array<oci::ResourceInfo>& reqResources) override
    {
        (void)instanceIdent;
        (void)runtimeID;
        (void)reqCPU;
        (void)reqRAM;
        (void)reqResources;

        return mReserveErr;
    }

    size_t GetAvailableResourceCount(const String& resourceName) const override
    {
        (void)resourceName;

        return 1;
    }

    const NodeConfig& GetConfig() const override { return mConfig; }

    bool NeedBalancing() const override { return mNeedBalancing; }

private:
    NodeConfig mConfig {};
    bool       mNeedBalancing {};
    Error      mReserveErr;
};

oci::IndexContentDescriptor CreateImageDescriptor(const std::string& itemID, const std::string& imageID = cImageID1)
{
    oci::IndexContentDescriptor descriptor;

    descriptor.mDigest = BuildManifestDigest(itemID, imageID);

    return descriptor;
}

ResourceRatios CreateResourceRatios(double cpu, double ram, double storage, double state)
{
    ResourceRatios ratios;

    ratios.mCPU.SetValue(cpu);
    ratios.mRAM.SetValue(ram);
    ratios.mStorage.SetValue(storage);
    ratios.mState.SetValue(state);

    return ratios;
}

/***********************************************************************************************************************
 * InstanceManager tests
 **********************************************************************************************************************/

class CMLauncherInstanceManagerTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        mStorageState.Init();
        mImageStore.Init();
        mStorage.Init();

        ASSERT_TRUE(mInstanceManager
                        ->Init(mAllocator, CreateConfig(), mImageStore, mStorageState, mImageStore, ValidateGID,
                            ValidateUID, mStorage)
                        .IsNone());
    }

    void TearDown() override { EXPECT_TRUE(mInstanceManager->Stop().IsNone()); }

    FailingAllocator                 mAllocator;
    imagemanager::ImageStoreStub     mImageStore;
    FaultyStorageStateStub           mStorageState;
    FaultyStorageStub                mStorage;
    std::unique_ptr<InstanceManager> mInstanceManager = std::make_unique<InstanceManager>();
};

TEST_F(CMLauncherInstanceManagerTest, StartFailsOnStorageStateError)
{
    mStorageState.mStateSizeErr = ErrorEnum::eFailed;

    EXPECT_TRUE(mInstanceManager->Start().Is(ErrorEnum::eFailed));

    mStorageState.mStateSizeErr   = ErrorEnum::eNone;
    mStorageState.mStorageSizeErr = ErrorEnum::eWrongState;
    mStorageState.mSamePartition  = false;

    EXPECT_TRUE(mInstanceManager->Start().Is(ErrorEnum::eWrongState));
}

TEST_F(CMLauncherInstanceManagerTest, StartFailsOnLoadInstancesError)
{
    mStorage.mLoadInstancesErr = ErrorEnum::eFailed;

    EXPECT_TRUE(mInstanceManager->Start().Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherInstanceManagerTest, StartFailsOnRemoveCachedInstanceWithInvalidImage)
{
    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1), "sha256:invalid", cRunnerRunc, "",
                        InstanceStateEnum::eCached, 0, 0, Time::Now()))
                    .IsNone());

    mStorageState.mRemoveErr = ErrorEnum::eFailed;

    EXPECT_TRUE(mInstanceManager->Start().Is(ErrorEnum::eFailed));
    EXPECT_TRUE(mInstanceManager->GetCachedInstances().IsEmpty());
}

TEST_F(CMLauncherInstanceManagerTest, StartFailsOnRemoveOutdatedInstance)
{
    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1), {}, cRunnerRunc, "",
                        InstanceStateEnum::eCached, 0, 0, Time::Now().Add(-25 * Time::cHours)))
                    .IsNone());

    mStorage.mRemoveErr = ErrorEnum::eFailed;

    EXPECT_TRUE(mInstanceManager->Start().Is(ErrorEnum::eFailed));
    EXPECT_TRUE(mInstanceManager->GetCachedInstances().IsEmpty());
}

TEST_F(CMLauncherInstanceManagerTest, InvalidStoredInstancesAreSkipped)
{
    ASSERT_TRUE(
        mStorage.AddInstance(CreateInstanceInfo(CreateInstanceIdent(cLayer1, "", 0, UpdateItemTypeEnum::eLayer)))
            .IsNone());

    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), {}, cRunnerRunc, "",
                        InstanceStateEnum::eActive, 7000))
                    .IsNone());
    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), {}, cRunnerRunc, "",
                        InstanceStateEnum::eActive, 7000))
                    .IsNone());

    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), {}, cRunnerRunc, "",
                        InstanceStateEnum::eActive, 0, 6000))
                    .IsNone());
    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService2, "subject2", 0), {}, cRunnerRunc, "",
                        InstanceStateEnum::eActive, 0, 6001))
                    .IsNone());

    mStorageState.mRemoveErr = ErrorEnum::eFailed;

    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    const auto& activeInstances = mInstanceManager->GetActiveInstances();

    ASSERT_EQ(activeInstances.Size(), 2);
    EXPECT_EQ(activeInstances[0]->GetInfo().mInstanceIdent.mItemID, cService1);
    EXPECT_EQ(activeInstances[1]->GetInfo().mInstanceIdent.mItemID, cService2);
}

TEST_F(CMLauncherInstanceManagerTest, UpdateStatus)
{
    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    auto [instance, createErr] = mInstanceManager->CreateInstance(CreateRunRequest(cService1, cSubject1), 0);
    ASSERT_TRUE(createErr.IsNone());

    mInstanceManager->ScheduleInstance(instance, ErrorEnum::eFailed);
    ASSERT_TRUE(mInstanceManager->SubmitScheduledInstances().IsNone());
    ASSERT_EQ(mInstanceManager->GetActiveInstances().Size(), 1);

    auto status = CreateInstanceStatus(
        CreateInstanceIdent(cService1, cSubject1, 0), "", "", aos::InstanceStateEnum::eActive, ErrorEnum::eNone);

    // Status for active instance missing in status list.
    EXPECT_TRUE(mInstanceManager->UpdateStatus(status).IsNone());
    EXPECT_EQ(instance->GetStatus().mState, aos::InstanceStateEnum::eActive);

    mStorage.mUpdateErr = ErrorEnum::eFailed;

    EXPECT_TRUE(mInstanceManager->UpdateStatus(status).Is(ErrorEnum::eFailed));

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    statuses->PushBack(status);
    statuses->PushBack(CreateInstanceStatus(
        CreateInstanceIdent(cService2, cSubject1, 0), "", "", aos::InstanceStateEnum::eActive, ErrorEnum::eNone));

    EXPECT_TRUE(mInstanceManager->UpdateRunningInstances("", *statuses).Is(ErrorEnum::eFailed));
    EXPECT_EQ(mInstanceManager->GetRunningInstances().Size(), 2);

    mStorage.mUpdateErr = ErrorEnum::eNone;

    EXPECT_TRUE(mInstanceManager
                    ->UpdateStatus(CreateInstanceStatus(CreateInstanceIdent(cService3, cSubject1, 0), "", "",
                        aos::InstanceStateEnum::eActive, ErrorEnum::eNone))
                    .IsNone());

    auto componentIdent = CreateInstanceIdent(cComponent1, "", 0, UpdateItemTypeEnum::eComponent);

    auto component = CreateInstanceStatus(
        componentIdent, cNodeIDLocalSM, cRunnerRootfs, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", true);

    EXPECT_TRUE(mInstanceManager->UpdateStatus(component).IsNone());
    EXPECT_EQ(mInstanceManager->FindPreinstalledComponent(componentIdent), nullptr);

    statuses->Clear();
    statuses->PushBack(component);

    EXPECT_TRUE(mInstanceManager->UpdateRunningInstances(cNodeIDLocalSM, *statuses).IsNone());
    ASSERT_EQ(mInstanceManager->GetPreinstalledComponents().Size(), 1);
    ASSERT_NE(mInstanceManager->FindPreinstalledComponent(componentIdent), nullptr);

    component.mState = aos::InstanceStateEnum::eFailed;

    EXPECT_TRUE(mInstanceManager->UpdateStatus(component).IsNone());
    EXPECT_EQ(mInstanceManager->FindPreinstalledComponent(componentIdent)->mState, aos::InstanceStateEnum::eFailed);

    auto otherVersionIdent = componentIdent;

    otherVersionIdent.mVersion = "1.0.0";

    EXPECT_EQ(mInstanceManager->FindPreinstalledComponent(otherVersionIdent), nullptr);
}

TEST_F(CMLauncherInstanceManagerTest, CreateInstanceErrors)
{
    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    auto request      = CreateRunRequest(cService1, cSubject1);
    auto newInstances = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    mStorage.mAddErr = ErrorEnum::eFailed;

    auto [failed1, addErr1] = mInstanceManager->CreateInstance(request, 0);
    EXPECT_TRUE(addErr1.Is(ErrorEnum::eFailed));
    EXPECT_FALSE(failed1);

    auto [failed2, addErr2] = mInstanceManager->CreateInstance(request, cNodeIDLocalSM, cRunnerRunc, *newInstances);
    EXPECT_TRUE(addErr2.Is(ErrorEnum::eFailed));
    EXPECT_FALSE(failed2);

    mStorage.mAddErr = ErrorEnum::eNone;

    // Scheduled instance with changed index digest fails to update storage.
    auto [instance, createErr] = mInstanceManager->CreateInstance(request, 0);
    ASSERT_TRUE(createErr.IsNone());

    mInstanceManager->ScheduleInstance(instance, ErrorEnum::eFailed);
    EXPECT_EQ(mInstanceManager->GetScheduledInstances().Size(), 1);

    request.mIndexDigest = "newIndexDigest";
    mStorage.mUpdateErr  = ErrorEnum::eFailed;

    auto [failed3, updateErr] = mInstanceManager->CreateInstance(request, 0);
    EXPECT_TRUE(updateErr.Is(ErrorEnum::eFailed));
    EXPECT_FALSE(failed3);

    mStorage.mUpdateErr = ErrorEnum::eNone;

    // Cached instance is reused.
    ASSERT_TRUE(mInstanceManager->SubmitScheduledInstances().IsNone());
    mInstanceManager->DisableInstance(instance);

    ASSERT_EQ(mInstanceManager->GetCachedInstances().Size(), 1);

    auto [cached, cachedErr] = mInstanceManager->CreateInstance(request, 0);
    ASSERT_TRUE(cachedErr.IsNone());
    EXPECT_EQ(cached.Get(), instance.Get());
}

TEST_F(CMLauncherInstanceManagerTest, GeneratedComponentInstances)
{
    auto activeInfo = CreateInstanceInfo(CreateInstanceIdent(cComponent1, cSubject1, 0, UpdateItemTypeEnum::eComponent),
        {}, cRunnerRootfs, cNodeIDLocalSM, InstanceStateEnum::eActive);
    activeInfo.mDisableRebalancing = true;

    auto cachedInfo
        = CreateInstanceInfo(CreateInstanceIdent("component2", cSubject1, 0, UpdateItemTypeEnum::eComponent), {},
            cRunnerRootfs, cNodeIDLocalSM, InstanceStateEnum::eCached, 0, 0, Time::Now().Add(Time::cHours));
    cachedInfo.mDisableRebalancing = true;

    ASSERT_TRUE(mStorage.AddInstance(activeInfo).IsNone());
    ASSERT_TRUE(mStorage.AddInstance(cachedInfo).IsNone());

    ASSERT_TRUE(mInstanceManager->Start().IsNone());
    ASSERT_EQ(mInstanceManager->GetActiveInstances().Size(), 1);
    ASSERT_EQ(mInstanceManager->GetCachedInstances().Size(), 1);

    auto activeRequest = CreateRunRequest(cComponent1, cSubject1, 0, 0, "", {}, UpdateItemTypeEnum::eComponent, "");
    auto cachedRequest = CreateRunRequest("component2", cSubject1, 0, 0, "", {}, UpdateItemTypeEnum::eComponent, "");
    auto newInstances  = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    // Existing instances are reused.
    auto [active, activeErr]
        = mInstanceManager->CreateInstance(activeRequest, cNodeIDLocalSM, cRunnerRootfs, *newInstances);
    ASSERT_TRUE(activeErr.IsNone());
    EXPECT_EQ(active.Get(), mInstanceManager->GetActiveInstances()[0].Get());

    auto [cached, cachedErr]
        = mInstanceManager->CreateInstance(cachedRequest, cNodeIDLocalSM, cRunnerRootfs, *newInstances);
    ASSERT_TRUE(cachedErr.IsNone());
    EXPECT_EQ(cached.Get(), mInstanceManager->GetCachedInstances()[0].Get());

    // Not component request is ignored.
    EXPECT_TRUE(mInstanceManager->RemoveGeneratedInstances(CreateRunRequest(cService1, cSubject1)).IsNone());

    // Remove errors are reported but instances are removed from the lists.
    mStorage.mRemoveErr = ErrorEnum::eFailed;

    EXPECT_TRUE(mInstanceManager->RemoveGeneratedInstances(cachedRequest).Is(ErrorEnum::eFailed));
    EXPECT_TRUE(mInstanceManager->GetCachedInstances().IsEmpty());

    EXPECT_TRUE(mInstanceManager->RemoveGeneratedInstances(activeRequest).Is(ErrorEnum::eFailed));
    EXPECT_TRUE(mInstanceManager->GetActiveInstances().IsEmpty());
}

TEST_F(CMLauncherInstanceManagerTest, CacheErrors)
{
    for (const auto& itemID : {cService1, cService2}) {
        ASSERT_TRUE(mStorage
                        .AddInstance(CreateInstanceInfo(CreateInstanceIdent(itemID, cSubject1, 0), {}, cRunnerRunc,
                            cNodeIDLocalSM, InstanceStateEnum::eActive))
                        .IsNone());
    }

    const auto componentIdent = CreateInstanceIdent(cComponent1, cSubject1, 0, UpdateItemTypeEnum::eComponent);

    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(
                        componentIdent, {}, cRunnerRootfs, cNodeIDLocalSM, InstanceStateEnum::eActive))
                    .IsNone());

    ASSERT_TRUE(mInstanceManager->Start().IsNone());
    ASSERT_EQ(mInstanceManager->GetActiveInstances().Size(), 3);

    auto findActive = [this](const char* itemID) {
        return *mInstanceManager->GetActiveInstances().FindIf([itemID](const SharedPtr<Instance>& instance) {
            return instance->GetInfo().mInstanceIdent.mItemID == itemID;
        });
    };

    auto component = findActive(cComponent1);
    auto service1  = findActive(cService1);

    // Disable fails to update storage.
    mStorage.mUpdateErr = ErrorEnum::eFailed;

    mInstanceManager->DisableInstance(component);
    mInstanceManager->DisableInstance(service1);

    EXPECT_EQ(mInstanceManager->GetActiveInstances().Size(), 1);
    EXPECT_EQ(mInstanceManager->GetCachedInstances().Size(), 2);

    // Cache fails to cleanup storage state.
    mStorage.mUpdateErr       = ErrorEnum::eNone;
    mStorageState.mCleanupErr = ErrorEnum::eFailed;

    ASSERT_TRUE(mInstanceManager->PrepareForBalancing().IsNone());
    EXPECT_TRUE(mInstanceManager->SubmitScheduledInstances().IsNone());

    EXPECT_TRUE(mInstanceManager->GetActiveInstances().IsEmpty());
    EXPECT_EQ(mInstanceManager->GetCachedInstances().Size(), 3);
}

TEST_F(CMLauncherInstanceManagerTest, SetSubjects)
{
    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), {}, cRunnerRunc,
                        cNodeIDLocalSM, InstanceStateEnum::eActive, 0, 0, Time::Now(), "", true))
                    .IsNone());

    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    StaticArray<StaticString<cIDLen>, 1> subjects;

    // Active instance subject is disabled.
    auto [rebalance1, err1] = mInstanceManager->SetSubjects(subjects);
    ASSERT_TRUE(err1.IsNone());
    EXPECT_TRUE(rebalance1);

    auto instance = mInstanceManager->GetActiveInstances()[0];

    mInstanceManager->DisableInstance(instance);

    // Disabled cached instance subject is enabled.
    subjects.PushBack(cSubject1);

    auto [rebalance2, err2] = mInstanceManager->SetSubjects(subjects);
    ASSERT_TRUE(err2.IsNone());
    EXPECT_TRUE(rebalance2);
}

TEST_F(CMLauncherInstanceManagerTest, ScheduleInstanceFails)
{
    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    auto [instance, createErr] = mInstanceManager->CreateInstance(
        CreateRunRequest(cComponent1, cSubject1, 0, 1, "", {}, UpdateItemTypeEnum::eComponent), 0);
    ASSERT_TRUE(createErr.IsNone());

    TestNode node;

    mStorage.mUpdateErr = ErrorEnum::eFailed;

    EXPECT_TRUE(mInstanceManager->ScheduleInstance(instance, node, cRunnerRootfs).Is(ErrorEnum::eFailed));
    EXPECT_TRUE(mInstanceManager->GetScheduledInstances().IsEmpty());
}

TEST_F(CMLauncherInstanceManagerTest, PrepareForBalancingFails)
{
    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    mStorageState.mStateSizeErr = ErrorEnum::eFailed;

    EXPECT_TRUE(mInstanceManager->PrepareForBalancing().Is(ErrorEnum::eFailed));
}

/***********************************************************************************************************************
 * Instance tests
 **********************************************************************************************************************/

class CMLauncherInstanceTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        mImageStore.Init();
        mStorage.Init();
        mStorageStateStub.Init();

        mImageInfoProvider.Init(mAllocator, mImageStore, mImageStore);
        mStorageState.Init(mAllocator, mStorageStateStub);

        ASSERT_TRUE(mStorageState.Start().IsNone());
        ASSERT_TRUE(mUIDPool.Init(ValidateUID).IsNone());
        ASSERT_TRUE(mGIDPool.Init(ValidateGID).IsNone());
    }

    std::unique_ptr<ServiceInstance> CreateService(const InstanceInfo& info)
    {
        EXPECT_TRUE(mStorage.AddInstance(info).IsNone());

        return std::make_unique<ServiceInstance>(
            mAllocator, info, mUIDPool, mGIDPool, mStorage, mStorageState, mImageInfoProvider);
    }

    std::unique_ptr<ServiceInstance> CreateServiceWithConfig(const std::string& itemID, const oci::ItemConfig& config)
    {
        AddItem(itemID, cImageID1, config);

        auto instance = CreateService(CreateInstanceInfo(CreateInstanceIdent(itemID, cSubject1, 0)));

        EXPECT_TRUE(instance->Init().IsNone());
        EXPECT_TRUE(instance->LoadConfigs(CreateImageDescriptor(itemID)).IsNone());

        return instance;
    }

    void AddItem(const std::string& itemID, const std::string& imageID, const oci::ItemConfig& config)
    {
        mImageStore.AddItem(itemID.c_str(), imageID.c_str(), config, CreateImageConfig());
    }

    FailingAllocator             mAllocator;
    imagemanager::ImageStoreStub mImageStore;
    FaultyStorageStub            mStorage;
    FaultyStorageStateStub       mStorageStateStub;
    StorageState                 mStorageState;
    ImageInfoProvider            mImageInfoProvider;
    UIDPool                      mUIDPool;
    GIDPool                      mGIDPool;
};

TEST_F(CMLauncherInstanceTest, LoadConfigs)
{
    auto instance = CreateService(CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0)));

    EXPECT_TRUE(instance->LoadConfigs(CreateImageDescriptor(cService1)).Is(ErrorEnum::eNotFound));

    oci::ItemConfig itemConfig;

    AddItem(cService1, cImageID1, itemConfig);
    mImageStore.SetBlobError("service1:image1:config", ErrorEnum::eFailed);

    EXPECT_TRUE(instance->LoadConfigs(CreateImageDescriptor(cService1)).Is(ErrorEnum::eFailed));

    AddItem(cService2, cImageID1, itemConfig);

    ASSERT_TRUE(instance->LoadConfigs(CreateImageDescriptor(cService2)).IsNone());

    EXPECT_TRUE(instance->IsRuntimeTypeOk("crun", "crun"));
    EXPECT_TRUE(instance->IsRuntimeTypeOk("runc", "runc"));
    EXPECT_FALSE(instance->IsRuntimeTypeOk("runx", "runx"));

    EXPECT_TRUE(instance->IsPlatformOk(CreatePlatform()));
    EXPECT_FALSE(instance->IsPlatformOk(CreatePlatform("x86_64", "other")));
    EXPECT_FALSE(instance->IsPlatformOk(CreatePlatform("x86_64", "generic", "linux", "6.0.0")));
    EXPECT_FALSE(instance->IsPlatformOk(CreatePlatform("x86_64", "generic", "linux", "5.4.0", "feature2")));
}

TEST_F(CMLauncherInstanceTest, OverrideEnvVars)
{
    auto instance = CreateService(CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0)));

    auto request = std::make_unique<OverrideEnvVarsRequest>();

    EnvVarsInstanceInfo otherItem;
    EnvVarInfo          var;

    otherItem.mItemID.EmplaceValue(cService2);
    static_cast<EnvVar&>(var) = CreateEnvVar("VAR0", "value0");
    otherItem.mVariables.PushBack(var);
    request->mItems.PushBack(otherItem);

    EnvVarsInstanceInfo item1;

    item1.mItemID.EmplaceValue(cService1);
    static_cast<EnvVar&>(var) = CreateEnvVar("VAR1", "value1");
    item1.mVariables.PushBack(var);
    request->mItems.PushBack(item1);

    EnvVarsInstanceInfo item2;

    item2.mItemID.EmplaceValue(cService1);
    static_cast<EnvVar&>(var) = CreateEnvVar("VAR1", "value2");
    item2.mVariables.PushBack(var);
    static_cast<EnvVar&>(var) = CreateEnvVar("VAR2", "value3");
    item2.mVariables.PushBack(var);
    request->mItems.PushBack(item2);

    auto [changed, err] = instance->OverrideEnvVars(*request);
    ASSERT_TRUE(err.IsNone());
    EXPECT_TRUE(changed);

    EnvVarArray expectedEnvVars;

    expectedEnvVars.PushBack(CreateEnvVar("VAR1", "value2"));
    expectedEnvVars.PushBack(CreateEnvVar("VAR2", "value3"));

    EXPECT_EQ(instance->GetSMInfo().mEnvVars, expectedEnvVars);

    auto [changedAgain, errAgain] = instance->OverrideEnvVars(*request);
    ASSERT_TRUE(errAgain.IsNone());
    EXPECT_FALSE(changedAgain);
}

TEST_F(CMLauncherInstanceTest, StorageErrors)
{
    auto instance = CreateService(CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0)));

    mStorage.mUpdateErr = ErrorEnum::eFailed;

    EXPECT_TRUE(instance
                    ->UpdateStatus(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), cNodeIDLocalSM,
                        cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone))
                    .Is(ErrorEnum::eFailed));

    instance->SetError(ErrorEnum::eTimeout);

    EXPECT_EQ(instance->GetStatus().mState, aos::InstanceStateEnum::eFailed);
    EXPECT_TRUE(instance->GetStatus().mError.Is(ErrorEnum::eTimeout));
    EXPECT_TRUE(instance->GetStatus().mNodeID.IsEmpty());
}

TEST_F(CMLauncherInstanceTest, ServiceInitErrors)
{
    auto instance1 = CreateService(CreateInstanceInfo(
        CreateInstanceIdent(cService1, cSubject1, 0), {}, cRunnerRunc, "", InstanceStateEnum::eActive, 7100, 6100));
    ASSERT_TRUE(instance1->Init().IsNone());

    auto instance2 = CreateService(CreateInstanceInfo(
        CreateInstanceIdent(cService2, cSubject1, 0), {}, cRunnerRunc, "", InstanceStateEnum::eActive, 7100, 6200));
    EXPECT_FALSE(instance2->Init().IsNone());

    auto instance3 = CreateService(CreateInstanceInfo(
        CreateInstanceIdent(cService1, cSubject1, 1), {}, cRunnerRunc, "", InstanceStateEnum::eActive, 7101, 6101));
    EXPECT_TRUE(instance3->Init().Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CMLauncherInstanceTest, ServiceRemoveAndCacheErrors)
{
    auto instance = CreateService(CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0)));
    ASSERT_TRUE(instance->Init().IsNone());

    mStorage.mUpdateErr = ErrorEnum::eFailed;

    EXPECT_TRUE(instance->Cache().Is(ErrorEnum::eFailed));

    mStorage.mUpdateErr           = ErrorEnum::eNone;
    mStorageStateStub.mCleanupErr = ErrorEnum::eWrongState;

    EXPECT_TRUE(instance->Cache(true).Is(ErrorEnum::eWrongState));
    EXPECT_EQ(instance->GetInfo().mState, InstanceStateEnum::eDisabled);

    mStorageStateStub.mRemoveErr = ErrorEnum::eFailed;

    EXPECT_TRUE(instance->Remove().Is(ErrorEnum::eFailed));

    mStorageStateStub.mRemoveErr = ErrorEnum::eNone;
    mStorage.mRemoveErr          = ErrorEnum::eWrongState;

    EXPECT_TRUE(instance->Remove().Is(ErrorEnum::eWrongState));
}

TEST_F(CMLauncherInstanceTest, ServiceRequestedResources)
{
    TestNode node;

    // Requested resources are clamped by quotas.
    oci::ItemConfig clampConfig;

    CreateItemConfig(clampConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled,
        CreateServiceQuotas(1000, 1000, 100, 200), CreateRequestedResources(10, 10, 500, 800));

    auto clamped = CreateServiceWithConfig(cService1, clampConfig);

    EXPECT_TRUE(clamped->IsAvailableCpuOk(100, node));
    EXPECT_FALSE(clamped->IsAvailableCpuOk(99, node));
    EXPECT_TRUE(clamped->IsAvailableRamOk(200, node));
    EXPECT_FALSE(clamped->IsAvailableRamOk(199, node));

    // Monitoring data is used for node which requires balancing.
    auto monitoringData = CreateInstanceMonitoring(CreateInstanceIdent(cService1, cSubject1, 0), 150.0).mMonitoringData;

    monitoringData.mRAM = 300;

    clamped->UpdateMonitoringData(monitoringData);
    node.SetNeedBalancing(true);

    EXPECT_FALSE(clamped->IsAvailableCpuOk(149, node));
    EXPECT_TRUE(clamped->IsAvailableCpuOk(150, node));
    EXPECT_FALSE(clamped->IsAvailableRamOk(299, node));
    EXPECT_TRUE(clamped->IsAvailableRamOk(300, node));

    node.SetNeedBalancing(false);

    // Node ratios greater than 100% are limited.
    oci::ItemConfig ratioConfig;

    CreateItemConfig(
        ratioConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled, CreateServiceQuotas(100, 100, 100, 200));

    auto ratio = CreateServiceWithConfig(cService2, ratioConfig);

    node.SetResourceRatios(CreateResourceRatios(150, 150, 150, 150));

    EXPECT_FALSE(ratio->IsAvailableCpuOk(99, node));
    EXPECT_TRUE(ratio->IsAvailableCpuOk(100, node));
    EXPECT_FALSE(ratio->IsAvailableRamOk(199, node));
    EXPECT_TRUE(ratio->IsAvailableRamOk(200, node));

    ASSERT_TRUE(ratio->Schedule(node, cRunnerRunc).IsNone());
    EXPECT_EQ(ratio->GetSMInfo().mStoragePath, "storage_path");
    EXPECT_EQ(ratio->GetStatus().mNodeID, cNodeIDLocalSM);

    // Resource limits are skipped.
    oci::ItemConfig skipConfig;

    CreateItemConfig(skipConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled,
        CreateServiceQuotas(2000, 2000, 100, 200), CreateRequestedResources(2000, 2000, 500, 800));
    skipConfig.mSkipResourceLimits = true;

    auto skip = CreateServiceWithConfig(cService3, skipConfig);

    EXPECT_TRUE(skip->IsAvailableCpuOk(0, node));
    EXPECT_TRUE(skip->IsAvailableRamOk(0, node));
    EXPECT_TRUE(skip->Schedule(node, cRunnerRunc).IsNone());
}

TEST_F(CMLauncherInstanceTest, ServiceScheduleErrors)
{
    TestNode        node;
    oci::ItemConfig itemConfig;

    CreateItemConfig(itemConfig, {cRunnerRunc});

    auto instance = CreateServiceWithConfig(cService1, itemConfig);

    node.SetReserveError(ErrorEnum::eNoMemory);

    EXPECT_TRUE(instance->Schedule(node, cRunnerRunc).Is(ErrorEnum::eNoMemory));

    node.SetReserveError(ErrorEnum::eNone);
    mStorage.mUpdateErr = ErrorEnum::eFailed;

    EXPECT_TRUE(instance->Schedule(node, cRunnerRunc).Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherInstanceTest, Component)
{
    auto info = CreateInstanceInfo(CreateInstanceIdent(cComponent1, cSubject1, 0, UpdateItemTypeEnum::eComponent));

    ASSERT_TRUE(mStorage.AddInstance(info).IsNone());

    ComponentInstance instance(mAllocator, info, mStorage, mImageInfoProvider);
    TestNode          node;

    ASSERT_TRUE(instance.Init().IsNone());
    EXPECT_EQ(instance.GetBalancingPolicy(), oci::BalancingPolicyEnum::eDisabled);

    mStorage.mUpdateErr = ErrorEnum::eFailed;

    EXPECT_TRUE(instance.Schedule(node, cRunnerRootfs).Is(ErrorEnum::eFailed));
    EXPECT_TRUE(instance.Cache().Is(ErrorEnum::eFailed));

    mStorage.mUpdateErr = ErrorEnum::eNone;
    mStorage.mRemoveErr = ErrorEnum::eFailed;

    EXPECT_TRUE(instance.Remove().Is(ErrorEnum::eFailed));

    mStorage.mRemoveErr = ErrorEnum::eNone;

    EXPECT_TRUE(instance.Remove().IsNone());
    EXPECT_TRUE(instance.Remove().IsNone());
}

/***********************************************************************************************************************
 * Node tests
 **********************************************************************************************************************/

class CMLauncherNodeTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        auto runtimeX = CreateRuntime(cRunnerRunx);
        runtimeX.mAllowedDMIPS.SetValue(100);
        runtimeX.mAllowedRAM.SetValue(200);

        mNodeInfo = CreateNodeInfo(
            cNodeIDLocalSM, 1000, 1000, {CreateRuntime(cRunnerRunc, 1), runtimeX}, {CreateResource("resource1", 1)});

        mNode.Init(mAllocator, cNodeIDLocalSM, mNodeConfigProvider, mRunner);
        (void)mNode.UpdateInfo(mNodeInfo);
        mNode.NotifyInstanceStatusReceived();
    }

    void SetNodeConfig(const AlertRules& alertRules = {})
    {
        NodeConfig nodeConfig;

        CreateNodeConfig(nodeConfig, cNodeIDLocalSM, 0, {}, {}, alertRules);
        mNodeConfigProvider.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, nodeConfig);
    }

    FailingAllocator                     mAllocator;
    resourcemanager::ResourceManagerStub mNodeConfigProvider;
    StrictMock<InstanceRunnerMock>       mRunner;
    UnitNodeInfo                         mNodeInfo;
    Node                                 mNode;
};

TEST_F(CMLauncherNodeTest, ReserveResources)
{
    SetNodeConfig();
    mNode.PrepareForBalancing(false);

    auto ident = CreateInstanceIdent(cService1, cSubject1, 0);

    EXPECT_EQ(mNode.GetAvailableCPU(cRunnerRunx), 100);
    EXPECT_EQ(mNode.GetAvailableRAM(cRunnerRunx), 200);
    EXPECT_EQ(mNode.GetAvailableCPU("unknown"), 0);
    EXPECT_EQ(mNode.GetAvailableRAM("unknown"), 0);
    EXPECT_TRUE(mNode.IsMaxNumInstancesReached("unknown"));

    EXPECT_TRUE(mNode.ReserveResources(ident, "unknown", 0, 0, {}).Is(ErrorEnum::eFailed));

    EXPECT_TRUE(mNode.ReserveResources(ident, cRunnerRunc, 0, 2000, {}).Is(ErrorEnum::eNoMemory));

    EXPECT_TRUE(mNode.ReserveResources(ident, cRunnerRunc, 2000, 10, {}).Is(ErrorEnum::eNoMemory));
    EXPECT_EQ(mNode.GetAvailableRAM(cRunnerRunc), 1000);

    StaticArray<oci::ResourceInfo, 2> resources;
    oci::ResourceInfo                 resource;

    resource.mName = "resource1";
    resources.PushBack(resource);
    resource.mName = "resource2";
    resources.PushBack(resource);

    EXPECT_TRUE(mNode.ReserveResources(ident, cRunnerRunc, 10, 10, resources).Is(ErrorEnum::eNoMemory));
    EXPECT_EQ(mNode.GetAvailableResourceCount("resource1"), 1);
    EXPECT_EQ(mNode.GetAvailableCPU(cRunnerRunc), 1000);
    EXPECT_EQ(mNode.GetAvailableRAM(cRunnerRunc), 1000);

    resources.Resize(1);

    EXPECT_TRUE(mNode.ReserveResources(ident, cRunnerRunc, 10, 10, resources).IsNone());
    EXPECT_EQ(mNode.GetAvailableResourceCount("resource1"), 0);
    EXPECT_TRUE(mNode.IsMaxNumInstancesReached(cRunnerRunc));
    EXPECT_TRUE(mNode.ReserveResources(ident, cRunnerRunc, 0, 0, {}).Is(ErrorEnum::eNoMemory));
}

TEST_F(CMLauncherNodeTest, PrepareForRebalancing)
{
    mNode.PrepareForBalancing(true);
    EXPECT_FALSE(mNode.NeedBalancing());

    SetNodeConfig(CreateAlertRules(0, 50));

    // Instances consume more than node total usage.
    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();

    CreateNodeMonitoring(
        *nodeMonitoring, cNodeIDLocalSM, 100.0, {CreateInstanceMonitoring(CreateInstanceIdent(cService1), 200.0)});

    nodeMonitoring->mMonitoringData.mRAM               = 900;
    nodeMonitoring->mInstances[0].mMonitoringData.mRAM = 950;

    mNode.UpdateMonitoringData(*nodeMonitoring);
    mNode.PrepareForBalancing(true);

    EXPECT_TRUE(mNode.NeedBalancing());
    EXPECT_EQ(mNode.GetAvailableCPU(), 1000);
    EXPECT_EQ(mNode.GetAvailableRAM(), 500);
}

TEST_F(CMLauncherNodeTest, SendInstancesErrors)
{
    auto running = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    auto active  = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    running->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), cNodeIDLocalSM, cRunnerRunc,
        aos::InstanceStateEnum::eActive, ErrorEnum::eNone));

    EXPECT_CALL(mRunner, UpdateInstances(_, _, _)).WillRepeatedly(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(mNode.SendScheduledInstances(*active, *running).Is(ErrorEnum::eFailed));

    auto [sent, err] = mNode.ResendInstances(*active, *running, false);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed));
    EXPECT_FALSE(sent);

    mNodeInfo.mIsConnected = false;

    (void)mNode.UpdateInfo(mNodeInfo);
    EXPECT_FALSE(mNode.IsConnected());
}

/***********************************************************************************************************************
 * NodeManager tests
 **********************************************************************************************************************/

class CMLauncherNodeManagerTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        mImageStore.Init();
        mStorage.Init();
        mImageInfoProvider.Init(mAllocator, mImageStore, mImageStore);

        ASSERT_TRUE(mOverrideEnvVarsProcessor.Init(CreateConfig(), mStorage, mSender, mListener).IsNone());

        mNodeManager->Init(mAllocator, mNodeInfoProvider, mNodeConfigProvider, mRunner, mOverrideEnvVarsProcessor);
    }

    void TearDown() override { EXPECT_TRUE(mNodeManager->Stop().IsNone()); }

    SharedPtr<Instance> CreateComponent(
        const std::string& itemID, const std::string& manifestDigest, const std::string& nodeID = cNodeIDLocalSM)
    {
        auto info = CreateInstanceInfo(CreateInstanceIdent(itemID, cSubject1, 0, UpdateItemTypeEnum::eComponent),
            manifestDigest.c_str(), cRunnerRootfs, nodeID.c_str());

        EXPECT_TRUE(mStorage.AddInstance(info).IsNone());

        return MakeShared<ComponentInstance>(&mAllocator, mAllocator, info, mStorage, mImageInfoProvider);
    }

    FailingAllocator                                 mAllocator;
    NiceMock<nodeinfoprovider::NodeInfoProviderMock> mNodeInfoProvider;
    resourcemanager::ResourceManagerStub             mNodeConfigProvider;
    StrictMock<InstanceRunnerMock>                   mRunner;
    imagemanager::ImageStoreStub                     mImageStore;
    ImageInfoProvider                                mImageInfoProvider;
    FaultyStorageStub                                mStorage;
    SenderStub                                       mSender;
    OverrideEnvVarsListenerStub                      mListener;
    OverrideEnvVarsProcessor                         mOverrideEnvVarsProcessor;
    std::unique_ptr<NodeManager>                     mNodeManager = std::make_unique<NodeManager>();
};

TEST_F(CMLauncherNodeManagerTest, StartErrors)
{
    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_)).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(mNodeManager->Start().Is(ErrorEnum::eFailed));

    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_)).WillOnce(Invoke([](Array<StaticString<cIDLen>>& ids) {
        return ids.PushBack(cNodeIDLocalSM);
    }));
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(_, _)).WillOnce(Return(ErrorEnum::eNotFound));

    EXPECT_TRUE(mNodeManager->Start().Is(ErrorEnum::eNotFound));
}

TEST_F(CMLauncherNodeManagerTest, UpdateNodes)
{
    // Status received before node info.
    ASSERT_TRUE(mNodeManager->NotifyNodeStatusReceived(cNodeIDRemoteSM1).IsNone());
    ASSERT_NE(mNodeManager->FindNode(cNodeIDRemoteSM1), nullptr);
    EXPECT_FALSE(mNodeManager->FindNode(cNodeIDRemoteSM1)->IsConnected());

    auto nodeInfo = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)});

    EXPECT_TRUE(mNodeManager->UpdateNodeInfo(nodeInfo));
    EXPECT_FALSE(mNodeManager->UpdateNodeInfo(nodeInfo));
    EXPECT_EQ(mNodeManager->GetNodes().Size(), 2);

    nodeInfo.mState = NodeStateEnum::eUnprovisioned;

    EXPECT_TRUE(mNodeManager->UpdateNodeInfo(nodeInfo));
    EXPECT_EQ(mNodeManager->FindNode(cNodeIDLocalSM)->GetInfo().mState, NodeStateEnum::eUnprovisioned);
}

TEST_F(CMLauncherNodeManagerTest, SendInstancesErrors)
{
    ASSERT_TRUE(mNodeManager->UpdateNodeInfo(CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)})));
    ASSERT_TRUE(mNodeManager->NotifyNodeStatusReceived(cNodeIDLocalSM).IsNone());

    auto running = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    auto active  = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    running->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), cNodeIDLocalSM, cRunnerRunc,
        aos::InstanceStateEnum::eActive, ErrorEnum::eNone));

    EXPECT_CALL(mRunner, UpdateInstances(_, _, _)).WillRepeatedly(Return(ErrorEnum::eFailed));

    Mutex             mutex;
    UniqueLock<Mutex> lock {mutex};

    EXPECT_TRUE(mNodeManager->SendScheduledInstances(lock, *active, *running).Is(ErrorEnum::eFailed));

    StaticArray<StaticString<cIDLen>, 1> updatedNodes;

    // Node is not updated.
    updatedNodes.PushBack(cNodeIDRemoteSM1);

    EXPECT_TRUE(mNodeManager->ResendInstances(lock, updatedNodes, *active, *running).IsNone());

    updatedNodes[0] = cNodeIDLocalSM;

    EXPECT_TRUE(mNodeManager->ResendInstances(lock, updatedNodes, *active, *running).Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherNodeManagerTest, LoadSMDataForActiveInstances)
{
    ASSERT_TRUE(
        mNodeManager->UpdateNodeInfo(CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRootfs)})));

    oci::ItemConfig itemConfig;

    CreateItemConfig(itemConfig, {cRunnerRootfs});

    for (const auto& itemID : {"component2", "component3", "component4"}) {
        mImageStore.AddItem(itemID, cImageID1, itemConfig, CreateImageConfig());
    }

    mImageStore.SetBlobError("component3:image1:item", ErrorEnum::eFailed);

    auto instances = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    instances->PushBack(CreateComponent("component0", "", ""));
    instances->PushBack(CreateComponent(cComponent1, BuildManifestDigest(cComponent1).CStr(), cNodeIDRemoteSM1));
    instances->PushBack(CreateComponent("component5", BuildManifestDigest("component5").CStr()));
    instances->PushBack(CreateComponent("component2", "unknownManifest"));
    instances->PushBack(CreateComponent("component3", BuildManifestDigest("component3").CStr()));
    instances->PushBack(CreateComponent("component4", BuildManifestDigest("component4").CStr()));

    mStorage.mUpdateErr = ErrorEnum::eFailed;

    EXPECT_TRUE(mNodeManager->LoadSMDataForActiveInstances(*instances, mImageInfoProvider).IsNone());

    for (size_t i = 0; i < 5; i++) {
        EXPECT_TRUE((*instances)[i]->GetSMInfo().mRuntimeID.IsEmpty());
    }

    EXPECT_EQ((*instances)[5]->GetSMInfo().mRuntimeID, cRunnerRootfs);
}

TEST_F(CMLauncherInstanceManagerTest, RunRequestsLoader)
{
    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    ImageInfoProvider imageInfoProvider;

    imageInfoProvider.Init(mAllocator, mImageStore, mImageStore);

    auto loader = std::make_unique<RunRequestsLoader>();

    loader->Init(mAllocator, mStorage, *mInstanceManager, imageInfoProvider);

    oci::ItemConfig itemConfig;

    CreateItemConfig(itemConfig, {cRunnerRootfs, cRunnerRootfs});

    for (const auto& itemID : {"component2", "component3"}) {
        mImageStore.AddItem(itemID, cImageID1, itemConfig, CreateImageConfig());
    }

    mImageStore.SetBlobError("component3:image1:item", ErrorEnum::eFailed);

    resourcemanager::ResourceManagerStub nodeConfigProvider;
    NiceMock<InstanceRunnerMock>         runner;
    auto                                 nodes = std::make_unique<StaticArray<Node, 1>>();

    nodes->EmplaceBack();
    nodes->Back().Init(mAllocator, cNodeIDLocalSM, nodeConfigProvider, runner);
    (void)nodes->Back().UpdateInfo(
        CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc), CreateRuntime(cRunnerRootfs)}));

    // Requests: unknown component image, generated component, component with broken item config and service. Unit
    // subjects are not enabled.
    auto requests  = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    auto instances = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    requests->PushBack(CreateRunRequest(cComponent1, cSubject1, 0, 0, "", {}, UpdateItemTypeEnum::eComponent));
    requests->PushBack(CreateRunRequest("component2", cSubject1, 0, 0, "", {}, UpdateItemTypeEnum::eComponent));
    requests->PushBack(CreateRunRequest("component3", cSubject1, 0, 0, "", {}, UpdateItemTypeEnum::eComponent));
    requests->PushBack(CreateRunRequest(cService1, cSubject1, 0, 1));

    ASSERT_TRUE(loader->Save(*requests).IsNone());

    loader->CreateInstances(*nodes, *instances);

    EXPECT_TRUE(instances->IsEmpty());
    ASSERT_EQ(mInstanceManager->GetCachedInstances().Size(), 2);
    EXPECT_EQ(mInstanceManager->GetCachedInstances()[0]->GetInfo().mInstanceIdent.mItemID, "component2");
    EXPECT_EQ(mInstanceManager->GetCachedInstances()[0]->GetInfo().mRuntimeID, cRunnerRootfs);
    EXPECT_EQ(mInstanceManager->GetCachedInstances()[0]->GetInfo().mState, InstanceStateEnum::eDisabled);
    EXPECT_EQ(mInstanceManager->GetCachedInstances()[1]->GetInfo().mInstanceIdent.mItemID, cService1);
    EXPECT_EQ(mInstanceManager->GetCachedInstances()[1]->GetInfo().mState, InstanceStateEnum::eDisabled);

    mStorage.mAddErr    = ErrorEnum::eFailed;
    mStorage.mRemoveErr = ErrorEnum::eFailed;

    requests->Clear();
    requests->PushBack(CreateRunRequest("component2", cSubject1, 0, 0, "", {}, UpdateItemTypeEnum::eComponent));
    requests->PushBack(CreateRunRequest(cService2, cSubject1, 0, 1));

    ASSERT_TRUE(loader->Save(*requests).IsNone());

    loader->CreateInstances(*nodes, *instances);

    EXPECT_TRUE(instances->IsEmpty());
    ASSERT_EQ(mInstanceManager->GetCachedInstances().Size(), 1);
    EXPECT_EQ(mInstanceManager->GetCachedInstances()[0]->GetInfo().mInstanceIdent.mItemID, cService1);

    // Run requests are restored from storage.
    ASSERT_TRUE(loader->Load().IsNone());
}

/***********************************************************************************************************************
 * Launcher error tests
 **********************************************************************************************************************/

class CMLauncherErrorTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        mStorage.Init();
        mStorageState.Init();
        mImageStore.Init();
        mMonitoringProvider.Init();
        mResourceManager.Init();
    }

    Error InitLauncher()
    {
        return mLauncher->Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
            mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider, ValidateGID,
            ValidateUID, mStorage, mSender);
    }

    FailingAllocator                                 mAllocator;
    NiceMock<nodeinfoprovider::NodeInfoProviderMock> mNodeInfoProvider;
    NiceMock<InstanceRunnerMock>                     mInstanceRunner;
    imagemanager::ImageStoreStub                     mImageStore;
    resourcemanager::ResourceManagerStub             mResourceManager;
    FaultyStorageStateStub                           mStorageState;
    CountingMonitoringProvider                       mMonitoringProvider;
    NiceMock<AlertsProviderMock>                     mAlertsProvider;
    NiceMock<iamclient::IdentProviderMock>           mIdentProvider;
    FaultyStorageStub                                mStorage;
    SenderStub                                       mSender;
    std::unique_ptr<Launcher>                        mLauncher = std::make_unique<Launcher>();
};

TEST_F(CMLauncherErrorTest, StartFailsOnInstanceManagerError)
{
    mStorage.mLoadInstancesErr = ErrorEnum::eFailed;

    ASSERT_TRUE(InitLauncher().IsNone());
    EXPECT_TRUE(mLauncher->Start().Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherErrorTest, StartFailsOnNodeManagerError)
{
    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_)).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(InitLauncher().IsNone());
    EXPECT_TRUE(mLauncher->Start().Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherErrorTest, StartFailsOnSubscribeNodeInfoError)
{
    EXPECT_CALL(mNodeInfoProvider, SubscribeListener(_)).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(InitLauncher().IsNone());
    EXPECT_TRUE(mLauncher->Start().Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherErrorTest, StartFailsOnSubscribeSubjectsError)
{
    EXPECT_CALL(mIdentProvider, SubscribeListener(_)).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(InitLauncher().IsNone());
    EXPECT_TRUE(mLauncher->Start().Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherErrorTest, StartFailsOnSubscribeAlertsError)
{
    EXPECT_CALL(mAlertsProvider, SubscribeListener(_, _)).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(InitLauncher().IsNone());
    EXPECT_TRUE(mLauncher->Start().Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherErrorTest, StartFailsOnGetSubjectsError)
{
    EXPECT_CALL(mIdentProvider, GetSubjects(_)).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(InitLauncher().IsNone());
    EXPECT_TRUE(mLauncher->Start().Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherErrorTest, StartFailsOnLoadRunRequestsError)
{
    mStorage.mLoadRunRequestsErr = ErrorEnum::eFailed;

    ASSERT_TRUE(InitLauncher().IsNone());
    EXPECT_TRUE(mLauncher->Start().Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherErrorTest, StartFailsOnLoadOverrideEnvVarsError)
{
    mStorage.mLoadEnvVarsErr = ErrorEnum::eFailed;

    ASSERT_TRUE(InitLauncher().IsNone());
    EXPECT_TRUE(mLauncher->Start().Is(ErrorEnum::eFailed));
}

TEST_F(CMLauncherErrorTest, StopErrors)
{
    EXPECT_CALL(mIdentProvider, UnsubscribeListener(_))
        .WillOnce(Return(ErrorEnum::eFailed))
        .WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mAlertsProvider, UnsubscribeListener(_))
        .WillOnce(Return(ErrorEnum::eFailed))
        .WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mNodeInfoProvider, UnsubscribeListener(_))
        .WillOnce(Return(ErrorEnum::eFailed))
        .WillRepeatedly(Return(ErrorEnum::eNone));

    ASSERT_TRUE(InitLauncher().IsNone());
    ASSERT_TRUE(mLauncher->Start().IsNone());

    EXPECT_TRUE(mLauncher->Stop().Is(ErrorEnum::eFailed));
    EXPECT_TRUE(mLauncher->Stop().Is(ErrorEnum::eFailed));
    EXPECT_TRUE(mLauncher->Stop().Is(ErrorEnum::eFailed));
    EXPECT_TRUE(mLauncher->Stop().IsNone());
}

TEST_F(CMLauncherErrorTest, Listeners)
{
    nodeinfoprovider::NodeInfoListenerItf* nodeInfoListener {};
    alerts::AlertsListenerItf*             alertsListener {};

    EXPECT_CALL(mNodeInfoProvider, SubscribeListener(_))
        .WillOnce(Invoke([&nodeInfoListener](nodeinfoprovider::NodeInfoListenerItf& listener) {
            nodeInfoListener = &listener;

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mAlertsProvider, SubscribeListener(_, _))
        .WillOnce(Invoke([&alertsListener](const Array<AlertTag>&, alerts::AlertsListenerItf& listener) {
            alertsListener = &listener;

            return ErrorEnum::eNone;
        }));

    ASSERT_TRUE(InitLauncher().IsNone());

    // Run requests are not saved.
    auto runRequests = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    runRequests->PushBack(CreateRunRequest(cService1, cSubject1));

    mStorage.mSaveRunRequestsErr = ErrorEnum::eFailed;

    EXPECT_TRUE(mLauncher->RunInstances(*runRequests, *runStatuses).Is(ErrorEnum::eFailed));

    // Launcher is not started.
    mStorage.mSaveRunRequestsErr = ErrorEnum::eNone;

    EXPECT_TRUE(mLauncher->RunInstances(*runRequests, *runStatuses).Is(ErrorEnum::eCanceled));

    InstanceStatusReceiverItf& statusReceiver = *mLauncher;

    EXPECT_TRUE(statusReceiver.OnNodeInstancesStatusesReceived(cNodeIDLocalSM, Array<InstanceStatus>()).IsNone());

    EXPECT_TRUE(statusReceiver
                    .OnInstanceStatusReceived(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0),
                        cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone))
                    .IsNone());

    InstanceStatusListenerStub statusListener;

    EXPECT_TRUE(mLauncher->UnsubscribeListener(statusListener).Is(ErrorEnum::eNotFound));

    ASSERT_TRUE(mLauncher->Start().IsNone());
    ASSERT_NE(nodeInfoListener, nullptr);
    ASSERT_NE(alertsListener, nullptr);

    // Not system quota alert is ignored.
    AlertVariant alert;

    alert.SetValue(CoreAlert {});

    EXPECT_TRUE(alertsListener->OnAlertReceived(alert).IsNone());

    nodeInfoListener->OnNodeInfoChanged(CreateNodeInfo(
        cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {}, NodeStateEnum::eUnprovisioned, false));

    EXPECT_TRUE(mLauncher->Stop().IsNone());
}

/***********************************************************************************************************************
 * Launcher balancing tests
 **********************************************************************************************************************/

TEST_F(CMLauncherTest, RunInstancesFailsOnMonitoringError)
{
    mStorageState.Init();
    mNodeInfoProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mImageStore.Init();
    mStorage.Init();

    mNodeInfoProvider.AddNodeInfo(
        cNodeIDLocalSM, CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}));

    auto nodeConfig = std::make_unique<NodeConfig>();

    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    oci::ItemConfig itemConfig;

    CreateItemConfig(itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, itemConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher);

    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    auto runRequests = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    runRequests->PushBack(CreateRunRequest(cService1, cSubject1));

    EXPECT_TRUE(mLauncher.RunInstances(*runRequests, *runStatuses).Is(ErrorEnum::eNotFound));

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, ScheduleErrors)
{
    mStorageState.Init();
    mNodeInfoProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mImageStore.Init();
    mStorage.Init();

    mNodeInfoProvider.AddNodeInfo(
        cNodeIDLocalSM, CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc, 1)}));

    auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();

    CreateNodeMonitoring(*nodeMonitoring, cNodeIDLocalSM, 0.0);
    mMonitoringProvider.SetAverageMonitoring(cNodeIDLocalSM, *nodeMonitoring);

    auto nodeConfig = std::make_unique<NodeConfig>();

    CreateNodeConfig(*nodeConfig, cNodeIDLocalSM);
    mResourceManager.SetNodeConfig(cNodeIDLocalSM, cNodeTypeVM, *nodeConfig);

    auto itemConfig = std::make_unique<oci::ItemConfig>();

    oci::ResourceInfo resource;

    resource.mName = "unknownResource";

    CreateItemConfig(*itemConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled, {}, {}, {}, {}, {resource});
    AddItem(cService1, cImageID1, *itemConfig, CreateImageConfig());

    *itemConfig = oci::ItemConfig {};
    CreateItemConfig(*itemConfig, {cRunnerRunx});
    AddItem(cService2, cImageID1, *itemConfig, CreateImageConfig());

    *itemConfig = oci::ItemConfig {};
    CreateItemConfig(
        *itemConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled, {}, CreateRequestedResources(0, 0, 5000, 0));
    AddItem(cService3, cImageID1, *itemConfig, CreateImageConfig());

    *itemConfig = oci::ItemConfig {};
    CreateItemConfig(
        *itemConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eEnabled, {}, CreateRequestedResources(0, 0, 0, 5000));
    AddItem("service4", cImageID1, *itemConfig, CreateImageConfig());

    // Max number of instances reached for the second one.
    *itemConfig = oci::ItemConfig {};
    CreateItemConfig(*itemConfig, {cRunnerRunc});
    AddItem("service5", cImageID1, *itemConfig, CreateImageConfig());
    AddItem("service6", cImageID1, *itemConfig, CreateImageConfig());

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    auto runRequests = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    for (const auto& serviceID : {cService1, cService2, cService3, "service4", "service5", "service6"}) {
        runRequests->PushBack(CreateRunRequest(serviceID, cSubject1));
    }

    ASSERT_TRUE(mLauncher.RunInstances(*runRequests, *runStatuses).IsNone());
    ASSERT_TRUE(mLauncher.Stop().IsNone());

    ASSERT_EQ(runStatuses->Size(), 6);

    size_t numFailed = 0;

    for (const auto& status : *runStatuses) {
        if (status.mState == aos::InstanceStateEnum::eFailed) {
            EXPECT_TRUE(status.mError.Is(ErrorEnum::eNotFound)) << status.mItemID.CStr();
            numFailed++;
        } else {
            EXPECT_EQ(status.mNodeID, cNodeIDLocalSM) << status.mItemID.CStr();
        }
    }

    EXPECT_EQ(numFailed, 5);
}

TEST_F(CMLauncherTest, PolicyRebalancingOnNodeInfoChange)
{
    using namespace std::chrono_literals;

    mStorageState.Init();
    mNodeInfoProvider.Init();
    mMonitoringProvider.Init();
    mResourceManager.Init();
    mImageStore.Init();
    mStorage.Init();

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1}) {
        auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();

        CreateNodeMonitoring(*nodeMonitoring, nodeID, 0.0);
        mMonitoringProvider.SetAverageMonitoring(nodeID, *nodeMonitoring);

        auto nodeConfig = std::make_unique<NodeConfig>();

        CreateNodeConfig(*nodeConfig, nodeID);
        mResourceManager.SetNodeConfig(nodeID, cNodeTypeVM, *nodeConfig);
    }

    mNodeInfoProvider.AddNodeInfo(
        cNodeIDLocalSM, CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)}));

    oci::ItemConfig itemConfig;

    CreateItemConfig(itemConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eDisabled);

    for (const auto& serviceID : {cService1, cService2, cService3, "service4"}) {
        AddItem(serviceID, cImageID1, itemConfig, CreateImageConfig());
    }

    std::mutex              mutex;
    std::condition_variable condVar;
    size_t                  numRunRequests = 0;

    EXPECT_CALL(mInstanceRunner, OnRunRequest()).WillRepeatedly(Invoke([&]() {
        std::lock_guard lock {mutex};

        numRunRequests++;
        condVar.notify_all();
    }));

    mInstanceRunner.Init(mLauncher, true, aos::InstanceStateEnum::eActive);

    ASSERT_TRUE(mLauncher
                    .Init(mAllocator, CreateConfig(), mNodeInfoProvider, mInstanceRunner, mImageStore, mImageStore,
                        mResourceManager, mStorageState, mMonitoringProvider, mAlertsProvider, mIdentProvider,
                        ValidateGID, ValidateUID, mStorage, mSender)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mInstanceRunner.SendInitialStatuses(cNodeIDLocalSM);

    auto runRequests = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    for (const auto& serviceID : {cService1, cService2, cService3, "service4"}) {
        runRequests->PushBack(CreateRunRequest(serviceID, cSubject1));
    }

    ASSERT_TRUE(mLauncher.RunInstances(*runRequests, *runStatuses).IsNone());

    size_t expectedRunRequests = 0;

    {
        std::lock_guard lock {mutex};

        expectedRunRequests = numRunRequests + 2;
    }

    // Break images of running instances: unknown index, broken item config and removed manifest.
    mImageStore.SetBlobError(imagemanager::ImageStoreStub::BuildIndexDigest(cService2, ""), ErrorEnum::eFailed);
    mImageStore.SetBlobError("service3:image1:item", ErrorEnum::eFailed);
    mImageStore.AddItem("service4", "image2", itemConfig, CreateImageConfig());

    // New node triggers rebalancing.
    mNodeInfoProvider.AddNodeInfo(
        cNodeIDRemoteSM1, CreateNodeInfo(cNodeIDRemoteSM1, 2000, 2048, {CreateRuntime(cRunnerRunc)}));
    mInstanceRunner.SendInitialStatuses(cNodeIDRemoteSM1);

    {
        std::unique_lock lock {mutex};

        ASSERT_TRUE(condVar.wait_for(lock, 5s, [&]() { return numRunRequests >= expectedRunRequests; }));
    }

    ASSERT_TRUE(mLauncher.Stop().IsNone());

    // Service with disabled balancing policy stays on its node.
    const auto& nodeRequests = mInstanceRunner.GetRunRequests();

    ASSERT_EQ(nodeRequests.count(cNodeIDLocalSM), 1);
    ASSERT_EQ(nodeRequests.count(cNodeIDRemoteSM1), 1);

    const auto& localStartInstances = nodeRequests.at(cNodeIDLocalSM).mStartInstances;

    EXPECT_TRUE(std::any_of(localStartInstances.begin(), localStartInstances.end(),
        [](const aos::InstanceInfo& info) { return info.mItemID == cService1; }));
}

/***********************************************************************************************************************
 * ImageInfoProvider tests
 **********************************************************************************************************************/

class CMLauncherImageInfoProviderTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        mImageStore.Init();

        oci::ItemConfig itemConfig;

        CreateItemConfig(itemConfig, {cRunnerRunc});
        mImageStore.AddItem(cService1, cImageID1, itemConfig, CreateImageConfig());

        mProvider.Init(mAllocator, mImageStore, mImageStore);
    }

    FailingAllocator     mAllocator;
    FaultyImageStoreStub mImageStore;
    ImageInfoProvider    mProvider;
};

TEST_F(CMLauncherImageInfoProviderTest, GetImageConfigErrors)
{
    using PathType = StaticString<cFilePathLen>;

    const auto descriptor = CreateImageDescriptor(cService1);
    auto       config     = std::make_unique<oci::ImageConfig>();

    ASSERT_TRUE(mProvider.GetImageConfig(descriptor, *config).IsNone());
    EXPECT_EQ(config->mArchitecture, "x86_64");

    mAllocator.Fail(sizeof(PathType));
    EXPECT_TRUE(mProvider.GetImageConfig(descriptor, *config).Is(ErrorEnum::eNoMemory));

    mAllocator.Fail(sizeof(oci::ImageManifest));
    EXPECT_TRUE(mProvider.GetImageConfig(descriptor, *config).Is(ErrorEnum::eNoMemory));

    mAllocator.Fail(sizeof(PathType), 1);
    EXPECT_TRUE(mProvider.GetImageConfig(descriptor, *config).Is(ErrorEnum::eNoMemory));

    EXPECT_TRUE(mProvider.GetImageConfig(CreateImageDescriptor(cService2), *config).Is(ErrorEnum::eNotFound));

    mImageStore.mManifestErr = ErrorEnum::eFailed;
    EXPECT_TRUE(mProvider.GetImageConfig(descriptor, *config).Is(ErrorEnum::eFailed));
    mImageStore.mManifestErr = ErrorEnum::eNone;

    mImageStore.mImageConfigErr = ErrorEnum::eWrongState;
    EXPECT_TRUE(mProvider.GetImageConfig(descriptor, *config).Is(ErrorEnum::eWrongState));
}

TEST_F(CMLauncherImageInfoProviderTest, GetItemConfigErrors)
{
    using PathType = StaticString<cFilePathLen>;

    const auto descriptor = CreateImageDescriptor(cService1);
    auto       config     = std::make_unique<oci::ItemConfig>();

    ASSERT_TRUE(mProvider.GetItemConfig(descriptor, *config).IsNone());
    ASSERT_EQ(config->mRuntimes.Size(), 1);
    EXPECT_EQ(config->mRuntimes[0], cRunnerRunc);

    mAllocator.Fail(sizeof(PathType));
    EXPECT_TRUE(mProvider.GetItemConfig(descriptor, *config).Is(ErrorEnum::eNoMemory));

    mAllocator.Fail(sizeof(oci::ImageManifest));
    EXPECT_TRUE(mProvider.GetItemConfig(descriptor, *config).Is(ErrorEnum::eNoMemory));

    mAllocator.Fail(sizeof(PathType), 1);
    EXPECT_TRUE(mProvider.GetItemConfig(descriptor, *config).Is(ErrorEnum::eNoMemory));

    mImageStore.mManifestErr = ErrorEnum::eFailed;
    EXPECT_TRUE(mProvider.GetItemConfig(descriptor, *config).Is(ErrorEnum::eFailed));
    mImageStore.mManifestErr = ErrorEnum::eNone;

    // Manifest without item config.
    mImageStore.mNoItemConfig = true;
    EXPECT_TRUE(mProvider.GetItemConfig(descriptor, *config).Is(ErrorEnum::eNotFound));
    mImageStore.mNoItemConfig = false;

    mImageStore.mItemConfigErr = ErrorEnum::eWrongState;
    EXPECT_TRUE(mProvider.GetItemConfig(descriptor, *config).Is(ErrorEnum::eWrongState));
}

TEST_F(CMLauncherImageInfoProviderTest, GetImageIndexErrors)
{
    const auto indexDigest = imagemanager::ImageStoreStub::BuildIndexDigest(cService1, "");
    auto       index       = std::make_unique<oci::ImageIndex>();

    ASSERT_TRUE(mProvider.GetImageIndex(indexDigest, *index).IsNone());
    EXPECT_EQ(index->mManifests.Size(), 1);

    EXPECT_TRUE(mProvider.GetImageIndex("", *index).Is(ErrorEnum::eInvalidArgument));

    mAllocator.Fail(sizeof(StaticString<cFilePathLen>));
    EXPECT_TRUE(mProvider.GetImageIndex(indexDigest, *index).Is(ErrorEnum::eNoMemory));

    EXPECT_TRUE(mProvider.GetImageIndex("unknown", *index).Is(ErrorEnum::eNotFound));

    mImageStore.mIndexErr = ErrorEnum::eFailed;
    EXPECT_TRUE(mProvider.GetImageIndex(indexDigest, *index).Is(ErrorEnum::eFailed));
}

/***********************************************************************************************************************
 * OverrideEnvVarsProcessor tests
 **********************************************************************************************************************/

class OverrideEnvVarsListenerCounter : public OverrideEnvVarsListenerItf {
public:
    void OnOverrideEnvVarsChanged() override { mCount++; }

    std::atomic<size_t> mCount {};
};

class CMLauncherOverrideEnvVarsTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        mStorage.Init();
    }

    Error InitProcessor(Duration checkPeriod = Time::cMinutes)
    {
        auto config = CreateConfig();

        config.mCheckOverrideEnvVarsPeriod = checkPeriod;

        return mProcessor->Init(config, mStorage, mSender, mListener);
    }

    static std::unique_ptr<OverrideEnvVarsRequest> CreateRequest(const Optional<Time>& ttl = {})
    {
        auto                request = std::make_unique<OverrideEnvVarsRequest>();
        EnvVarsInstanceInfo item;
        EnvVarInfo          var;

        item.mItemID.EmplaceValue(cService1);
        static_cast<EnvVar&>(var) = CreateEnvVar("VAR1", "value1");
        var.mTTL                  = ttl;
        item.mVariables.PushBack(var);
        request->mItems.PushBack(item);

        return request;
    }

    FaultyStorageStub                         mStorage;
    FaultySenderStub                          mSender;
    OverrideEnvVarsListenerCounter            mListener;
    std::unique_ptr<OverrideEnvVarsProcessor> mProcessor = std::make_unique<OverrideEnvVarsProcessor>();
};

TEST_F(CMLauncherOverrideEnvVarsTest, StartFailsOnSaveExpiredVars)
{
    ASSERT_TRUE(InitProcessor().IsNone());

    // Variable expired while offline.
    ASSERT_TRUE(mStorage.StorageStub::SaveOverrideEnvVars(*CreateRequest(Time::Now().Add(-Time::cHours))).IsNone());

    mStorage.mSaveEnvVarsErr = ErrorEnum::eFailed;

    auto [changed, err] = mProcessor->Start();
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed));
    EXPECT_TRUE(changed);

    // Expired variable is removed on successful start.
    mStorage.mSaveEnvVarsErr = ErrorEnum::eNone;

    auto [changedAgain, errAgain] = mProcessor->Start();
    EXPECT_TRUE(errAgain.IsNone());
    EXPECT_TRUE(changedAgain);
    EXPECT_TRUE((*mProcessor->GetOverrideEnvVars()).mItems.IsEmpty());

    EXPECT_TRUE(mProcessor->Stop().IsNone());
}

TEST_F(CMLauncherOverrideEnvVarsTest, OverrideEnvVarsSaveError)
{
    ASSERT_TRUE(InitProcessor().IsNone());

    auto request = CreateRequest(Time::Now().Add(Time::cHours));

    mStorage.mSaveEnvVarsErr = ErrorEnum::eFailed;

    auto [changed, err] = mProcessor->OverrideEnvVars(*request);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed));
    EXPECT_TRUE(changed);
    EXPECT_EQ(mListener.mCount, 0);

    mStorage.mSaveEnvVarsErr = ErrorEnum::eNone;

    // Variables are already applied, so nothing is changed.
    auto [changedAgain, errAgain] = mProcessor->OverrideEnvVars(*request);
    EXPECT_TRUE(errAgain.IsNone());
    EXPECT_FALSE(changedAgain);
    EXPECT_EQ(mListener.mCount, 0);
}

TEST_F(CMLauncherOverrideEnvVarsTest, TTLTimerSaveError)
{
    ASSERT_TRUE(InitProcessor(10 * Time::cMilliseconds).IsNone());

    ASSERT_TRUE(mStorage.StorageStub::SaveOverrideEnvVars(*CreateRequest(Time::Now().Add(Time::cSeconds))).IsNone());

    auto [changed, err] = mProcessor->Start();
    ASSERT_TRUE(err.IsNone());
    EXPECT_FALSE(changed);

    auto saveCount = mStorage.GetSaveEnvVarsCount();

    mStorage.mSaveEnvVarsErr = ErrorEnum::eFailed;

    // Timer detects expired variable and fails to save the update.
    EXPECT_TRUE(mStorage.WaitForSaveEnvVarsCount(saveCount + 1));
    EXPECT_TRUE(mProcessor->Stop().IsNone());

    EXPECT_EQ(mListener.mCount, 0);
    EXPECT_TRUE((*mProcessor->GetOverrideEnvVars()).mItems.IsEmpty());
}

TEST_F(CMLauncherOverrideEnvVarsTest, SendStatuses)
{
    ASSERT_TRUE(InitProcessor().IsNone());

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    statuses->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), cNodeIDLocalSM, cRunnerRunc,
        aos::InstanceStateEnum::eActive, ErrorEnum::eNone));
    statuses->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService2, cSubject1, 0), cNodeIDLocalSM, cRunnerRunc,
        aos::InstanceStateEnum::eActive, ErrorEnum::eNone));
    (*statuses)[1].mEnvVarsStatuses.PushBack(EnvVarStatus {"VAR1", ErrorEnum::eNone});

    ASSERT_TRUE(mProcessor->AddStatuses(cNodeIDLocalSM, *statuses).IsNone());

    mProcessor->SendStatuses();

    EXPECT_EQ(mSender.mSendCount, 1);
    EXPECT_EQ(mSender.GetOverrideEnvVarsStatuses(),
        CreateOverrideEnvVarsStatuses({CreateEnvVarsInstanceStatus(
            CreateInstanceIdent(cService2, cSubject1, 0), {EnvVarStatus {"VAR1", ErrorEnum::eNone}})}));

    // Statuses of the same node are replaced, unchanged statuses are not sent.
    ASSERT_TRUE(mProcessor->AddStatuses(cNodeIDLocalSM, *statuses).IsNone());

    mProcessor->SendStatuses();

    EXPECT_EQ(mSender.mSendCount, 1);

    ASSERT_TRUE(mProcessor->AddStatuses(cNodeIDRemoteSM1, *statuses).IsNone());

    mSender.mSendErr = ErrorEnum::eFailed;

    mProcessor->SendStatuses();

    EXPECT_EQ(mSender.mSendCount, 1);

    // Statuses are sent after error.
    mSender.mSendErr = ErrorEnum::eNone;

    mProcessor->SendStatuses();

    EXPECT_EQ(mSender.mSendCount, 2);
    EXPECT_EQ(mSender.GetOverrideEnvVarsStatuses().mStatuses.Size(), 2);
}

/***********************************************************************************************************************
 * Allocation error tests
 **********************************************************************************************************************/

TEST_F(CMLauncherNodeTest, SendInstancesNoMemory)
{
    using InstanceInfoArray = StaticArray<aos::InstanceInfo, cMaxNumInstances>;

    auto running = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    auto active  = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    for (size_t skip = 0; skip < 2; skip++) {
        mAllocator.Fail(sizeof(InstanceInfoArray), skip);

        EXPECT_TRUE(mNode.SendScheduledInstances(*active, *running).Is(ErrorEnum::eNoMemory));

        mAllocator.Fail(sizeof(InstanceInfoArray), skip);

        auto [sent, err] = mNode.ResendInstances(*active, *running, false);
        EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory));
        EXPECT_FALSE(sent);
    }

    // Nothing to resend.
    auto [sent, err] = mNode.ResendInstances(*active, *running, false);
    EXPECT_TRUE(err.IsNone());
    EXPECT_FALSE(sent);
}

TEST_F(CMLauncherInstanceTest, AllocationErrors)
{
    oci::ItemConfig itemConfig;

    CreateItemConfig(itemConfig, {cRunnerRunc});
    AddItem(cService1, cImageID1, itemConfig);

    auto instance = CreateService(
        CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), BuildManifestDigest(cService1)));
    ASSERT_TRUE(instance->Init().IsNone());

    const auto descriptor = CreateImageDescriptor(cService1);

    EXPECT_TRUE(instance->IsImageValid());

    mAllocator.Fail(sizeof(oci::ImageIndex));
    EXPECT_FALSE(instance->IsImageValid());

    mAllocator.Fail(sizeof(oci::ItemConfig));
    EXPECT_TRUE(instance->LoadConfigs(descriptor).Is(ErrorEnum::eNoMemory));

    mAllocator.Fail(sizeof(oci::ImageConfig));
    EXPECT_TRUE(instance->LoadConfigs(descriptor).Is(ErrorEnum::eNoMemory));

    ASSERT_TRUE(instance->LoadConfigs(descriptor).IsNone());

    auto request = std::make_unique<OverrideEnvVarsRequest>();

    mAllocator.Fail(sizeof(EnvVarArray));

    auto [changed, err] = instance->OverrideEnvVars(*request);
    EXPECT_TRUE(err.Is(ErrorEnum::eNoMemory));
    EXPECT_FALSE(changed);

    for (size_t i = 0; i < 2; i++) {
        EnvVarsInstanceInfo item;

        item.mItemID.EmplaceValue(cService1);

        for (size_t j = 0; j < cMaxNumEnvVariables; j++) {
            EnvVarInfo var;

            static_cast<EnvVar&>(var)
                = CreateEnvVar("VAR_" + std::to_string(i) + "_" + std::to_string(j), std::to_string(j));
            ASSERT_TRUE(item.mVariables.PushBack(var).IsNone());
        }

        ASSERT_TRUE(request->mItems.PushBack(item).IsNone());
    }

    auto [changedMany, errMany] = instance->OverrideEnvVars(*request);
    EXPECT_TRUE(errMany.Is(ErrorEnum::eNoMemory));
    EXPECT_FALSE(changedMany);
}

TEST_F(CMLauncherInstanceManagerTest, AllocationErrors)
{
    mAllocator.Fail(sizeof(StaticArray<InstanceInfo, cMaxNumInstances>));

    EXPECT_TRUE(mInstanceManager->Start().Is(ErrorEnum::eNoMemory));
    ASSERT_TRUE(mInstanceManager->Stop().IsNone());
    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    auto request      = CreateRunRequest(cService1, cSubject1);
    auto newInstances = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    mAllocator.Fail(sizeof(InstanceInfo));

    auto [instance1, err1] = mInstanceManager->CreateInstance(request, 0);
    EXPECT_TRUE(err1.Is(ErrorEnum::eNoMemory));
    EXPECT_FALSE(instance1);

    mAllocator.Fail(sizeof(InstanceInfo));

    auto [instance2, err2] = mInstanceManager->CreateInstance(request, cNodeIDLocalSM, cRunnerRunc, *newInstances);
    EXPECT_TRUE(err2.Is(ErrorEnum::eNoMemory));
    EXPECT_FALSE(instance2);

    mAllocator.Fail(sizeof(SharedObjectControlBlock<ServiceInstance>));

    auto [instance3, err3] = mInstanceManager->CreateInstance(request, 0);
    EXPECT_TRUE(err3.Is(ErrorEnum::eNoMemory));
    EXPECT_FALSE(instance3);
}

TEST_F(CMLauncherInstanceManagerTest, ScheduledGeneratedInstanceIsReused)
{
    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    auto request      = CreateRunRequest(cComponent1, cSubject1, 0, 0, "", {}, UpdateItemTypeEnum::eComponent);
    auto newInstances = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    auto [instance, createErr]
        = mInstanceManager->CreateInstance(request, cNodeIDLocalSM, cRunnerRootfs, *newInstances);
    ASSERT_TRUE(createErr.IsNone());

    TestNode node;

    ASSERT_TRUE(mInstanceManager->ScheduleInstance(instance, node, cRunnerRootfs).IsNone());

    auto [scheduled, scheduledErr]
        = mInstanceManager->CreateInstance(request, cNodeIDLocalSM, cRunnerRootfs, *newInstances);
    ASSERT_TRUE(scheduledErr.IsNone());
    EXPECT_EQ(scheduled.Get(), instance.Get());

    request.mIndexDigest = "newIndexDigest";
    mStorage.mUpdateErr  = ErrorEnum::eFailed;

    auto [failed, updateErr] = mInstanceManager->CreateInstance(request, cNodeIDLocalSM, cRunnerRootfs, *newInstances);
    EXPECT_TRUE(updateErr.Is(ErrorEnum::eFailed));
    EXPECT_FALSE(failed);
}

TEST_F(CMLauncherInstanceManagerTest, PreinstalledComponentsAreReplaced)
{
    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    auto componentIdent = CreateInstanceIdent(cComponent1, "", 0, UpdateItemTypeEnum::eComponent);

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    statuses->PushBack(CreateInstanceStatus(
        componentIdent, cNodeIDLocalSM, cRunnerRootfs, aos::InstanceStateEnum::eActive, ErrorEnum::eNone, "", true));

    ASSERT_TRUE(mInstanceManager->UpdateRunningInstances(cNodeIDLocalSM, *statuses).IsNone());
    ASSERT_TRUE(mInstanceManager->UpdateRunningInstances(cNodeIDRemoteSM1, *statuses).IsNone());
    EXPECT_EQ(mInstanceManager->GetPreinstalledComponents().Size(), 1);

    statuses->Clear();

    ASSERT_TRUE(mInstanceManager->UpdateRunningInstances(cNodeIDLocalSM, *statuses).IsNone());
    EXPECT_TRUE(mInstanceManager->GetPreinstalledComponents().IsEmpty());
}

TEST_F(CMLauncherInstanceManagerTest, CacheIsClearedWhenLimitReached)
{
    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    auto request = CreateRunRequest(cService1, cSubject1);

    for (size_t i = 0; i < cMaxNumInstances; i++) {
        auto [instance, err] = mInstanceManager->CreateInstance(request, i);
        ASSERT_TRUE(err.IsNone()) << i;

        mInstanceManager->DisableInstance(instance);
    }

    ASSERT_EQ(mInstanceManager->GetCachedInstances().Size(), cMaxNumInstances);

    auto [instance, err] = mInstanceManager->CreateInstance(request, cMaxNumInstances);
    ASSERT_TRUE(err.IsNone());

    mInstanceManager->ScheduleInstance(instance, ErrorEnum::eFailed);

    // Remove errors are ignored.
    mStorage.mRemoveErr = ErrorEnum::eFailed;

    ASSERT_TRUE(mInstanceManager->SubmitScheduledInstances().IsNone());

    EXPECT_EQ(mInstanceManager->GetActiveInstances().Size(), 1);
    EXPECT_TRUE(mInstanceManager->GetCachedInstances().IsEmpty());
}

TEST_F(CMLauncherInstanceManagerTest, ExpiredActivatingInstancesFail)
{
    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), {}, cRunnerRunc,
                        cNodeIDLocalSM, InstanceStateEnum::eActive))
                    .IsNone());

    auto config = CreateConfig();

    config.mNodesConnectionTimeout = Time::cMilliseconds;

    auto instanceManager = std::make_unique<InstanceManager>();

    ASSERT_TRUE(
        instanceManager
            ->Init(mAllocator, config, mImageStore, mStorageState, mImageStore, ValidateGID, ValidateUID, mStorage)
            .IsNone());
    ASSERT_TRUE(instanceManager->Start().IsNone());

    // Instance status is not received during nodes connection timeout.
    ASSERT_TRUE(mStorage.WaitForUpdateCount(1));
    ASSERT_EQ(instanceManager->GetActiveInstances().Size(), 1);
    EXPECT_EQ(instanceManager->GetActiveInstances()[0]->GetStatus().mState, aos::InstanceStateEnum::eFailed);
    EXPECT_TRUE(instanceManager->GetActiveInstances()[0]->GetStatus().mError.Is(ErrorEnum::eFailed));
    EXPECT_TRUE(instanceManager->Stop().IsNone());
}

TEST_F(CMLauncherInstanceManagerTest, RunRequestsLoaderErrors)
{
    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    StaticArray<StaticString<cIDLen>, 1> subjects;

    subjects.PushBack(cSubject1);
    ASSERT_TRUE(mInstanceManager->SetSubjects(subjects).mError.IsNone());

    ImageInfoProvider imageInfoProvider;

    imageInfoProvider.Init(mAllocator, mImageStore, mImageStore);

    FailingAllocator loaderAllocator;
    auto             loader = std::make_unique<RunRequestsLoader>();

    loader->Init(loaderAllocator, mStorage, *mInstanceManager, imageInfoProvider);

    oci::ItemConfig itemConfig;

    CreateItemConfig(itemConfig, {cRunnerRootfs});
    mImageStore.AddItem(cComponent1, cImageID1, itemConfig, CreateImageConfig());

    resourcemanager::ResourceManagerStub nodeConfigProvider;
    NiceMock<InstanceRunnerMock>         runner;
    auto                                 nodes = std::make_unique<StaticArray<Node, 2>>();

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1}) {
        nodes->EmplaceBack();
        nodes->Back().Init(mAllocator, nodeID, nodeConfigProvider, runner);
        (void)nodes->Back().UpdateInfo(CreateNodeInfo(nodeID, 1000, 1024, {CreateRuntime(cRunnerRootfs)}));
    }

    auto tooManyRequests = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances + 1>>();

    ASSERT_TRUE(tooManyRequests->Resize(cMaxNumInstances + 1).IsNone());
    EXPECT_FALSE(loader->Save(*tooManyRequests).IsNone());

    // cppcheck-suppress templateRecursion
    auto requests  = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    auto instances = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    requests->PushBack(CreateRunRequest(cComponent1, cSubject1, 0, 0, "", {}, UpdateItemTypeEnum::eComponent));

    ASSERT_TRUE(loader->Save(*requests).IsNone());

    for (size_t skip = 0; skip < 3; skip++) {
        loaderAllocator.Fail(FailingAllocator::cAnySize, skip);

        loader->CreateInstances(*nodes, *instances);
        EXPECT_TRUE(instances->IsEmpty());
    }

    // Generated instances don't fit into result array.
    auto oneInstance = std::make_unique<StaticArray<SharedPtr<Instance>, 1>>();

    loader->CreateInstances(*nodes, *oneInstance);
    ASSERT_EQ(oneInstance->Size(), 1);
    EXPECT_EQ(oneInstance->Front()->GetInfo().mNodeID, cNodeIDLocalSM);

    // Service instances don't fit into result array.
    requests->Clear();
    requests->PushBack(CreateRunRequest(cService1, cSubject1, 0, 2));

    ASSERT_TRUE(loader->Save(*requests).IsNone());

    loader->CreateInstances(*nodes, *oneInstance);
    ASSERT_EQ(oneInstance->Size(), 1);
    EXPECT_EQ(oneInstance->Front()->GetInfo().mInstanceIdent, CreateInstanceIdent(cService1, cSubject1, 0));
}

TEST_F(CMLauncherNodeManagerTest, StartAllocationErrors)
{
    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_)).WillRepeatedly(Invoke([](Array<StaticString<cIDLen>>& ids) {
        return ids.PushBack(cNodeIDLocalSM);
    }));
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(_, _))
        .WillRepeatedly(Invoke([](const String& nodeID, UnitNodeInfo& info) {
            info = CreateNodeInfo(nodeID.CStr(), 1000, 1024, {CreateRuntime(cRunnerRunc)});

            return ErrorEnum::eNone;
        }));

    mAllocator.Fail(sizeof(StaticArray<StaticString<cIDLen>, cMaxNumNodes>));
    EXPECT_TRUE(mNodeManager->Start().Is(ErrorEnum::eNoMemory));

    mAllocator.Fail(sizeof(UnitNodeInfo));
    EXPECT_TRUE(mNodeManager->Start().Is(ErrorEnum::eNoMemory));

    ASSERT_TRUE(mNodeManager->Start().IsNone());
    EXPECT_EQ(mNodeManager->GetNodes().Size(), 1);
}

TEST_F(CMLauncherNodeManagerTest, TooManyNodes)
{
    for (size_t i = 0; i < cMaxNumNodes; i++) {
        auto nodeID = "node" + std::to_string(i);

        ASSERT_TRUE(mNodeManager->UpdateNodeInfo(CreateNodeInfo(nodeID, 1000, 1024, {CreateRuntime(cRunnerRunc)})));
    }

    EXPECT_FALSE(mNodeManager->UpdateNodeInfo(CreateNodeInfo("extraNode", 1000, 1024, {CreateRuntime(cRunnerRunc)})));
    EXPECT_FALSE(mNodeManager->NotifyNodeStatusReceived("extraNode").IsNone());
    EXPECT_EQ(mNodeManager->FindNode("extraNode"), nullptr);

    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_)).WillOnce(Invoke([](Array<StaticString<cIDLen>>& ids) {
        return ids.PushBack("extraNode");
    }));

    EXPECT_FALSE(mNodeManager->Start().IsNone());
}

TEST_F(CMLauncherNodeManagerTest, LoadSMDataAllocationErrors)
{
    ASSERT_TRUE(
        mNodeManager->UpdateNodeInfo(CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRootfs)})));

    oci::ItemConfig itemConfig;

    CreateItemConfig(itemConfig, {cRunnerRootfs});
    mImageStore.AddItem(cComponent1, cImageID1, itemConfig, CreateImageConfig());

    auto instances = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    instances->PushBack(CreateComponent(cComponent1, BuildManifestDigest(cComponent1).CStr()));

    for (auto size : {sizeof(oci::IndexContentDescriptor), sizeof(oci::ImageIndex)}) {
        mAllocator.Fail(size);

        EXPECT_TRUE(mNodeManager->LoadSMDataForActiveInstances(*instances, mImageInfoProvider).IsNone());
        EXPECT_TRUE(instances->Front()->GetSMInfo().mRuntimeID.IsEmpty());
    }

    EXPECT_TRUE(mNodeManager->LoadSMDataForActiveInstances(*instances, mImageInfoProvider).IsNone());
    EXPECT_EQ(instances->Front()->GetSMInfo().mRuntimeID, cRunnerRootfs);
}

TEST_F(CMLauncherNodeManagerTest, OverrideEnvVarsErrors)
{
    ASSERT_TRUE(mNodeManager->UpdateNodeInfo(CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)})));
    ASSERT_TRUE(mNodeManager->NotifyNodeStatusReceived(cNodeIDLocalSM).IsNone());

    auto running = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    auto active  = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    active->PushBack(CreateComponent(cComponent1, ""));
    active->PushBack(CreateComponent("component2", ""));

    Mutex             mutex;
    UniqueLock<Mutex> lock {mutex};

    mAllocator.Fail(sizeof(EnvVarArray), 0, 2);
    EXPECT_TRUE(mNodeManager->SendScheduledInstances(lock, *active, *running).Is(ErrorEnum::eNoMemory));

    StaticArray<StaticString<cIDLen>, 1> updatedNodes;

    updatedNodes.PushBack(cNodeIDLocalSM);

    mAllocator.Fail(sizeof(EnvVarArray));
    EXPECT_TRUE(mNodeManager->ResendInstances(lock, updatedNodes, *active, *running).Is(ErrorEnum::eNoMemory));
}

TEST_F(CMLauncherNodeManagerTest, UnprovisionedNodeIsNotWaitedForStatus)
{
    auto nodeInfo = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)});

    ASSERT_TRUE(mNodeManager->UpdateNodeInfo(nodeInfo));
    ASSERT_TRUE(mNodeManager->NotifyNodeStatusReceived(cNodeIDLocalSM).IsNone());

    EXPECT_CALL(mRunner, UpdateInstances(String(cNodeIDLocalSM), _, _)).WillOnce(Return(ErrorEnum::eNone));

    auto running = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();
    auto active  = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

    Mutex             mutex;
    UniqueLock<Mutex> lock {mutex};

    // Node becomes unprovisioned while node manager waits for its status.
    std::thread thread([&]() {
        LockGuard<Mutex> threadLock {mutex};

        nodeInfo.mState = NodeStateEnum::eUnprovisioned;

        EXPECT_TRUE(mNodeManager->UpdateNodeInfo(nodeInfo));
    });

    EXPECT_TRUE(mNodeManager->SendScheduledInstances(lock, *active, *running).IsNone());

    (void)lock.Unlock();
    thread.join();

    EXPECT_EQ(mNodeManager->FindNode(cNodeIDLocalSM)->GetInfo().mState, NodeStateEnum::eUnprovisioned);
}

/***********************************************************************************************************************
 * Balancer tests
 **********************************************************************************************************************/

class CMLauncherBalancerTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        mImageStore.Init();
        mStorage.Init();
        mStorageState.Init();
        mMonitoringProvider.Init();
        mNodeConfigProvider.Init();

        ON_CALL(mRunner, UpdateInstances(_, _, _)).WillByDefault(Return(ErrorEnum::eFailed));

        ASSERT_TRUE(mInstanceManager
                        ->Init(mAllocator, CreateConfig(), mImageStore, mStorageState, mImageStore, ValidateGID,
                            ValidateUID, mStorage)
                        .IsNone());

        mImageInfoProvider.Init(mAllocator, mImageStore, mImageStore);

        ASSERT_TRUE(mOverrideEnvVarsProcessor.Init(CreateConfig(), mStorage, mSender, mListener).IsNone());

        mNodeManager->Init(mAllocator, mNodeInfoProvider, mNodeConfigProvider, mRunner, mOverrideEnvVarsProcessor);
        mBalancer->Init(
            mBalancerAllocator, *mInstanceManager, mImageInfoProvider, *mNodeManager, mMonitoringProvider, mRunner);

        oci::ItemConfig itemConfig;

        CreateItemConfig(itemConfig, {cRunnerRunc}, oci::BalancingPolicyEnum::eDisabled);

        for (const auto& itemID :
            {cService1, cService2, cService3, "service4", "service5", "service6", "service7", "service8"}) {
            mImageStore.AddItem(itemID, cImageID1, itemConfig, CreateImageConfig());
        }
    }

    void TearDown() override
    {
        EXPECT_TRUE(mNodeManager->Stop().IsNone());
        EXPECT_TRUE(mInstanceManager->Stop().IsNone());
    }

    void AddNode(const std::string& nodeID, const std::vector<RuntimeInfo>& runtimes,
        NodeState state = NodeStateEnum::eProvisioned, bool isConnected = true)
    {
        ASSERT_TRUE(mNodeManager->UpdateNodeInfo(CreateNodeInfo(nodeID, 1000, 1024, runtimes, {}, state, isConnected)));
        ASSERT_TRUE(mNodeManager->NotifyNodeStatusReceived(nodeID.c_str()).IsNone());

        auto nodeMonitoring = std::make_unique<monitoring::NodeMonitoringData>();

        CreateNodeMonitoring(*nodeMonitoring, nodeID, 0.0);
        mMonitoringProvider.SetAverageMonitoring(nodeID.c_str(), *nodeMonitoring);

        auto nodeConfig = std::make_unique<NodeConfig>();

        CreateNodeConfig(*nodeConfig, nodeID);
        mNodeConfigProvider.SetNodeConfig(nodeID.c_str(), cNodeTypeVM, *nodeConfig);
    }

    void AddInstance(const std::string& itemID, const std::string& nodeID, const std::string& runtimeID,
        bool withManifest = true, bool disableRebalancing = false)
    {
        auto info = CreateInstanceInfo(CreateInstanceIdent(itemID, cSubject1, 0),
            withManifest ? BuildManifestDigest(itemID) : StaticString<oci::cDigestLen>(), runtimeID.c_str(),
            nodeID.c_str(), InstanceStateEnum::eActive);

        info.mDisableRebalancing = disableRebalancing;

        ASSERT_TRUE(mStorage.AddInstance(info).IsNone());
    }

    Error RunInstances(bool rebalancing)
    {
        auto instances = std::make_unique<StaticArray<SharedPtr<Instance>, cMaxNumInstances>>();

        if (auto err = instances->Assign(mInstanceManager->GetActiveInstances()); !err.IsNone()) {
            return err;
        }

        Mutex             mutex;
        UniqueLock<Mutex> lock {mutex};

        return mBalancer->RunInstances(lock, *instances, rebalancing);
    }

    SharedPtr<Instance> FindInstance(const std::string& itemID)
    {
        auto it = mInstanceManager->GetActiveInstances().FindIf([&itemID](const SharedPtr<Instance>& instance) {
            return instance->GetInfo().mInstanceIdent.mItemID == itemID.c_str();
        });

        return it != mInstanceManager->GetActiveInstances().end() ? *it : SharedPtr<Instance>();
    }

    FailingAllocator                                 mAllocator;
    FailingAllocator                                 mBalancerAllocator;
    FaultyImageStoreStub                             mImageStore;
    FaultyStorageStub                                mStorage;
    FaultyStorageStateStub                           mStorageState;
    MonitoringProviderStub                           mMonitoringProvider;
    resourcemanager::ResourceManagerStub             mNodeConfigProvider;
    NiceMock<nodeinfoprovider::NodeInfoProviderMock> mNodeInfoProvider;
    NiceMock<InstanceRunnerMock>                     mRunner;
    SenderStub                                       mSender;
    OverrideEnvVarsListenerStub                      mListener;
    OverrideEnvVarsProcessor                         mOverrideEnvVarsProcessor;
    ImageInfoProvider                                mImageInfoProvider;
    std::unique_ptr<InstanceManager>                 mInstanceManager = std::make_unique<InstanceManager>();
    std::unique_ptr<NodeManager>                     mNodeManager     = std::make_unique<NodeManager>();
    std::unique_ptr<Balancer>                        mBalancer        = std::make_unique<Balancer>();
};

TEST_F(CMLauncherBalancerTest, PolicyBalancing)
{
    AddNode(cNodeIDLocalSM, {CreateRuntime(cRunnerRunc)});
    AddNode(cNodeIDRemoteSM1, {CreateRuntime(cRunnerRunc)}, NodeStateEnum::eProvisioned, false);
    AddNode(cNodeIDRemoteSM2, {CreateRuntime(cRunnerRunc)}, NodeStateEnum::ePaused);

    AddInstance(cService1, "unknownNode", cRunnerRunc);
    AddInstance(cService2, cNodeIDRemoteSM1, cRunnerRunc);
    AddInstance(cService3, cNodeIDRemoteSM2, cRunnerRunc);
    AddInstance("service4", cNodeIDLocalSM, "unknownRuntime");
    AddInstance("service5", cNodeIDLocalSM, cRunnerRunc);
    AddInstance("service6", cNodeIDLocalSM, cRunnerRunc, false);
    AddInstance("service7", "", cRunnerRunc);
    AddInstance("service8", cNodeIDLocalSM, "");

    ASSERT_TRUE(mInstanceManager->Start().IsNone());
    ASSERT_EQ(mInstanceManager->GetActiveInstances().Size(), 8);

    mImageStore.SetBlobError("service5:image1:item", ErrorEnum::eFailed);

    // Instances are sent to nodes, but sending fails.
    EXPECT_TRUE(RunInstances(true).Is(ErrorEnum::eFailed));

    auto service5 = FindInstance("service5");

    ASSERT_TRUE(service5);
    EXPECT_EQ(service5->GetStatus().mState, aos::InstanceStateEnum::eFailed);

    for (const auto& itemID : {cService1, cService2, cService3, "service4"}) {
        auto instance = FindInstance(itemID);

        ASSERT_TRUE(instance) << itemID;
        EXPECT_EQ(instance->GetInfo().mNodeID, cNodeIDLocalSM) << itemID;
        EXPECT_EQ(instance->GetInfo().mRuntimeID, cRunnerRunc) << itemID;
    }
}

TEST_F(CMLauncherBalancerTest, AllocationErrors)
{
    AddNode(cNodeIDLocalSM, {CreateRuntime(cRunnerRunc)});
    AddInstance(cService1, cNodeIDLocalSM, cRunnerRunc);

    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    mBalancerAllocator.Fail(sizeof(monitoring::NodeMonitoringData));
    EXPECT_TRUE(RunInstances(true).Is(ErrorEnum::eNoMemory));

    mBalancerAllocator.Fail(sizeof(oci::ImageIndex));
    EXPECT_TRUE(RunInstances(true).Is(ErrorEnum::eNoMemory));

    mBalancerAllocator.Fail(sizeof(monitoring::NodeMonitoringData));
    EXPECT_TRUE(mBalancer->LoadSMDataForActiveInstances().Is(ErrorEnum::eNoMemory));
    EXPECT_TRUE(mBalancer->LoadSMDataForActiveInstances().IsNone());

    // Allocation errors of node balancing: image index, node list and node runtimes.
    for (size_t skip = 1; skip < 4; skip++) {
        mBalancerAllocator.Fail(FailingAllocator::cAnySize, skip);

        EXPECT_TRUE(RunInstances(false).Is(ErrorEnum::eFailed));

        auto instance = FindInstance(cService1);

        ASSERT_TRUE(instance);
        EXPECT_EQ(instance->GetStatus().mState, aos::InstanceStateEnum::eFailed);
        EXPECT_TRUE(instance->GetStatus().mError.Is(ErrorEnum::eNoMemory));
    }

    EXPECT_TRUE(RunInstances(false).Is(ErrorEnum::eFailed));

    auto instance = FindInstance(cService1);

    ASSERT_TRUE(instance);
    EXPECT_EQ(instance->GetInfo().mNodeID, cNodeIDLocalSM);
    EXPECT_TRUE(instance->GetStatus().mError.IsNone());
}

TEST_F(CMLauncherBalancerTest, PrepareForBalancingErrors)
{
    AddNode(cNodeIDLocalSM, {CreateRuntime(cRunnerRunc)});

    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    mStorageState.mStateSizeErr = ErrorEnum::eWrongState;

    EXPECT_TRUE(RunInstances(false).Is(ErrorEnum::eWrongState));
    EXPECT_TRUE(mBalancer->LoadSMDataForActiveInstances().Is(ErrorEnum::eWrongState));

    mStorageState.mStateSizeErr = ErrorEnum::eNone;
    mMonitoringProvider.Init();

    EXPECT_TRUE(RunInstances(false).Is(ErrorEnum::eNotFound));
}

TEST_F(CMLauncherBalancerTest, NodeBalancingWithoutNodes)
{
    AddNode(cNodeIDLocalSM, {CreateRuntime(cRunnerRunc)});
    AddNode("emptyNode", {});

    // Instance bound to unknown node.
    AddInstance(cService1, "unknownNode", cRunnerRunc, true, true);
    AddInstance(cService2, cNodeIDLocalSM, cRunnerRunc);

    ASSERT_TRUE(mInstanceManager->Start().IsNone());

    EXPECT_TRUE(RunInstances(false).Is(ErrorEnum::eFailed));

    auto service1 = FindInstance(cService1);

    ASSERT_TRUE(service1);
    EXPECT_EQ(service1->GetStatus().mState, aos::InstanceStateEnum::eFailed);
    EXPECT_TRUE(service1->GetStatus().mError.Is(ErrorEnum::eNotFound));

    auto service2 = FindInstance(cService2);

    ASSERT_TRUE(service2);
    EXPECT_EQ(service2->GetInfo().mNodeID, cNodeIDLocalSM);
}

/***********************************************************************************************************************
 * Launcher allocation and process update error tests
 **********************************************************************************************************************/

TEST_F(CMLauncherErrorTest, StartFailsOnAllocationError)
{
    ASSERT_TRUE(InitLauncher().IsNone());

    // Node manager allocates node IDs array of the same size before subjects.
    constexpr auto cSkip = sizeof(SubjectArray) == sizeof(StaticArray<StaticString<cIDLen>, cMaxNumNodes>) ? 1 : 0;

    mAllocator.Fail(sizeof(SubjectArray), cSkip);

    EXPECT_TRUE(mLauncher->Start().Is(ErrorEnum::eNoMemory));
}

TEST_F(CMLauncherErrorTest, UpdateStatusesAllocationErrors)
{
    const auto ident = CreateInstanceIdent(cService1, cSubject1, 0);

    ASSERT_TRUE(
        mStorage.AddInstance(CreateInstanceInfo(ident, {}, cRunnerRunc, cNodeIDLocalSM, InstanceStateEnum::eActive))
            .IsNone());

    ASSERT_TRUE(InitLauncher().IsNone());

    InstanceStatusListenerStub statusListener;

    ASSERT_TRUE(mLauncher->SubscribeListener(statusListener).IsNone());
    ASSERT_TRUE(mLauncher->Start().IsNone());

    const auto notifyCount = statusListener.GetNotifyCount();
    const auto status
        = CreateInstanceStatus(ident, cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone);

    InstanceStatusReceiverItf& statusReceiver = *mLauncher;

    mAllocator.Fail(sizeof(StaticArray<InstanceStatus, cMaxNumInstances>));

    EXPECT_TRUE(statusReceiver.OnInstanceStatusReceived(status).IsNone());
    EXPECT_EQ(statusListener.GetNotifyCount(), notifyCount);

    mAllocator.Fail(sizeof(StaticArray<InstanceStatus, cMaxNumInstances>), 1);

    EXPECT_TRUE(statusReceiver.OnInstanceStatusReceived(status).IsNone());
    EXPECT_EQ(statusListener.GetNotifyCount(), notifyCount);

    // Next changed status is notified.
    EXPECT_TRUE(statusReceiver
                    .OnInstanceStatusReceived(CreateInstanceStatus(
                        ident, cNodeIDLocalSM, cRunnerRunc, aos::InstanceStateEnum::eFailed, ErrorEnum::eFailed))
                    .IsNone());
    EXPECT_EQ(statusListener.GetNotifyCount(), notifyCount + 1);
    ASSERT_EQ(statusListener.GetLastStatuses().Size(), 1);
    EXPECT_EQ(statusListener.GetLastStatuses()[0].mState, aos::InstanceStateEnum::eFailed);

    auto runRequests = std::make_unique<StaticArray<RunInstanceRequest, cMaxNumInstances>>();
    auto runStatuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    mAllocator.Fail(sizeof(StaticArray<SharedPtr<Instance>, cMaxNumInstances>));

    EXPECT_TRUE(mLauncher->RunInstances(*runRequests, *runStatuses).Is(ErrorEnum::eNoMemory));

    EXPECT_TRUE(mLauncher->UnsubscribeListener(statusListener).IsNone());
    EXPECT_TRUE(mLauncher->Stop().IsNone());
}

TEST_F(CMLauncherErrorTest, NodeInstancesStatusesErrors)
{
    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_)).WillRepeatedly(Invoke([](Array<StaticString<cIDLen>>& ids) {
        for (size_t i = 0; i < cMaxNumNodes; i++) {
            if (auto err = ids.PushBack(("node" + std::to_string(i)).c_str()); !err.IsNone()) {
                return err;
            }
        }

        return Error(ErrorEnum::eNone);
    }));
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(_, _))
        .WillRepeatedly(Invoke([](const String& nodeID, UnitNodeInfo& info) {
            info = CreateNodeInfo(
                nodeID.CStr(), 1000, 1024, {CreateRuntime(cRunnerRunc)}, {}, NodeStateEnum::eUnprovisioned, false);

            return ErrorEnum::eNone;
        }));

    const auto ident = CreateInstanceIdent(cService1, cSubject1, 0);

    ASSERT_TRUE(
        mStorage.AddInstance(CreateInstanceInfo(ident, {}, cRunnerRunc, "extraNode", InstanceStateEnum::eActive))
            .IsNone());

    ASSERT_TRUE(InitLauncher().IsNone());
    ASSERT_TRUE(mLauncher->Start().IsNone());

    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    statuses->PushBack(
        CreateInstanceStatus(ident, "extraNode", cRunnerRunc, aos::InstanceStateEnum::eActive, ErrorEnum::eNone));

    InstanceStatusReceiverItf& statusReceiver = *mLauncher;

    // Storage error is returned first, node can't be added as max number of nodes reached.
    mStorage.mUpdateErr = ErrorEnum::eFailed;

    EXPECT_TRUE(statusReceiver.OnNodeInstancesStatusesReceived("extraNode", *statuses).Is(ErrorEnum::eFailed));

    mStorage.mUpdateErr = ErrorEnum::eNone;

    auto err = statusReceiver.OnNodeInstancesStatusesReceived("extraNode", *statuses);
    EXPECT_FALSE(err.IsNone());
    EXPECT_FALSE(err.Is(ErrorEnum::eFailed));

    EXPECT_TRUE(mLauncher->Stop().IsNone());
}

TEST_F(CMLauncherErrorTest, ProcessUpdateErrors)
{
    EXPECT_CALL(mNodeInfoProvider, GetAllNodeIDs(_)).WillRepeatedly(Invoke([](Array<StaticString<cIDLen>>& ids) {
        return ids.PushBack(cNodeIDLocalSM);
    }));
    EXPECT_CALL(mNodeInfoProvider, GetNodeInfo(_, _))
        .WillRepeatedly(Invoke([](const String& nodeID, UnitNodeInfo& info) {
            info = CreateNodeInfo(nodeID.CStr(), 1000, 1024, {CreateRuntime(cRunnerRunc)});

            return ErrorEnum::eNone;
        }));

    std::mutex              mutex;
    std::condition_variable condVar;
    size_t                  numUpdates = 0;

    EXPECT_CALL(mInstanceRunner, UpdateInstances(_, _, _))
        .WillRepeatedly(Invoke([&](const String&, const Array<aos::InstanceInfo>&, const Array<aos::InstanceInfo>&) {
            std::lock_guard lock {mutex};

            numUpdates++;
            condVar.notify_all();

            return ErrorEnum::eFailed;
        }));

    alerts::AlertsListenerItf* alertsListener {};

    EXPECT_CALL(mAlertsProvider, SubscribeListener(_, _))
        .WillOnce(Invoke([&alertsListener](const Array<AlertTag>&, alerts::AlertsListenerItf& listener) {
            alertsListener = &listener;

            return ErrorEnum::eNone;
        }));

    ASSERT_TRUE(InitLauncher().IsNone());
    ASSERT_TRUE(mLauncher->Start().IsNone());
    ASSERT_NE(alertsListener, nullptr);

    // Node reports unknown running instance: launcher fails to stop it.
    auto statuses = std::make_unique<StaticArray<InstanceStatus, cMaxNumInstances>>();

    statuses->PushBack(CreateInstanceStatus(CreateInstanceIdent(cService1, cSubject1, 0), cNodeIDLocalSM, cRunnerRunc,
        aos::InstanceStateEnum::eActive, ErrorEnum::eNone));

    InstanceStatusReceiverItf& statusReceiver = *mLauncher;

    EXPECT_TRUE(statusReceiver.OnNodeInstancesStatusesReceived(cNodeIDLocalSM, *statuses).IsNone());

    {
        std::unique_lock lock {mutex};

        EXPECT_TRUE(condVar.wait_for(lock, std::chrono::seconds(5), [&]() { return numUpdates >= 1; }));
    }

    // Quota alerts: only raise and continue states trigger rebalancing, which fails as there is no monitoring.
    AlertVariant     alert;
    SystemQuotaAlert quotaAlert;

    quotaAlert.mState = QuotaAlertStateEnum::eFall;
    alert.SetValue(quotaAlert);

    EXPECT_TRUE(alertsListener->OnAlertReceived(alert).IsNone());

    for (const auto state : {QuotaAlertStateEnum::eRaise, QuotaAlertStateEnum::eContinue}) {
        quotaAlert.mState = state;
        alert.SetValue(quotaAlert);

        EXPECT_TRUE(alertsListener->OnAlertReceived(alert).IsNone());
    }

    EXPECT_TRUE(mMonitoringProvider.WaitForRequestCount(1));

    EXPECT_TRUE(mLauncher->Stop().IsNone());
}

} // namespace aos::cm::launcher
