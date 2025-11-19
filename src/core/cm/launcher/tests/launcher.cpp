/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include <core/cm/launcher/launcher.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

#include "stubs/imageinfoproviderstub.hpp"
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
    void OnInstancesStatusesChanged(const Array<InstanceStatus>& statuses) override { mLastStatuses = statuses; }
    const Array<InstanceStatus>& GetLastStatuses() const { return mLastStatuses; }
    void                         Clear() { mLastStatuses.Clear(); }

private:
    StaticArray<InstanceStatus, cMaxNumInstances> mLastStatuses;
};

class CMLauncherTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        LOG_INF() << "Launcher size: size=" << sizeof(Launcher);
    }

    void TearDown() override { }

protected:
    void AddService(const std::string& id, const String& imageID, const oci::ServiceConfig& serviceConfig,
        const oci::ImageConfig& imageConfig)
    {
        mImageInfoProvider.SetServiceConfig(id.c_str(), imageID, serviceConfig);
        mImageInfoProvider.SetImageConfig(id.c_str(), imageID, imageConfig);
    }

    // Stub objects
    launcher::ImageInfoProviderStub        mImageInfoProvider;
    networkmanager::NetworkManagerStub     mNetworkManager;
    nodeinfoprovider::NodeInfoProviderStub mNodeInfoProvider;
    launcher::InstanceRunnerStub           mInstanceRunner;
    launcher::InstanceStatusProviderStub   mInstanceStatusProvider;
    launcher::MonitoringProviderStub       mMonitoringProvider;
    resourcemanager::ResourceManagerStub   mResourceManager;
    launcher::StorageStub                  mStorage;
    storagestate::StorageStateStub         mStorageState;

    Launcher mLauncher;
};

InstanceInfo CreateInstanceInfo(const InstanceIdent& instance, const String& runtimeID = "1.0.0",
    const String& imageID = "1.0.0", const String& nodeID = "",
    UpdateItemType updateType = UpdateItemTypeEnum::eService, InstanceState instanceState = InstanceStateEnum::eActive,
    uint32_t uid = 0, Time timestamp = {}, bool cached = false)
{
    (void)instanceState;
    InstanceInfo result;

    result.mInstanceIdent  = instance;
    result.mRuntimeID      = runtimeID;
    result.mImageID        = imageID;
    result.mNodeID         = nodeID;
    result.mUpdateItemType = updateType;

    if (uid != 0) {
        result.mUID = uid;
    } else {
        static uint64_t curUID = 5000;

        result.mUID = curUID++;
    }

    result.mTimestamp = timestamp;
    result.mCached    = cached;

    return result;
}

InstanceStatus CreateInstanceStatus(const InstanceIdent& instance, const std::string& nodeID,
    const std::string& runtimeID, InstanceState state = InstanceStateEnum::eActivating, const Error& error = Error())
{
    InstanceStatus result;

    static_cast<InstanceIdent&>(result) = instance;
    result.mNodeID                      = nodeID.c_str();
    result.mRuntimeID                   = runtimeID.c_str();
    result.mState                       = state;
    result.mError                       = error;

    if (!error.IsNone()) {
        result.mState = InstanceStateEnum::eFailed;
    }

    return result;
}

InstanceIdent CreateInstanceIdent(const std::string& itemID, const std::string& subjectID = "", uint64_t instance = 0)
{
    return InstanceIdent {itemID.c_str(), subjectID.c_str(), instance};
}

RunInstanceRequest CreateRunRequest(const std::string& itemID, const std::string& subjectID = "", uint64_t priority = 0,
    size_t numInstances = 1, const std::string& providerID = "", UpdateItemType itemType = UpdateItemTypeEnum::eService,
    const std::vector<std::string>& labels = {})
{
    RunInstanceRequest request;

    request.mItemID       = itemID.c_str();
    request.mProviderID   = providerID.c_str();
    request.mItemType     = itemType;
    request.mSubjectID    = subjectID.c_str();
    request.mPriority     = priority;
    request.mNumInstances = numInstances;

    for (const auto& label : labels) {
        request.mLabels.PushBack(label.c_str());
    }

    return request;
}

aos::InstanceInfo CreateAosInstanceInfo(const InstanceIdent& id, const std::string& imageID,
    const std::string& runtimeID, uint32_t uid, const String& ip, uint64_t priority)
{
    aos::InstanceInfo result;

    static_cast<InstanceIdent&>(result) = id;
    result.mImageID                     = imageID.c_str();
    result.mRuntimeID                   = runtimeID.c_str();
    result.mUID                         = uid;
    result.mPriority                    = priority;
    result.mStoragePath                 = "storage_path";
    result.mStatePath                   = "state_path";
    result.mNetworkParameters.mIP       = (std::string("172.17.0.") + ip.CStr()).c_str();
    result.mNetworkParameters.mSubnet   = "172.17.0.0/16";

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
        nodeMonitoring.mServiceInstances.PushBack(inst);
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

void CreateServiceConfig(oci::ServiceConfig& config, const std::vector<std::string>& runners = {},
    const oci::BalancingPolicy& balancingPolicy = oci::BalancingPolicyEnum::eNone,
    const oci::ServiceQuotas& quotas = {}, const oci::RequestedResources& requestedResources = {},
    const Optional<AlertRules>& alertRules = {}, const std::vector<std::string>& allowedConnections = {},
    const std::vector<std::string>& resources = {})
{
    for (const auto& runner : runners) {
        config.mRunners.PushBack(runner.c_str());
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

oci::ImageConfig CreateImageConfig()
{
    oci::ImageConfig config;

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

RuntimeInfo CreateRuntimeInfo(
    const std::string& runtimeID, size_t allowedDMIPS = 0, ssize_t allowedRAM = 0, size_t maxInstances = 0)
{
    RuntimeInfo runtimeInfo;

    runtimeInfo.mRuntimeID   = runtimeID.c_str();
    runtimeInfo.mRuntimeType = "linux";

    // set platform info
    runtimeInfo.mArchInfo.mArchitecture = "x86_64";
    runtimeInfo.mArchInfo.mVariant.SetValue("generic");
    runtimeInfo.mOSInfo.mOS = "linux";
    runtimeInfo.mOSInfo.mVersion.SetValue("5.4.0");

    if (allowedDMIPS > 0) {
        runtimeInfo.mMaxDMIPS     = allowedDMIPS;
        runtimeInfo.mAllowedDMIPS = allowedDMIPS;
    }

    if (allowedRAM != 0) {
        runtimeInfo.mAllowedRAM = allowedRAM;
        runtimeInfo.mTotalRAM   = allowedRAM;
    }

    runtimeInfo.mMaxInstances = maxInstances;

    return runtimeInfo;
}

RuntimeInfo CreateRuntime(const std::string& runtimeID, size_t maxInstances = 0)
{
    RuntimeInfo runtimeInfo;

    runtimeInfo.mRuntimeID    = runtimeID.c_str();
    runtimeInfo.mRuntimeType  = runtimeID.c_str();
    runtimeInfo.mMaxInstances = maxInstances;

    return runtimeInfo;
}

UnitNodeInfo CreateNodeInfo(const std::string& nodeID, size_t maxDMIPS, size_t totalRAM,
    const std::vector<RuntimeInfo>& runtimes = {}, const std::vector<ResourceInfo>& resources = {},
    bool provisioned = true, NodeState state = NodeStateEnum::eOnline, Error error = ErrorEnum::eNone)
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

    nodeInfo.mProvisioned = provisioned;
    nodeInfo.mState       = state;
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
    cfg.mServiceTTL             = 1 * Time::cSeconds;

    ASSERT_TRUE(mStorage.AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1))).IsNone());

    ASSERT_TRUE(mLauncher
                    .Init(cfg, mStorage, mNodeInfoProvider, mInstanceRunner, mImageInfoProvider, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider)
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
    cfg.mServiceTTL             = 1 * Time::cHours;

    // Add outdated TTL.
    ASSERT_TRUE(
        mStorage
            .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService1), "", "", "", UpdateItemTypeEnum::eService,
                InstanceStateEnum::eInactive, 5000, Time::Now().Add(-25 * Time::cHours), true))
            .IsNone());

    // Add instance with current timestamp.
    ASSERT_TRUE(mStorage
                    .AddInstance(CreateInstanceInfo(CreateInstanceIdent(cService2), "", "", "",
                        UpdateItemTypeEnum::eService, InstanceStateEnum::eInactive, 5001, Time::Now(), true))
                    .IsNone());

    // Add services to the image provider.
    oci::ServiceConfig serviceConfig1;
    oci::ServiceConfig serviceConfig2;

    CreateServiceConfig(serviceConfig1);
    CreateServiceConfig(serviceConfig2);

    AddService(cService1, "", serviceConfig1, CreateImageConfig());
    AddService(cService2, "", serviceConfig2, CreateImageConfig());

    ASSERT_TRUE(mLauncher
                    .Init(cfg, mStorage, mNodeInfoProvider, mInstanceRunner, mImageInfoProvider, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider)
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

TEST_F(CMLauncherTest, InitialStatus)
{
    Config                         cfg;
    const std::vector<const char*> nodeIDs = {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2, cNodeIDRunxSM};

    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)},
        {CreateResource("resource1", 2), CreateResource("resource3", 2)}, true);
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRunc)},
        {CreateResource("resource1", 2), CreateResource("resource2", 2)}, true);
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    auto nodeInfoRemoteSM2 = CreateNodeInfo(cNodeIDRemoteSM2, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {}, true);
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM2, nodeInfoRemoteSM2);

    for (const auto& nodeID : nodeIDs) {
        NodeConfig nodeConfig;

        CreateNodeConfig(nodeConfig, nodeID);
        mResourceManager.SetNodeConfig(nodeID, cNodeTypeVM, nodeConfig);
    }

    // Init stubs and launcher
    for (const auto& serviceID : {cService1, cService2, cService3}) {
        oci::ServiceConfig serviceConfig;

        CreateServiceConfig(serviceConfig);
        AddService(serviceID, cImageID1, serviceConfig, CreateImageConfig());
    }

    StaticArray<InstanceInfo, cMaxNumInstances> storedInstances;

    storedInstances.PushBack(
        CreateInstanceInfo(CreateInstanceIdent(cService1), cImageID1, cRunnerRunc, cNodeIDLocalSM));
    storedInstances.PushBack(
        CreateInstanceInfo(CreateInstanceIdent(cService2), cImageID1, cRunnerRunc, cNodeIDRemoteSM1));
    storedInstances.PushBack(
        CreateInstanceInfo(CreateInstanceIdent(cService3), cImageID1, cRunnerRunc, cNodeIDRemoteSM2));

    mStorage.Init(storedInstances);

    ASSERT_TRUE(mLauncher
                    .Init(cfg, mStorage, mNodeInfoProvider, mInstanceRunner, mImageInfoProvider, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider)
                    .IsNone());

    // Start launcher
    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    ASSERT_TRUE(mLauncher.Start().IsNone());

    std::vector<InstanceStatus> statuses;
    for (const auto& instance : storedInstances) {
        statuses.push_back(CreateInstanceStatus(instance.mInstanceIdent, instance.mNodeID.CStr(),
            instance.mRuntimeID.CStr(), InstanceStateEnum::eActivating, ErrorEnum::eNone));
    }

    mInstanceStatusProvider.SetStatuses(statuses);
    EXPECT_EQ(instanceStatusListener.GetLastStatuses(), Array<InstanceStatus>(statuses.data(), statuses.size()));

    // Stop launcher
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
        {CreateResource("resource1", 2), CreateResource("resource3", 2)}, true);
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRunc)},
        {CreateResource("resource1", 2), CreateResource("resource2", 2)}, true);
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    auto nodeInfoRemoteSM2 = CreateNodeInfo(cNodeIDRemoteSM2, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {}, true);
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM2, nodeInfoRemoteSM2);

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2}) {
        NodeConfig nodeConfig;

        CreateNodeConfig(nodeConfig, nodeID);
        mResourceManager.SetNodeConfig(nodeID, cNodeTypeVM, nodeConfig);
    }

    // Set up configs
    for (const auto& serviceID : {cService1, cService2, cService3}) {
        oci::ServiceConfig serviceConfig;

        CreateServiceConfig(serviceConfig);
        AddService(serviceID, cImageID1, serviceConfig, CreateImageConfig());
    }

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mStorage, mNodeInfoProvider, mInstanceRunner, mImageInfoProvider, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    // Run instances 1
    StaticArray<RunInstanceRequest, cMaxNumInstances> runRequest1;
    runRequest1.PushBack(CreateRunRequest(cService1, cSubject1, 50, 1));
    runRequest1.PushBack(CreateRunRequest(cService2, cSubject1, 50, 1));
    runRequest1.PushBack(CreateRunRequest(cService3, cSubject1, 50, 1));

    ASSERT_TRUE(mLauncher.RunInstances(runRequest1).IsNone());

    StaticArray<InstanceInfo, cMaxNumInstances> instances;

    EXPECT_TRUE(mStorage.GetActiveInstances(instances).IsNone());
    ASSERT_EQ(instances.Size(), 3);
    EXPECT_EQ(instances[0].mInstanceIdent, CreateInstanceIdent(cService1, cSubject1, 0));
    EXPECT_EQ(instances[1].mInstanceIdent, CreateInstanceIdent(cService2, cSubject1, 0));
    EXPECT_EQ(instances[2].mInstanceIdent, CreateInstanceIdent(cService3, cSubject1, 0));
    EXPECT_FALSE(instances[0].mCached);
    EXPECT_FALSE(instances[1].mCached);
    EXPECT_FALSE(instances[2].mCached);

    // Run instances 2
    StaticArray<RunInstanceRequest, cMaxNumInstances> runRequest2;
    runRequest2.PushBack(CreateRunRequest(cService1, cSubject1, 50, 1));

    ASSERT_TRUE(mLauncher.RunInstances(runRequest2).IsNone());

    EXPECT_TRUE(mStorage.GetActiveInstances(instances).IsNone());
    ASSERT_EQ(instances.Size(), 3);

    EXPECT_EQ(instances[0].mInstanceIdent, CreateInstanceIdent(cService1, cSubject1, 0));
    EXPECT_EQ(instances[1].mInstanceIdent, CreateInstanceIdent(cService2, cSubject1, 0));
    EXPECT_EQ(instances[2].mInstanceIdent, CreateInstanceIdent(cService3, cSubject1, 0));
    EXPECT_FALSE(instances[0].mCached);
    EXPECT_TRUE(instances[1].mCached);
    EXPECT_TRUE(instances[2].mCached);

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

    mNodeInfoProvider.Init();
    auto nodeInfoLocalSM = CreateNodeInfo(cNodeIDLocalSM, 1000, 1024, {CreateRuntime(cRunnerRunc)},
        {CreateResource("resource1", 2), CreateResource("resource3", 2)}, true);
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRootfs, 1)},
        {CreateResource("resource1", 2), CreateResource("resource2", 2)}, true);
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    for (const auto& nodeID : {cNodeIDLocalSM, cNodeIDRemoteSM1}) {
        NodeConfig nodeConfig;

        CreateNodeConfig(nodeConfig, nodeID);
        mResourceManager.SetNodeConfig(nodeID, cNodeTypeVM, nodeConfig);
    }

    // Init launcher
    ASSERT_TRUE(mLauncher
                    .Init(cfg, mStorage, mNodeInfoProvider, mInstanceRunner, mImageInfoProvider, mResourceManager,
                        mStorageState, mNetworkManager, mMonitoringProvider)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    InstanceStatusListenerStub instanceStatusListener;
    mLauncher.SubscribeListener(instanceStatusListener);

    // Run instances
    StaticArray<RunInstanceRequest, cMaxNumInstances> runRequest;
    runRequest.PushBack(CreateRunRequest(cComponent1, cSubject1, 50, 1, "", UpdateItemTypeEnum::eComponent));

    ASSERT_TRUE(mLauncher.RunInstances(runRequest).IsNone());

    ASSERT_TRUE(mLauncher.Stop().IsNone());

    // Check sent run requests
    std::map<std::string, InstanceRunnerStub::NodeRunRequest> expectedRunRequests;
    InstanceRunnerStub::NodeRunRequest                        remoteRunRequest = {{},
                               {CreateAosInstanceInfo(
            CreateInstanceIdent(cComponent1, cSubject1, 0), cRootfsImageID, cRunnerRootfs, 0, "2", 50)}};

    expectedRunRequests[cNodeIDLocalSM]   = {};
    expectedRunRequests[cNodeIDRemoteSM1] = remoteRunRequest;

    EXPECT_EQ(mInstanceRunner.GetNodeInstances(), expectedRunRequests);

    // Check run status
    StaticArray<InstanceStatus, cMaxNumInstances> expectedRunStatus;
    expectedRunStatus.PushBack(CreateInstanceStatus({cComponent1, cSubject1, 0}, cNodeIDRemoteSM1, cRunnerRootfs));

    EXPECT_EQ(instanceStatusListener.GetLastStatuses(), expectedRunStatus);
}

/***********************************************************************************************************************
 * Balancing tests
 **********************************************************************************************************************/

struct TestData {
    const char*                                       mTestCaseName;
    std::map<std::string, NodeConfig>                 mNodeConfigs;
    std::map<std::string, oci::ServiceConfig>         mServiceConfigs;
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

    // Service configs
    CreateServiceConfig(testData->mServiceConfigs[cService1], {cRunnerRunc});
    CreateServiceConfig(testData->mServiceConfigs[cService2], {cRunnerRunc});
    CreateServiceConfig(testData->mServiceConfigs[cService3], {cRunnerRunx});

    // Desired instances
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 2));
    testData->mRunRequests.PushBack(CreateRunRequest(cService2, cSubject1, 50, 2));
    testData->mRunRequests.PushBack(CreateRunRequest(cService3, cSubject1, 0, 2));

    // Expected run requests
    std::vector<aos::InstanceInfo> localSMRequests;
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, "2", 100));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, "3", 100));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5002, "4", 50));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 1), cImageID1, cRunnerRunc, 5003, "5", 50));
    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = std::vector<aos::InstanceInfo>();

    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = std::vector<aos::InstanceInfo>();

    std::vector<aos::InstanceInfo> runxSMRequests;
    runxSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunx, 5004, "6", 0));
    runxSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService3, cSubject1, 1), cImageID1, cRunnerRunx, 5005, "7", 0));
    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances = runxSMRequests;

    // Expected run status
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 1}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 0}, cNodeIDRunxSM, cRunnerRunx));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 1}, cNodeIDRunxSM, cRunnerRunx));

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

    // Service configs
    CreateServiceConfig(testData->mServiceConfigs[cService1], {cRunnerRunc});
    CreateServiceConfig(testData->mServiceConfigs[cService2], {cRunnerRunc});
    CreateServiceConfig(testData->mServiceConfigs[cService3], {cRunnerRunx});

    // Desired instances
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService1, cSubject1, 100, 2, "", UpdateItemTypeEnum::eService, {"label2"}));
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService2, cSubject1, 50, 2, "", UpdateItemTypeEnum::eService, {"label1"}));
    testData->mRunRequests.PushBack(
        CreateRunRequest(cService3, cSubject1, 0, 2, "", UpdateItemTypeEnum::eService, {"label1"}));

    // Expected run requests
    std::vector<aos::InstanceInfo> localSMRequests;
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5002, "2", 50));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 1), cImageID1, cRunnerRunc, 5003, "3", 50));
    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    std::vector<aos::InstanceInfo> remoteSM1Requests;
    remoteSM1Requests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, "4", 100));
    remoteSM1Requests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, "5", 100));
    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = remoteSM1Requests;

    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = std::vector<aos::InstanceInfo>();

    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances = std::vector<aos::InstanceInfo>();

    // Expected run status
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDRemoteSM1, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDRemoteSM1, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 1}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 0}, "", "",
        InstanceStateEnum::eFailed, Error(ErrorEnum::eNotFound, "no nodes with instance labels")));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 1}, "", "",
        InstanceStateEnum::eFailed, Error(ErrorEnum::eNotFound, "no nodes with instance labels")));

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

    // Service configs
    CreateServiceConfig(
        testData->mServiceConfigs[cService1], {cRunnerRunc}, {}, {}, {}, {}, {}, {"resource1", "resource2"});
    CreateServiceConfig(testData->mServiceConfigs[cService2], {cRunnerRunc}, {}, {}, {}, {}, {}, {"resource1"});
    CreateServiceConfig(testData->mServiceConfigs[cService3], {cRunnerRunc}, {}, {}, {}, {}, {}, {"resource3"});

    // Desired instances
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 2));
    testData->mRunRequests.PushBack(CreateRunRequest(cService2, cSubject1, 50, 2));
    testData->mRunRequests.PushBack(CreateRunRequest(cService3, cSubject1, 0, 2));

    // Expected run requests
    std::vector<aos::InstanceInfo> localSMRequests;
    std::vector<aos::InstanceInfo> remoteSMRequests;

    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5002, "2", 50));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 1), cImageID1, cRunnerRunc, 5003, "3", 50));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5004, "4", 0));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService3, cSubject1, 1), cImageID1, cRunnerRunc, 5005, "5", 0));
    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    remoteSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, "6", 100));
    remoteSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, "7", 100));
    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = remoteSMRequests;

    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = std::vector<aos::InstanceInfo>();

    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances = std::vector<aos::InstanceInfo>();

    // Expected run status
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDRemoteSM1, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDRemoteSM1, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 1}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 1}, cNodeIDLocalSM, cRunnerRunc));

    return testData;
}

TestDataPtr TestItemStorageRatio()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "storage ratio";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100);

    // Service configs
    CreateServiceConfig(testData->mServiceConfigs[cService1], {cRunnerRunc}, {}, CreateServiceQuotas(500, 0, 0, 0),
        CreateRequestedResources(300, 0, 0, 0));

    // Desired instances
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 5));

    // Expected run requests - only 4 instances should fit due to storage limits
    std::vector<aos::InstanceInfo> localSMRequests;
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, "2", 100));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, "3", 100));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 2), cImageID1, cRunnerRunc, 5002, "4", 100));

    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = std::vector<aos::InstanceInfo>();
    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = std::vector<aos::InstanceInfo>();
    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances    = std::vector<aos::InstanceInfo>();

    // Expected run status - 4 succeed, 1 fails due to storage limits
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 2}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(
        {cService1, cSubject1, 3}, "", "", InstanceStateEnum::eFailed, Error(ErrorEnum::eNoMemory)));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(
        {cService1, cSubject1, 4}, "", "", InstanceStateEnum::eFailed, Error(ErrorEnum::eNoMemory)));

    return testData;
}

TestDataPtr TestItemStateRatio()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "state ratio";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100);

    // Service configs
    CreateServiceConfig(testData->mServiceConfigs[cService1], {cRunnerRunc}, {}, CreateServiceQuotas(0, 500, 0, 0),
        CreateRequestedResources(0, 300, 0, 0));

    // Desired instances
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 5));

    // Expected run requests - only 4 instances should fit due to state limits
    std::vector<aos::InstanceInfo> localSMRequests;
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, "2", 100));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, "3", 100));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 2), cImageID1, cRunnerRunc, 5002, "4", 100));

    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = std::vector<aos::InstanceInfo>();
    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = std::vector<aos::InstanceInfo>();
    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances    = std::vector<aos::InstanceInfo>();

    // Expected run status - 4 succeed, 1 fails due to state limits
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 2}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(
        {cService1, cSubject1, 3}, "", "", InstanceStateEnum::eFailed, Error(ErrorEnum::eNoMemory)));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus(
        {cService1, cSubject1, 4}, "", "", InstanceStateEnum::eFailed, Error(ErrorEnum::eNoMemory)));

    return testData;
}

TestDataPtr TestItemCpuRatio()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "cpu ratio";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100);

    // Service configs
    CreateServiceConfig(testData->mServiceConfigs[cService1], {cRunnerRunc}, {}, CreateServiceQuotas(0, 0, 500, 0),
        CreateRequestedResources(0, 0, 300, 0));

    // Desired instances
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 5));

    // Expected run requests
    std::vector<aos::InstanceInfo> localSMRequests;
    std::vector<aos::InstanceInfo> remoteSM1Requests;
    std::vector<aos::InstanceInfo> remoteSM2Requests;

    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, "2", 100));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, "3", 100));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 2), cImageID1, cRunnerRunc, 5002, "4", 100));

    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    remoteSM1Requests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 3), cImageID1, cRunnerRunc, 5003, "5", 100));

    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = remoteSM1Requests;

    remoteSM2Requests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 4), cImageID1, cRunnerRunc, 5004, "6", 100));

    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = remoteSM2Requests;

    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances = std::vector<aos::InstanceInfo>();

    // Expected run status
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 2}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 3}, cNodeIDRemoteSM1, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 4}, cNodeIDRemoteSM2, cRunnerRunc));

    return testData;
}

TestDataPtr TestItemRamRatio()
{
    auto testData = std::make_shared<TestData>();

    testData->mTestCaseName = "ram ratio";
    testData->mRebalancing  = false;

    // Node configs
    CreateNodeConfig(testData->mNodeConfigs[cNodeIDLocalSM], cNodeIDLocalSM, 100);

    // Service configs
    CreateServiceConfig(testData->mServiceConfigs[cService1], {cRunnerRunc}, {}, CreateServiceQuotas(0, 0, 0, 500),
        CreateRequestedResources(0, 0, 0, 300));

    // Desired instances
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 5));

    // Expected run requests
    std::vector<aos::InstanceInfo> localSMRequests;
    std::vector<aos::InstanceInfo> remoteSM1Requests;
    std::vector<aos::InstanceInfo> remoteSM2Requests;

    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, "2", 100));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 1), cImageID1, cRunnerRunc, 5001, "3", 100));
    localSMRequests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 2), cImageID1, cRunnerRunc, 5002, "4", 100));

    testData->mExpectedRunRequests[cNodeIDLocalSM].mStartInstances = localSMRequests;

    remoteSM1Requests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 3), cImageID1, cRunnerRunc, 5003, "5", 100));

    testData->mExpectedRunRequests[cNodeIDRemoteSM1].mStartInstances = remoteSM1Requests;

    remoteSM2Requests.push_back(
        CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 4), cImageID1, cRunnerRunc, 5004, "6", 100));

    testData->mExpectedRunRequests[cNodeIDRemoteSM2].mStartInstances = remoteSM2Requests;

    testData->mExpectedRunRequests[cNodeIDRunxSM].mStartInstances = std::vector<aos::InstanceInfo>();

    // Expected run status
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 2}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 3}, cNodeIDRemoteSM1, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 4}, cNodeIDRemoteSM2, cRunnerRunc));

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

    // Service configs
    CreateServiceConfig(testData->mServiceConfigs[cService1], {cRunnerRunc}, oci::BalancingPolicyEnum::eNone,
        CreateServiceQuotas(0, 0, 1000, 0));
    CreateServiceConfig(testData->mServiceConfigs[cService2], {cRunnerRunc}, oci::BalancingPolicyEnum::eNone,
        CreateServiceQuotas(0, 0, 1000, 0));
    CreateServiceConfig(testData->mServiceConfigs[cService3], {cRunnerRunc}, oci::BalancingPolicyEnum::eNone,
        CreateServiceQuotas(0, 0, 1000, 0));

    // Desired instances with priorities
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 1));
    testData->mRunRequests.PushBack(CreateRunRequest(cService2, cSubject1, 50, 1));
    testData->mRunRequests.PushBack(CreateRunRequest(cService3, cSubject1, 50, 1));

    // Expected run requests
    InstanceRunnerStub::NodeRunRequest localSMRequest = {
        {CreateAosInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5001, "3", 50)},
        {CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, "5", 100)},
    };

    testData->mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    InstanceRunnerStub::NodeRunRequest remoteSM1Request = {
        {CreateAosInstanceInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5002, "4", 50)},
        {CreateAosInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5001, "6", 50)},
    };

    testData->mExpectedRunRequests[cNodeIDRemoteSM1] = remoteSM1Request;

    InstanceRunnerStub::NodeRunRequest remoteSM2Request = {
        {},
        {CreateAosInstanceInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5002, "7", 50)},
    };

    testData->mExpectedRunRequests[cNodeIDRemoteSM2] = remoteSM2Request;

    testData->mExpectedRunRequests[cNodeIDRunxSM] = InstanceRunnerStub::NodeRunRequest {};

    // Expected run status
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDRemoteSM1, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService3, cSubject1, 0}, cNodeIDRemoteSM2, cRunnerRunc));

    // Monitoring data
    CreateNodeMonitoring(testData->mMonitoring[cNodeIDLocalSM], cNodeIDLocalSM, 1000,
        {CreateInstanceMonitoring(InstanceIdent {cService1, cSubject1, 0}, 500),
            CreateInstanceMonitoring(InstanceIdent {cService2, cSubject1, 0}, 500)});

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
    CreateServiceConfig(testData->mServiceConfigs[cService1], {cRunnerRunc}, oci::BalancingPolicyEnum::eNone,
        CreateServiceQuotas(0, 0, 1000, 0), {}, {});
    CreateServiceConfig(testData->mServiceConfigs[cService2], {cRunnerRunc}, oci::BalancingPolicyEnum::eNone,
        CreateServiceQuotas(0, 0, 1000, 0));
    CreateServiceConfig(testData->mServiceConfigs[cService3], {cRunnerRunc},
        oci::BalancingPolicyEnum::eBalancingDisabled, CreateServiceQuotas(0, 0, 1000, 0));

    // Desired instances with priorities
    testData->mRunRequests.PushBack(CreateRunRequest(cService1, cSubject1, 100, 1));
    testData->mRunRequests.PushBack(CreateRunRequest(cService2, cSubject1, 50, 1));
    testData->mRunRequests.PushBack(CreateRunRequest(cService3, cSubject1, 50, 1));

    // Expected run requests
    InstanceRunnerStub::NodeRunRequest localSMRequest = {
        {CreateAosInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5001, "3", 50)},
        {CreateAosInstanceInfo(CreateInstanceIdent(cService1, cSubject1, 0), cImageID1, cRunnerRunc, 5000, "5", 100)},
    };

    testData->mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    InstanceRunnerStub::NodeRunRequest remoteSM1Request = {
        {},
        {CreateAosInstanceInfo(CreateInstanceIdent(cService3, cSubject1, 0), cImageID1, cRunnerRunc, 5002, "6", 50)},
    }; // service3 should not rebalance

    testData->mExpectedRunRequests[cNodeIDRemoteSM1] = remoteSM1Request;

    InstanceRunnerStub::NodeRunRequest remoteSM2Request = {
        {},
        {CreateAosInstanceInfo(CreateInstanceIdent(cService2, cSubject1, 0), cImageID1, cRunnerRunc, 5001, "7", 50)},
    };

    testData->mExpectedRunRequests[cNodeIDRemoteSM2] = remoteSM2Request;

    testData->mExpectedRunRequests[cNodeIDRunxSM] = InstanceRunnerStub::NodeRunRequest {};

    // Expected run status
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService3, cSubject1, 0}, cNodeIDRemoteSM1, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, cRunnerRunc));
    testData->mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDRemoteSM2, cRunnerRunc));

    // Monitoring data
    CreateNodeMonitoring(testData->mMonitoring[cNodeIDLocalSM], cNodeIDLocalSM, 1000,
        {CreateInstanceMonitoring(InstanceIdent {cService1, cSubject1, 0}, 500),
            CreateInstanceMonitoring(InstanceIdent {cService2, cSubject1, 0}, 500)});

    return testData;
}

TEST_F(CMLauncherTest, Balancing)
{
    Config cfg;

    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    const std::vector<const char*> nodeIDs = {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2, cNodeIDRunxSM};

    std::vector<TestDataPtr> testItems
        = {TestItemNodePriority(), TestItemLabels(), TestItemResources(), TestItemStorageRatio(), TestItemStateRatio(),
            TestItemCpuRatio(), TestItemRamRatio(), TestItemRebalancing(), TestItemRebalancingPolicy()};

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
            {CreateResource("resource1", 2), CreateResource("resource3", 2)}, true);
        mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

        auto nodeInfoRemoteSM1 = CreateNodeInfo(cNodeIDRemoteSM1, 1000, 1024, {CreateRuntime(cRunnerRunc)},
            {CreateResource("resource1", 2), CreateResource("resource2", 2)}, true);
        mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

        auto nodeInfoRemoteSM2 = CreateNodeInfo(cNodeIDRemoteSM2, 1000, 1024, {CreateRuntime(cRunnerRunc)}, {}, true);
        mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM2, nodeInfoRemoteSM2);

        auto nodeInfoRunxSM = CreateNodeInfo(cNodeIDRunxSM, 1000, 1024, {CreateRuntime(cRunnerRunx)}, {}, true);
        mNodeInfoProvider.AddNodeInfo(cNodeIDRunxSM, nodeInfoRunxSM);

        mImageInfoProvider.Init();
        mNetworkManager.Init();
        mInstanceRunner.Init();
        mInstanceStatusProvider.Init();
        mMonitoringProvider.Init();
        mResourceManager.Init();
        mStorage.Init(testItem.mStoredInstances);

        // Set up configs
        for (const auto& [serviceID, config] : testItem.mServiceConfigs) {
            mImageInfoProvider.SetServiceConfig(serviceID.c_str(), cImageID1, config);
            mImageInfoProvider.SetImageConfig(serviceID.c_str(), cImageID1, CreateImageConfig());
        }

        for (const auto& [nodeID, nodeConfig] : testItem.mNodeConfigs) {
            mResourceManager.SetNodeConfig(nodeID.c_str(), cNodeTypeVM, nodeConfig);
        }

        // Init launcher
        ASSERT_TRUE(mLauncher
                        .Init(cfg, mStorage, mNodeInfoProvider, mInstanceRunner, mImageInfoProvider, mResourceManager,
                            mStorageState, mNetworkManager, mMonitoringProvider)
                        .IsNone());

        InstanceStatusListenerStub instanceStatusListener;
        mLauncher.SubscribeListener(instanceStatusListener);

        ASSERT_TRUE(mLauncher.Start().IsNone());

        // Run instances
        ASSERT_TRUE(mLauncher.RunInstances(testItem.mRunRequests).IsNone());

        if (testItem.mRebalancing) {
            for (const auto& [nodeID, monitoring] : testItem.mMonitoring) {
                mMonitoringProvider.SetAverageMonitoring(nodeID.c_str(), monitoring);
            }

            ASSERT_TRUE(mLauncher.Rebalance().IsNone());
        }

        EXPECT_EQ(instanceStatusListener.GetLastStatuses(), testItem.mExpectedRunStatus);
        ASSERT_TRUE(mLauncher.Stop().IsNone());

        // Check sent run requests
        EXPECT_EQ(mInstanceRunner.GetNodeInstances(), testItem.mExpectedRunRequests);
    }
}

} // namespace aos::cm::launcher
