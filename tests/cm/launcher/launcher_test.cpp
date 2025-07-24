/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>
#include <gmock/gmock.h>

#include <aos/cm/launcher/launcher.hpp>

#include <aos/test/log.hpp>
#include <aos/test/utils.hpp>

#include "stubs/imageprovider.hpp"
#include "stubs/networkmanager.hpp"
#include "stubs/nodeinfoprovider.hpp"
#include "stubs/nodemanager.hpp"
#include "stubs/resourcemanager.hpp"
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
constexpr auto cStoragePartition  = "storages";
constexpr auto cStatePartition    = "states";
constexpr auto cNodeIDLocalSM     = "localSM";
constexpr auto cNodeIDRemoteSM1   = "remoteSM1";
constexpr auto cNodeIDRemoteSM2   = "remoteSM2";
constexpr auto cNodeIDRunxSM      = "runxSM";
constexpr auto cNodeTypeLocalSM   = "localSMType";
constexpr auto cNodeTypeRemoteSM  = "remoteSMType";
constexpr auto cNodeTypeRunxSM    = "runxSMType";
constexpr auto cSubject1          = "subject1";
constexpr auto cService1          = "service1";
constexpr auto cService1LocalURL  = "service1LocalURL";
constexpr auto cService1RemoteURL = "service1RemoteURL";
constexpr auto cService2          = "service2";
constexpr auto cService2LocalURL  = "service2LocalURL";
constexpr auto cService2RemoteURL = "service2RemoteURL";
constexpr auto cService3          = "service3";
constexpr auto cService3LocalURL  = "service3LocalURL";
constexpr auto cService3RemoteURL = "service3RemoteURL";
constexpr auto cLayer1            = "layer1";
constexpr auto cLayer1LocalURL    = "layer1LocalURL";
constexpr auto cLayer1RemoteURL   = "layer1RemoteURL";
constexpr auto cLayer2            = "layer2";
constexpr auto cLayer2LocalURL    = "layer2LocalURL";
constexpr auto cLayer2RemoteURL   = "layer2RemoteURL";

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class MockRunStatusListener : public RunStatusListenerItf {
public:
    MOCK_METHOD(void, OnRunStatusChanged, (const Array<nodemanager::InstanceStatus>& runStatuses), (override));
};

class CMLauncherTest : public Test {
protected:
    void SetUp() override
    {
        test::InitLog();

        LOG_INF() << "Launcher size: size=" << sizeof(Launcher);
    }

    void TearDown() override { }

protected:
    imageprovider::ImageProviderStub       mImageProvider;
    networkmanager::NetworkManagerStub     mNetworkManager;
    nodeinfoprovider::NodeInfoProviderStub mNodeInfoProvider;
    nodemanager::NodeManagerStub           mNodeManager;
    resourcemanager::ResourceManagerStub   mResourceManager;
    storagestate::StorageStateStub         mStorageState;
    storage::StorageStub                   mStorage;

    Launcher mLauncher;
};

PartitionInfo CreatePartitionInfo(const String& name, const String& type, uint64_t totalSize)
{
    PartitionInfo result;

    result.mName = name;
    result.mTypes.PushBack(type);
    result.mTotalSize = totalSize;

    return result;
}

ServiceInfo CreateServiceInfo(const String& id, uint32_t gid, const String& url)
{
    ServiceInfo serviceInfo;

    serviceInfo.mServiceID = id;
    serviceInfo.mVersion   = "1.0";
    serviceInfo.mURL       = url;
    serviceInfo.mGID       = gid;

    return serviceInfo;
}

imageprovider::ServiceInfo CreateExServiceInfo(const String& id, uint32_t gid, const String& url)
{
    imageprovider::ServiceInfo serviceInfo;

    static_cast<ServiceInfo&>(serviceInfo) = CreateServiceInfo(id, gid, url);

    return serviceInfo;
}

LayerInfo CreateLayerInfo(const String& digest, const String& url)
{
    LayerInfo layerInfo;

    layerInfo.mLayerDigest = digest;
    layerInfo.mURL         = url;

    return layerInfo;
}

imageprovider::LayerInfo CreateExLayerInfo(const String& digest, const String& url)
{
    imageprovider::LayerInfo layerInfo;

    static_cast<LayerInfo&>(layerInfo) = CreateLayerInfo(digest, url);

    return layerInfo;
}

InstanceInfo CreateInstanceInfo(const InstanceIdent& id, uint32_t uid, const String& ip, uint64_t priority)
{
    InstanceInfo result;

    result.mInstanceIdent             = id;
    result.mUID                       = uid;
    result.mPriority                  = priority;
    result.mNetworkParameters.mIP     = (std::string("172.17.0.") + ip.CStr()).c_str();
    result.mNetworkParameters.mSubnet = "172.17.0.0/16";
    result.mNetworkParameters.mDNSServers.PushBack("10.10.0.1");

    return result;
}

RunServiceRequest CreateRunServiceRequest(const String& serviceID, const String& subjectID, uint64_t priority,
    uint64_t numInstances, const std::vector<std::string>& labels = {})
{
    auto request = RunServiceRequest {serviceID, subjectID, priority, numInstances, {}};

    for (const auto& label : labels) {
        request.mLabels.PushBack(label.c_str());
    }

    return request;
}

DeviceInfo Device(const String& name, size_t count)
{
    DeviceInfo device;

    device.mName        = name;
    device.mSharedCount = count;

    return device;
}

Optional<AlertRules> AlertRulesCPU(double minTreshold, double maxTreshold)
{
    AlertRules alertRules;

    alertRules.mCPU.SetValue(AlertRulePercents());
    alertRules.mCPU->mMinThreshold = minTreshold;
    alertRules.mCPU->mMaxThreshold = maxTreshold;

    return alertRules;
}

NodeConfig CreateNodeConfig(const String& nodeType, uint32_t priority, const std::vector<std::string>& labels = {},
    const std::vector<std::string>& resources = {}, const std::vector<DeviceInfo>& devices = {},
    const Optional<AlertRules>& alertRules = {})
{
    NodeConfig config;

    config.mNodeType = nodeType;
    config.mPriority = priority;

    for (const auto& label : labels) {
        config.mLabels.PushBack(label.c_str());
    }

    for (const auto& resource : resources) {
        ResourceInfo info;
        info.mName = resource.c_str();

        config.mResources.PushBack(info);
    }

    config.mDevices.Insert(config.mDevices.end(), devices.data(), devices.data() + devices.size());

    config.mAlertRules = alertRules;

    return config;
}

oci::ServiceQuotas Quotas(const Optional<uint64_t>& storageLimit, const Optional<uint64_t>& stateLimit = {},
    const Optional<uint64_t>& cpuDMIPS = {}, const Optional<uint64_t>& ramLimit = {})
{
    oci::ServiceQuotas quota;

    quota.mStorageLimit  = storageLimit;
    quota.mStateLimit    = stateLimit;
    quota.mCPUDMIPSLimit = cpuDMIPS;
    quota.mRAMLimit      = ramLimit;

    return quota;
}

oci::ServiceConfig CreateServiceConfig(const std::vector<std::string>& runners,
    const std::vector<std::string>& resources = {}, const std::vector<std::string>& devices = {},
    const oci::ServiceQuotas& quotas = {}, bool skipResourceLimits = false)
{
    oci::ServiceConfig config;

    for (const auto& runner : runners) {
        config.mRunners.PushBack(runner.c_str());
    }

    for (const auto& resource : resources) {
        config.mResources.PushBack(resource.c_str());
    }

    for (const auto& device : devices) {
        config.mDevices.PushBack({device.c_str(), ""});
    }

    config.mQuotas             = quotas;
    config.mSkipResourceLimits = skipResourceLimits;

    return config;
}

nodemanager::InstanceStatus CreateInstanceStatus(const InstanceIdent& id, const String& nodeID, const Error& err)
{
    nodemanager::InstanceStatus status;

    status.mInstanceIdent  = id;
    status.mRunState       = InstanceRunStateEnum::eActive;
    status.mServiceVersion = "1.0";
    status.mNodeID         = nodeID;
    status.mError          = err;

    if (err.IsNone()) {
        status.mStateChecksum = cMagicSum;
    } else {
        status.mRunState = InstanceRunStateEnum::eFailed;
    }

    return status;
}

monitoring::NodeMonitoringData CreateNodeMonitoring(
    const String& nodeID, double cpu, const std::vector<monitoring::InstanceMonitoringData>& instances)
{
    monitoring::NodeMonitoringData data;

    data.mNodeID              = nodeID;
    data.mMonitoringData.mCPU = cpu;

    for (const auto& instance : instances) {
        data.mServiceInstances.PushBack(instance);
    }

    return data;
}

monitoring::InstanceMonitoringData InstanceMonitoring(const InstanceIdent& instance, double cpu)
{
    monitoring::InstanceMonitoringData data;

    data.mInstanceIdent       = instance;
    data.mMonitoringData.mCPU = cpu;

    return data;
}

/***********************************************************************************************************************
 * Balancing test data
 **********************************************************************************************************************/

struct TestData {
    const char*                                                mTestCaseName;
    std::map<std::string, NodeConfig>                          mNodeConfigs;
    std::map<std::string, oci::ServiceConfig>                  mServiceConfigs;
    StaticArray<RunServiceRequest, cMaxNumInstances>           mDesiredInstances;
    StaticArray<storage::InstanceInfo, cMaxNumInstances>       mStoredInstances;
    std::map<std::string, nodemanager::StartRequest>           mExpectedRunRequests;
    StaticArray<nodemanager::InstanceStatus, cMaxNumInstances> mExpectedRunStatus;
    std::map<std::string, monitoring::NodeMonitoringData>      mMonitoring;
    bool                                                       mRebalancing;
};

TestData TestItemNodePriority()
{
    TestData testData;
    testData.mTestCaseName = "node priority";
    testData.mRebalancing  = false;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM]  = CreateNodeConfig(cNodeTypeLocalSM, 100);
    testData.mNodeConfigs[cNodeTypeRemoteSM] = CreateNodeConfig(cNodeTypeRemoteSM, 50);
    testData.mNodeConfigs[cNodeTypeRunxSM]   = CreateNodeConfig(cNodeTypeRunxSM, 0);

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({cRunnerRunc});
    testData.mServiceConfigs[cService2] = CreateServiceConfig({cRunnerRunc});
    testData.mServiceConfigs[cService3] = CreateServiceConfig({cRunnerRunx});

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 2));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService2, cSubject1, 50, 2));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService3, cSubject1, 0, 2));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1LocalURL));
    localSMRequest.mServices.push_back(CreateServiceInfo(cService2, 5001, cService2LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "2", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 1}, 5001, "3", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 0}, 5002, "4", 50));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 1}, 5003, "5", 50));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    testData.mExpectedRunRequests[cNodeIDRemoteSM1] = {};

    testData.mExpectedRunRequests[cNodeIDRemoteSM2] = {};

    nodemanager::StartRequest runxSMRequest;
    runxSMRequest.mServices.push_back(CreateServiceInfo(cService3, 5002, cService3RemoteURL));
    runxSMRequest.mInstances.push_back(CreateInstanceInfo({cService3, cSubject1, 0}, 5004, "6", 0));
    runxSMRequest.mInstances.push_back(CreateInstanceInfo({cService3, cSubject1, 1}, 5005, "7", 0));
    testData.mExpectedRunRequests[cNodeIDRunxSM] = runxSMRequest;

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 1}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 0}, cNodeIDRunxSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 1}, cNodeIDRunxSM, Error()));

    return testData;
}

TestData TestItemLabels()
{
    TestData testData;
    testData.mTestCaseName = "labels";
    testData.mRebalancing  = false;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM]  = CreateNodeConfig(cNodeTypeLocalSM, 100, {"label1"});
    testData.mNodeConfigs[cNodeTypeRemoteSM] = CreateNodeConfig(cNodeTypeRemoteSM, 50, {"label2"});
    testData.mNodeConfigs[cNodeTypeRunxSM]   = CreateNodeConfig(cNodeTypeRunxSM, 0);

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({cRunnerRunc});
    testData.mServiceConfigs[cService2] = CreateServiceConfig({cRunnerRunc});
    testData.mServiceConfigs[cService3] = CreateServiceConfig({cRunnerRunx});

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 2, {"label2"}));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService2, cSubject1, 50, 2, {"label1"}));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService3, cSubject1, 0, 2, {"label1"}));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService2, 5001, cService2LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 0}, 5002, "2", 50));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 1}, 5003, "3", 50));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    nodemanager::StartRequest remoteSM1Request;
    remoteSM1Request.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1RemoteURL));
    remoteSM1Request.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1RemoteURL));
    remoteSM1Request.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2RemoteURL));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "4", 100));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 1}, 5001, "5", 100));
    testData.mExpectedRunRequests[cNodeIDRemoteSM1] = remoteSM1Request;

    testData.mExpectedRunRequests[cNodeIDRemoteSM2] = {};
    testData.mExpectedRunRequests[cNodeIDRunxSM]    = {};

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 1}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus(
        {cService3, cSubject1, 0}, "", Error(ErrorEnum::eNotFound, "no nodes with instance labels")));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus(
        {cService3, cSubject1, 1}, "", Error(ErrorEnum::eNotFound, "no nodes with instance labels")));

    return testData;
}

TestData TestItemResources()
{
    TestData testData;
    testData.mTestCaseName = "resources";
    testData.mRebalancing  = false;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM]
        = CreateNodeConfig(cNodeTypeLocalSM, 100, {}, {{"resource1"}, {"resource3"}});
    testData.mNodeConfigs[cNodeTypeRemoteSM]
        = CreateNodeConfig(cNodeTypeRemoteSM, 50, {"label2"}, {{"resource1"}, {"resource2"}});
    testData.mNodeConfigs[cNodeTypeRunxSM] = CreateNodeConfig(cNodeTypeRunxSM, 0);

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({cRunnerRunc}, {"resource1", "resource2"});
    testData.mServiceConfigs[cService2] = CreateServiceConfig({cRunnerRunc}, {"resource1"});
    testData.mServiceConfigs[cService3] = CreateServiceConfig({cRunnerRunc}, {"resource3"});

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 2));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService2, cSubject1, 50, 2));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService3, cSubject1, 0, 2, {"label2"}));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService2, 5001, cService2LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 0}, 5002, "2", 50));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 1}, 5003, "3", 50));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    nodemanager::StartRequest remoteSM1Request;
    remoteSM1Request.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1RemoteURL));
    remoteSM1Request.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1RemoteURL));
    remoteSM1Request.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2RemoteURL));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "4", 100));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 1}, 5001, "5", 100));
    testData.mExpectedRunRequests[cNodeIDRemoteSM1] = remoteSM1Request;

    testData.mExpectedRunRequests[cNodeIDRemoteSM2] = {};
    testData.mExpectedRunRequests[cNodeIDRunxSM]    = {};

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 1}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService3, cSubject1, 0}, "", Error(ErrorEnum::eNotFound)));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService3, cSubject1, 1}, "", Error(ErrorEnum::eNotFound)));

    return testData;
}

TestData TestItemDevices()
{
    TestData testData;
    testData.mTestCaseName = "devices";
    testData.mRebalancing  = false;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM]
        = CreateNodeConfig(cNodeTypeLocalSM, 100, {}, {}, {Device("dev1", 1), Device("dev2", 2), Device("dev3", 0)});
    testData.mNodeConfigs[cNodeTypeRemoteSM]
        = CreateNodeConfig(cNodeTypeRemoteSM, 50, {"label2"}, {}, {Device("dev1", 1), Device("dev2", 3)});
    testData.mNodeConfigs[cNodeTypeRunxSM]
        = CreateNodeConfig(cNodeTypeRunxSM, 0, {}, {}, {Device("dev1", 1), Device("dev2", 2)});

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({cRunnerRunc}, {}, {"dev1", "dev2"});
    testData.mServiceConfigs[cService2] = CreateServiceConfig({cRunnerRunc}, {}, {"dev2"});
    testData.mServiceConfigs[cService3] = CreateServiceConfig({cRunnerRunc}, {}, {"dev3"});

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 4));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService2, cSubject1, 50, 3));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService3, cSubject1, 0, 2, {"label2"}));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1LocalURL));
    localSMRequest.mServices.push_back(CreateServiceInfo(cService2, 5001, cService2LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "2", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 0}, 5003, "3", 50));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    nodemanager::StartRequest remoteSM1Request;
    remoteSM1Request.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1RemoteURL));
    remoteSM1Request.mServices.push_back(CreateServiceInfo(cService2, 5001, cService2RemoteURL));
    remoteSM1Request.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1RemoteURL));
    remoteSM1Request.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2RemoteURL));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 1}, 5001, "4", 100));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 1}, 5004, "5", 50));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 2}, 5005, "6", 50));
    testData.mExpectedRunRequests[cNodeIDRemoteSM1] = remoteSM1Request;

    nodemanager::StartRequest remoteSM2Request;
    remoteSM2Request.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1RemoteURL));
    remoteSM2Request.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1RemoteURL));
    remoteSM2Request.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2RemoteURL));
    remoteSM2Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 2}, 5002, "7", 100));
    testData.mExpectedRunRequests[cNodeIDRemoteSM2] = remoteSM2Request;

    testData.mExpectedRunRequests[cNodeIDRunxSM] = {};

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 1}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 2}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 2}, cNodeIDRemoteSM2, Error()));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 3}, "", Error(ErrorEnum::eNotFound)));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService3, cSubject1, 0}, "", Error(ErrorEnum::eNotFound)));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService3, cSubject1, 1}, "", Error(ErrorEnum::eNotFound)));

    return testData;
}

TestData TestItemStorageRatio()
{
    TestData testData;
    testData.mTestCaseName = "storage ratio";
    testData.mRebalancing  = false;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM] = CreateNodeConfig(cNodeTypeLocalSM, 100);

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({cRunnerRunc}, {}, {}, Quotas(500));

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 5));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "2", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 1}, 5001, "3", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 2}, 5002, "4", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 3}, 5003, "5", 100));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 2}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 3}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 4}, "", Error(ErrorEnum::eNoMemory)));

    return testData;
}

TestData TestItemStateRatio()
{
    TestData testData;
    testData.mTestCaseName = "state ratio";
    testData.mRebalancing  = false;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM] = CreateNodeConfig(cNodeTypeLocalSM, 100);

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({cRunnerRunc}, {}, {}, Quotas({}, 500));

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 5));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "2", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 1}, 5001, "3", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 2}, 5002, "4", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 3}, 5003, "5", 100));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 2}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 3}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 4}, "", Error(ErrorEnum::eNoMemory)));

    return testData;
}

TestData TestItemCPURatio()
{
    TestData testData;
    testData.mTestCaseName = "CPU ratio";
    testData.mRebalancing  = false;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM] = CreateNodeConfig(cNodeTypeLocalSM, 100);

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({cRunnerRunc}, {}, {}, Quotas({}, {}, 1000));

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 8));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "2", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 1}, 5001, "3", 100));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    nodemanager::StartRequest remoteSM1Request;
    remoteSM1Request.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1RemoteURL));
    remoteSM1Request.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1RemoteURL));
    remoteSM1Request.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2RemoteURL));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 2}, 5002, "4", 100));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 4}, 5004, "5", 100));
    testData.mExpectedRunRequests[cNodeIDRemoteSM1] = remoteSM1Request;

    nodemanager::StartRequest remoteSM2Request;
    remoteSM2Request.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1RemoteURL));
    remoteSM2Request.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1RemoteURL));
    remoteSM2Request.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2RemoteURL));
    remoteSM2Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 3}, 5003, "6", 100));
    remoteSM2Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 5}, 5005, "7", 100));
    testData.mExpectedRunRequests[cNodeIDRemoteSM2] = remoteSM2Request;

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 2}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 4}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 3}, cNodeIDRemoteSM2, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 5}, cNodeIDRemoteSM2, Error()));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 6}, "", Error(ErrorEnum::eNotFound)));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 7}, "", Error(ErrorEnum::eNotFound)));

    return testData;
}

TestData TestItemRAMRatio()
{
    TestData testData;
    testData.mTestCaseName = "RAM ratio";
    testData.mRebalancing  = false;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM] = CreateNodeConfig(cNodeTypeLocalSM, 100);

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({cRunnerRunc}, {}, {}, Quotas({}, {}, {}, 1024));

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 8));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1LocalURL));
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "2", 100));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 1}, 5001, "3", 100));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    nodemanager::StartRequest remoteSM1Request;
    remoteSM1Request.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1RemoteURL));
    remoteSM1Request.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1RemoteURL));
    remoteSM1Request.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2RemoteURL));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 2}, 5002, "4", 100));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 3}, 5003, "5", 100));
    testData.mExpectedRunRequests[cNodeIDRemoteSM1] = remoteSM1Request;

    nodemanager::StartRequest remoteSM2Request;
    remoteSM2Request.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1RemoteURL));
    remoteSM2Request.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1RemoteURL));
    remoteSM2Request.mLayers.push_back(CreateLayerInfo(cLayer2, cLayer2RemoteURL));
    remoteSM2Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 4}, 5004, "6", 100));
    remoteSM2Request.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 5}, 5005, "7", 100));
    testData.mExpectedRunRequests[cNodeIDRemoteSM2] = remoteSM2Request;

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 1}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 2}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 3}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 4}, cNodeIDRemoteSM2, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 5}, cNodeIDRemoteSM2, Error()));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 6}, "", Error(ErrorEnum::eNotFound)));
    testData.mExpectedRunStatus.PushBack(
        CreateInstanceStatus({cService1, cSubject1, 7}, "", Error(ErrorEnum::eNotFound)));

    return testData;
}

TestData TestItemSkipResourceLimits()
{
    TestData testData;
    testData.mTestCaseName = "skip resource limits";
    testData.mRebalancing  = false;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM] = CreateNodeConfig(cNodeTypeLocalSM, 100);

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({cRunnerRunc}, {}, {}, Quotas({}, {}, 4000, 4096));
    testData.mServiceConfigs[cService2] = CreateServiceConfig({cRunnerRunc}, {}, {}, Quotas({}, {}, 4000, 4096), true);

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 0, 1));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService2, cSubject1, 0, 1));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mLayers.push_back(CreateLayerInfo(cLayer1, cLayer1LocalURL));
    localSMRequest.mServices.push_back(CreateServiceInfo(cService2, 5001, cService2LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 0}, 5000, "2", 0));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, "", ErrorEnum::eNotFound));

    return testData;
}

TestData TestItemRebalancing()
{
    TestData testData;
    testData.mTestCaseName = "rebalancing";
    testData.mRebalancing  = true;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM]
        = CreateNodeConfig(cNodeTypeLocalSM, 100, {}, {}, {}, AlertRulesCPU(75.0, 85.0));
    testData.mNodeConfigs[cNodeTypeRemoteSM] = CreateNodeConfig(cNodeTypeRemoteSM, 50);

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({}, {}, {}, Quotas({}, {}, 1000, {}));
    testData.mServiceConfigs[cService2] = CreateServiceConfig({}, {}, {}, Quotas({}, {}, 1000, {}));
    testData.mServiceConfigs[cService3] = CreateServiceConfig({}, {}, {}, Quotas({}, {}, 1000, {}));

    // Monitoring data
    testData.mMonitoring[cNodeIDLocalSM] = CreateNodeMonitoring(cNodeIDLocalSM, 1000,
        {InstanceMonitoring(InstanceIdent {cService1, cSubject1, 0}, 500),
            InstanceMonitoring(InstanceIdent {cService2, cSubject1, 0}, 500)});

    // Stored instances
    storage::InstanceInfo storedInstance1
        = {InstanceIdent {cService1, cSubject1, 0}, cNodeIDLocalSM, "", 5000, Time::Now(), InstanceStateEnum::eActive};
    testData.mStoredInstances.PushBack(storedInstance1);

    storage::InstanceInfo storedInstance2
        = {InstanceIdent {cService2, cSubject1, 0}, cNodeIDLocalSM, "", 5001, Time::Now(), InstanceStateEnum::eActive};
    testData.mStoredInstances.PushBack(storedInstance2);

    storage::InstanceInfo storedInstance3 = {
        InstanceIdent {cService3, cSubject1, 0}, cNodeIDRemoteSM1, "", 5002, Time::Now(), InstanceStateEnum::eActive};
    testData.mStoredInstances.PushBack(storedInstance3);

    // Desired instances with priorities
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 1));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService2, cSubject1, 50, 1));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService3, cSubject1, 0, 1));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "2", 100));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    nodemanager::StartRequest remoteSM1Request;
    remoteSM1Request.mServices.push_back(CreateServiceInfo(cService2, 5001, cService2RemoteURL));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 0}, 5001, "3", 50));
    testData.mExpectedRunRequests[cNodeIDRemoteSM1] = remoteSM1Request;

    nodemanager::StartRequest remoteSM2Request;
    remoteSM2Request.mServices.push_back(CreateServiceInfo(cService3, 5002, cService3RemoteURL));
    remoteSM2Request.mInstances.push_back(CreateInstanceInfo({cService3, cSubject1, 0}, 5002, "4", 0));
    testData.mExpectedRunRequests[cNodeIDRemoteSM2] = remoteSM2Request;

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 0}, cNodeIDRemoteSM2, Error()));

    return testData;
}

TestData TestItemRebalancingPolicy()
{
    TestData testData;
    testData.mTestCaseName = "rebalancing policy";
    testData.mRebalancing  = true;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM]
        = CreateNodeConfig(cNodeTypeLocalSM, 100, {}, {}, {}, AlertRulesCPU(75.0, 85.0));
    testData.mNodeConfigs[cNodeTypeRemoteSM] = CreateNodeConfig(cNodeTypeRemoteSM, 50);

    // Service configs
    testData.mServiceConfigs[cService1]                  = CreateServiceConfig({}, {}, {}, Quotas({}, {}, 1000, {}));
    testData.mServiceConfigs[cService2]                  = CreateServiceConfig({}, {}, {}, Quotas({}, {}, 1000, {}));
    testData.mServiceConfigs[cService3]                  = CreateServiceConfig({}, {}, {}, Quotas({}, {}, 1000, {}));
    testData.mServiceConfigs[cService3].mBalancingPolicy = "disabled";

    // Monitoring data
    testData.mMonitoring[cNodeIDLocalSM] = CreateNodeMonitoring(cNodeIDLocalSM, 1000,
        {InstanceMonitoring(InstanceIdent {cService1, cSubject1, 0}, 500),
            InstanceMonitoring(InstanceIdent {cService2, cSubject1, 0}, 500)});

    // Stored instances
    storage::InstanceInfo storedInstance1
        = {InstanceIdent {cService1, cSubject1, 0}, cNodeIDLocalSM, "", 5000, Time::Now(), InstanceStateEnum::eActive};
    testData.mStoredInstances.PushBack(storedInstance1);

    storage::InstanceInfo storedInstance2
        = {InstanceIdent {cService2, cSubject1, 0}, cNodeIDLocalSM, "", 5001, Time::Now(), InstanceStateEnum::eActive};
    testData.mStoredInstances.PushBack(storedInstance2);

    storage::InstanceInfo storedInstance3 = {
        InstanceIdent {cService3, cSubject1, 0}, cNodeIDRemoteSM1, "", 5002, Time::Now(), InstanceStateEnum::eActive};
    testData.mStoredInstances.PushBack(storedInstance3);

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 1));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService2, cSubject1, 50, 1));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService3, cSubject1, 0, 1));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "2", 100));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    nodemanager::StartRequest remoteSM1Request;
    remoteSM1Request.mServices.push_back(CreateServiceInfo(cService3, 5002, cService3RemoteURL));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService3, cSubject1, 0}, 5002, "3", 0));
    testData.mExpectedRunRequests[cNodeIDRemoteSM1] = remoteSM1Request;

    nodemanager::StartRequest remoteSM2Request;
    remoteSM2Request.mServices.push_back(CreateServiceInfo(cService2, 5001, cService2RemoteURL));
    remoteSM2Request.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 0}, 5001, "4", 50));
    testData.mExpectedRunRequests[cNodeIDRemoteSM2] = remoteSM2Request;

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 0}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDRemoteSM2, Error()));

    return testData;
}

TestData TestItemRebalancingPrevNode()
{
    TestData testData;
    testData.mTestCaseName = "rebalancing prev node";
    testData.mRebalancing  = true;

    // Node configs
    testData.mNodeConfigs[cNodeTypeLocalSM]
        = CreateNodeConfig(cNodeTypeLocalSM, 100, {}, {}, {}, AlertRulesCPU(75.0, 85.0));
    testData.mNodeConfigs[cNodeTypeRemoteSM] = CreateNodeConfig(cNodeTypeRemoteSM, 50);

    // Service configs
    testData.mServiceConfigs[cService1] = CreateServiceConfig({}, {}, {}, Quotas({}, {}, 1000, {}));
    testData.mServiceConfigs[cService2] = CreateServiceConfig({}, {}, {}, Quotas({}, {}, 1000, {}));
    testData.mServiceConfigs[cService3] = CreateServiceConfig({}, {}, {}, Quotas({}, {}, 1000, {}));

    // Monitoring data
    testData.mMonitoring[cNodeIDLocalSM] = CreateNodeMonitoring(cNodeIDLocalSM, 1000,
        {InstanceMonitoring(InstanceIdent {cService1, cSubject1, 0}, 500),
            InstanceMonitoring(InstanceIdent {cService2, cSubject1, 0}, 500)});

    // Stored instances
    storage::InstanceInfo storedInstance1 = {InstanceIdent {cService1, cSubject1, 0}, cNodeIDLocalSM, cNodeIDLocalSM,
        5000, Time::Now(), InstanceStateEnum::eActive};
    testData.mStoredInstances.PushBack(storedInstance1);

    storage::InstanceInfo storedInstance2 = {InstanceIdent {cService2, cSubject1, 0}, cNodeIDLocalSM, cNodeIDRemoteSM1,
        5001, Time::Now(), InstanceStateEnum::eActive};
    testData.mStoredInstances.PushBack(storedInstance2);

    storage::InstanceInfo storedInstance3 = {InstanceIdent {cService3, cSubject1, 0}, cNodeIDRemoteSM1,
        cNodeIDRemoteSM2, 5002, Time::Now(), InstanceStateEnum::eActive};
    testData.mStoredInstances.PushBack(storedInstance3);

    // Desired instances
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 1));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService2, cSubject1, 50, 1));
    testData.mDesiredInstances.PushBack(CreateRunServiceRequest(cService3, cSubject1, 0, 1));

    // Expected run requests
    nodemanager::StartRequest localSMRequest;
    localSMRequest.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1LocalURL));
    localSMRequest.mInstances.push_back(CreateInstanceInfo({cService1, cSubject1, 0}, 5000, "2", 100));
    testData.mExpectedRunRequests[cNodeIDLocalSM] = localSMRequest;

    nodemanager::StartRequest remoteSM1Request;
    remoteSM1Request.mServices.push_back(CreateServiceInfo(cService3, 5002, cService3RemoteURL));
    remoteSM1Request.mInstances.push_back(CreateInstanceInfo({cService3, cSubject1, 0}, 5002, "3", 0));
    testData.mExpectedRunRequests[cNodeIDRemoteSM1] = remoteSM1Request;

    nodemanager::StartRequest remoteSM2Request;
    remoteSM2Request.mServices.push_back(CreateServiceInfo(cService2, 5001, cService2RemoteURL));
    remoteSM2Request.mInstances.push_back(CreateInstanceInfo({cService2, cSubject1, 0}, 5001, "4", 50));
    testData.mExpectedRunRequests[cNodeIDRemoteSM2] = remoteSM2Request;

    // Expected run status
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService1, cSubject1, 0}, cNodeIDLocalSM, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService3, cSubject1, 0}, cNodeIDRemoteSM1, Error()));
    testData.mExpectedRunStatus.PushBack(CreateInstanceStatus({cService2, cSubject1, 0}, cNodeIDRemoteSM2, Error()));

    return testData;
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CMLauncherTest, InstancesWithRemovedServiceInfoAreRemovedOnStart)
{
    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;
    cfg.mServiceTTL             = 1 * Time::cSeconds;

    mImageProvider.Init();
    mNetworkManager.Init();
    mNodeInfoProvider.Init(cNodeIDLocalSM);
    mNodeManager.Init();
    mResourceManager.Init();
    mStorageState.Init();
    mStorage.Init(Array<storage::InstanceInfo>());

    ASSERT_TRUE(mStorage
                    .AddInstance(storage::InstanceInfo {
                        {"", "SubjectID", 0}, "", "", 5000, Time::Now(), InstanceStateEnum::eCached})
                    .IsNone());

    ASSERT_TRUE(mLauncher
                    .Init(cfg, mStorage, mNodeInfoProvider, mNodeManager, mImageProvider, mResourceManager,
                        mStorageState, mNetworkManager)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    StaticArray<storage::InstanceInfo, cMaxNumInstances> instances;
    ASSERT_TRUE(mStorage.GetInstances(instances).IsNone());
    ASSERT_EQ(instances.Size(), 0);

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, InstancesWithOutdatedTTLRemovedOnStart)
{
    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;
    cfg.mServiceTTL             = 1 * Time::cHours;

    mImageProvider.Init();
    mNetworkManager.Init();
    mNodeInfoProvider.Init(cNodeIDLocalSM);
    mNodeManager.Init();
    mResourceManager.Init();
    mStorageState.Init();
    mStorage.Init(Array<storage::InstanceInfo>());

    // Add outdated TTL.
    ASSERT_TRUE(mStorage
                    .AddInstance(storage::InstanceInfo {{cService1, "", 0}, "", "", 5000,
                        Time::Now().Add(-25 * Time::cHours), InstanceStateEnum::eCached})
                    .IsNone());

    // Add instance with current timestamp.
    ASSERT_TRUE(mStorage
                    .AddInstance(storage::InstanceInfo {
                        {cService2, "", 0}, "", "", 5001, Time::Now(), InstanceStateEnum::eCached})
                    .IsNone());

    // Add services to the image provider.
    mImageProvider.AddService(cService1, CreateExServiceInfo(cService1, 0, ""));
    mImageProvider.AddService(cService2, CreateExServiceInfo(cService2, 0, ""));

    ASSERT_TRUE(mLauncher
                    .Init(cfg, mStorage, mNodeInfoProvider, mNodeManager, mImageProvider, mResourceManager,
                        mStorageState, mNetworkManager)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    ASSERT_EQ(mStorageState.GetRemovedInstances().size(), 1);
    ASSERT_EQ(mStorageState.GetRemovedInstances()[0].mServiceID, cService1);

    StaticArray<storage::InstanceInfo, cMaxNumInstances> instances;

    ASSERT_TRUE(mStorage.GetInstances(instances).IsNone());
    ASSERT_EQ(instances.Size(), 1);
    ASSERT_EQ(instances[0].mInstanceID.mServiceID, cService2);

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, InstancesAreRemovedViaListener)
{
    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;
    cfg.mServiceTTL             = 1 * Time::cHours;

    mImageProvider.Init();
    mNetworkManager.Init();
    mNodeInfoProvider.Init(cNodeIDLocalSM);
    mNodeManager.Init();
    mResourceManager.Init();
    mStorageState.Init();
    mStorage.Init(Array<storage::InstanceInfo>());

    // Add active instance
    ASSERT_TRUE(mStorage
                    .AddInstance(storage::InstanceInfo {
                        {cService1, "", 0}, "", "", 5000, Time::Now(), InstanceStateEnum::eActive})
                    .IsNone());

    ASSERT_TRUE(mLauncher
                    .Init(cfg, mStorage, mNodeInfoProvider, mNodeManager, mImageProvider, mResourceManager,
                        mStorageState, mNetworkManager)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    mImageProvider.RemoveService(cService1);

    for (int i = 0; i < 3; ++i) {
        sleep(i);

        StaticArray<storage::InstanceInfo, cMaxNumInstances> instances;

        EXPECT_TRUE(mStorage.GetInstances(instances).IsNone());

        EXPECT_EQ(instances.Size(), 0);
        EXPECT_EQ(mStorageState.GetRemovedInstances().size(), 1);
        EXPECT_EQ(mStorageState.GetRemovedInstances()[0].mServiceID, cService1);
    }

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

TEST_F(CMLauncherTest, InitialStatus)
{
    const auto nodeIDs = {cNodeIDLocalSM, cNodeIDRemoteSM1};

    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    mImageProvider.Init();
    mNetworkManager.Init();
    mNodeInfoProvider.Init(cNodeIDLocalSM);
    mNodeManager.Init();
    mResourceManager.Init();
    mStorageState.Init();
    mStorage.Init(Array<storage::InstanceInfo>());

    for (const auto& nodeID : nodeIDs) {
        NodeInfo nodeInfo;
        nodeInfo.mNodeID   = nodeID;
        nodeInfo.mNodeType = "nodeType";
        nodeInfo.mStatus   = NodeStatusEnum::eProvisioned;
        nodeInfo.mTotalRAM = 100;

        CPUInfo cpuInfo;
        cpuInfo.mModelName = "Intel(R) Core(TM) i7-1185G7";
        nodeInfo.mCPUs.PushBack(cpuInfo);

        PartitionInfo partitionInfo;
        partitionInfo.mName      = "id";
        partitionInfo.mTotalSize = 200;
        nodeInfo.mPartitions.PushBack(partitionInfo);

        mNodeInfoProvider.AddNodeInfo(nodeID, nodeInfo);
    }

    ASSERT_TRUE(mLauncher
                    .Init(cfg, mStorage, mNodeInfoProvider, mNodeManager, mImageProvider, mResourceManager,
                        mStorageState, mNetworkManager)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    MockRunStatusListener runStatusListener;
    mLauncher.SetListener(runStatusListener);

    int                                                        i = 0;
    StaticArray<nodemanager::InstanceStatus, cMaxNumInstances> expectedRunStatus;

    for (const auto& nodeID : nodeIDs) {
        nodemanager::InstanceStatus instanceStatus;
        instanceStatus.mInstanceIdent  = {cService1, cSubject1, static_cast<uint64_t>(i++)};
        instanceStatus.mServiceVersion = "1.0";
        instanceStatus.mStateChecksum  = cMagicSum;
        instanceStatus.mRunState       = InstanceRunStateEnum::eActive;
        instanceStatus.mNodeID         = nodeID;

        expectedRunStatus.PushBack(instanceStatus);
    }

    EXPECT_CALL(runStatusListener, OnRunStatusChanged(expectedRunStatus));

    for (const auto& status : expectedRunStatus) {
        StaticArray<nodemanager::InstanceStatus, 1> instances;
        instances.PushBack(status);

        mNodeManager.SendRunStatus(nodemanager::NodeRunInstanceStatus {status.mNodeID, "", instances});
    }

    ASSERT_TRUE(mLauncher.Stop().IsNone());

    mLauncher.ResetListener();
}

TEST_F(CMLauncherTest, Balancing)
{
    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cMinutes;

    mNodeInfoProvider.Init(cNodeIDLocalSM);

    // Set up node info
    NodeInfo nodeInfoLocalSM;
    nodeInfoLocalSM.mNodeID   = cNodeIDLocalSM;
    nodeInfoLocalSM.mNodeType = cNodeTypeLocalSM;
    nodeInfoLocalSM.mStatus   = NodeStatusEnum::eProvisioned;
    nodeInfoLocalSM.mAttrs.PushBack(NodeAttribute {cNodeRunners, cRunnerRunc});
    nodeInfoLocalSM.mMaxDMIPS = 1000;
    nodeInfoLocalSM.mTotalRAM = 1024;
    nodeInfoLocalSM.mPartitions.PushBack(CreatePartitionInfo(cStoragePartition, cStoragePartition, 1024));
    nodeInfoLocalSM.mPartitions.PushBack(CreatePartitionInfo(cStatePartition, cStatePartition, 1024));
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    NodeInfo nodeInfoRemoteSM1;
    nodeInfoRemoteSM1.mNodeID   = cNodeIDRemoteSM1;
    nodeInfoRemoteSM1.mNodeType = cNodeTypeRemoteSM;
    nodeInfoRemoteSM1.mStatus   = NodeStatusEnum::eProvisioned;
    nodeInfoRemoteSM1.mAttrs.PushBack(NodeAttribute {cNodeRunners, cRunnerRunc});
    nodeInfoRemoteSM1.mMaxDMIPS = 1000;
    nodeInfoRemoteSM1.mTotalRAM = 1024;
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    NodeInfo nodeInfoRemoteSM2;
    nodeInfoRemoteSM2.mNodeID   = cNodeIDRemoteSM2;
    nodeInfoRemoteSM2.mNodeType = cNodeTypeRemoteSM;
    nodeInfoRemoteSM2.mStatus   = NodeStatusEnum::eProvisioned;
    nodeInfoRemoteSM2.mAttrs.PushBack(NodeAttribute {cNodeRunners, cRunnerRunc});
    nodeInfoRemoteSM2.mMaxDMIPS = 1000;
    nodeInfoRemoteSM2.mTotalRAM = 1024;
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM2, nodeInfoRemoteSM2);

    NodeInfo nodeInfoRunxSM;
    nodeInfoRunxSM.mNodeID   = cNodeIDRunxSM;
    nodeInfoRunxSM.mNodeType = cNodeTypeRunxSM;
    nodeInfoRunxSM.mStatus   = NodeStatusEnum::eProvisioned;
    nodeInfoRunxSM.mAttrs.PushBack(NodeAttribute {cNodeRunners, cRunnerRunx});
    nodeInfoRunxSM.mMaxDMIPS = 1000;
    nodeInfoRunxSM.mTotalRAM = 1024;
    mNodeInfoProvider.AddNodeInfo(cNodeIDRunxSM, nodeInfoRunxSM);

    mImageProvider.Init();
    // Set up services
    imageprovider::ServiceInfo service1Info = CreateExServiceInfo(cService1, 5000, cService1LocalURL);
    service1Info.mRemoteURL                 = cService1RemoteURL;
    service1Info.mLayerDigests.PushBack(cLayer1);
    service1Info.mLayerDigests.PushBack(cLayer2);
    mImageProvider.AddService(cService1, service1Info);

    imageprovider::ServiceInfo service2Info = CreateExServiceInfo(cService2, 5001, cService2LocalURL);
    service2Info.mRemoteURL                 = cService2RemoteURL;
    service2Info.mLayerDigests.PushBack(cLayer1);
    mImageProvider.AddService(cService2, service2Info);

    imageprovider::ServiceInfo service3Info = CreateExServiceInfo(cService3, 5002, cService3LocalURL);
    service3Info.mRemoteURL                 = cService3RemoteURL;
    mImageProvider.AddService(cService3, service3Info);

    // Set up layers
    imageprovider::LayerInfo layer1Info = CreateExLayerInfo(cLayer1, cLayer1LocalURL);
    layer1Info.mRemoteURL               = cLayer1RemoteURL;
    mImageProvider.AddLayer(cLayer1, layer1Info);

    imageprovider::LayerInfo layer2Info = CreateExLayerInfo(cLayer2, cLayer2LocalURL);
    layer2Info.mRemoteURL               = cLayer2RemoteURL;
    mImageProvider.AddLayer(cLayer2, layer2Info);

    // Test data array
    std::vector<TestData> testItems
        = {TestItemNodePriority(), TestItemLabels(), TestItemResources(), TestItemDevices(), TestItemStorageRatio(),
            TestItemStateRatio(), TestItemCPURatio(), TestItemRAMRatio(), TestItemSkipResourceLimits()};

    for (size_t i = 0; i < testItems.size(); ++i) {
        const auto& testItem = testItems[i];

        LOG_INF();
        LOG_INF() << "Test case: " << testItem.mTestCaseName;

        mNetworkManager.Init();
        mNodeManager.Init();
        mStorageState.Init();
        mStorage.Init(Array<storage::InstanceInfo>());
        mResourceManager.Init(testItem.mNodeConfigs);

        // Set up service configs
        for (const auto& [serviceID, config] : testItem.mServiceConfigs) {
            imageprovider::ServiceInfo info;
            ASSERT_TRUE(mImageProvider.GetServiceInfo(serviceID.c_str(), info).IsNone());

            info.mConfig = config;
            mImageProvider.AddService(serviceID.c_str(), info);
        }

        mStorage.Init(testItem.mStoredInstances);

        ASSERT_TRUE(mLauncher
                        .Init(cfg, mStorage, mNodeInfoProvider, mNodeManager, mImageProvider, mResourceManager,
                            mStorageState, mNetworkManager)
                        .IsNone());

        ASSERT_TRUE(mLauncher.Start().IsNone());

        MockRunStatusListener runStatusListener;
        mLauncher.SetListener(runStatusListener);

        // Wait initial run status for all nodes.
        EXPECT_CALL(runStatusListener, OnRunStatusChanged(Array<nodemanager::InstanceStatus>()));

        const std::vector<const char*> nodeIDs = {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2, cNodeIDRunxSM};
        const std::vector<const char*> nodeTypes
            = {cNodeTypeLocalSM, cNodeTypeRemoteSM, cNodeTypeRemoteSM, cNodeTypeRunxSM};

        for (size_t nodeIdx = 0; nodeIdx < nodeIDs.size(); ++nodeIdx) {
            nodemanager::NodeRunInstanceStatus nodeRunStatus;

            nodeRunStatus.mNodeID   = nodeIDs[nodeIdx];
            nodeRunStatus.mNodeType = nodeTypes[nodeIdx];
            nodeRunStatus.mInstances.Clear();

            mNodeManager.SendRunStatus(nodeRunStatus);
        }

        // Run instances
        StaticArray<nodemanager::InstanceStatus, cMaxNumInstances> actualStatus;

        EXPECT_CALL(runStatusListener, OnRunStatusChanged(_))
            .Times(AnyNumber())
            .WillRepeatedly(SaveArg<0>(&actualStatus));
        ASSERT_TRUE(mLauncher.RunInstances(testItem.mDesiredInstances, testItem.mRebalancing).IsNone());
        EXPECT_EQ(actualStatus, testItem.mExpectedRunStatus);

        ASSERT_TRUE(mLauncher.Stop().IsNone());
    }
}

TEST_F(CMLauncherTest, Rebalancing)
{
    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cSeconds;

    mNodeInfoProvider.Init(cNodeIDLocalSM);

    // Set up local SM node info
    NodeInfo nodeInfoLocalSM;
    nodeInfoLocalSM.mNodeID   = cNodeIDLocalSM;
    nodeInfoLocalSM.mNodeType = cNodeTypeLocalSM;
    nodeInfoLocalSM.mStatus   = NodeStatusEnum::eProvisioned;
    nodeInfoLocalSM.mAttrs.PushBack(NodeAttribute {cNodeRunners, cRunnerRunc});
    nodeInfoLocalSM.mMaxDMIPS = 1000;
    nodeInfoLocalSM.mTotalRAM = 1024;
    nodeInfoLocalSM.mPartitions.PushBack(CreatePartitionInfo(cStoragePartition, cStoragePartition, 1024));
    nodeInfoLocalSM.mPartitions.PushBack(CreatePartitionInfo(cStatePartition, cStatePartition, 1024));
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    // Set up remote SM1 node info
    NodeInfo nodeInfoRemoteSM1;
    nodeInfoRemoteSM1.mNodeID   = cNodeIDRemoteSM1;
    nodeInfoRemoteSM1.mNodeType = cNodeTypeRemoteSM;
    nodeInfoRemoteSM1.mStatus   = NodeStatusEnum::eProvisioned;
    nodeInfoRemoteSM1.mAttrs.PushBack(NodeAttribute {cNodeRunners, cRunnerRunc});
    nodeInfoRemoteSM1.mMaxDMIPS = 1000;
    nodeInfoRemoteSM1.mTotalRAM = 1024;
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM1, nodeInfoRemoteSM1);

    // Set up remote SM2 node info
    NodeInfo nodeInfoRemoteSM2;
    nodeInfoRemoteSM2.mNodeID   = cNodeIDRemoteSM2;
    nodeInfoRemoteSM2.mNodeType = cNodeTypeRemoteSM;
    nodeInfoRemoteSM2.mStatus   = NodeStatusEnum::eProvisioned;
    nodeInfoRemoteSM2.mAttrs.PushBack(NodeAttribute {cNodeRunners, cRunnerRunc});
    nodeInfoRemoteSM2.mMaxDMIPS = 1000;
    nodeInfoRemoteSM2.mTotalRAM = 1024;
    mNodeInfoProvider.AddNodeInfo(cNodeIDRemoteSM2, nodeInfoRemoteSM2);

    mImageProvider.Init();

    // Set up services
    imageprovider::ServiceInfo service1Info = CreateExServiceInfo(cService1, 5000, cService1LocalURL);
    service1Info.mRemoteURL                 = cService1RemoteURL;
    mImageProvider.AddService(cService1, service1Info);

    imageprovider::ServiceInfo service2Info = CreateExServiceInfo(cService2, 5001, cService2LocalURL);
    service2Info.mRemoteURL                 = cService2RemoteURL;
    mImageProvider.AddService(cService2, service2Info);

    imageprovider::ServiceInfo service3Info = CreateExServiceInfo(cService3, 5002, cService3LocalURL);
    service3Info.mRemoteURL                 = cService3RemoteURL;
    mImageProvider.AddService(cService3, service3Info);

    imageprovider::LayerInfo layer1Info = CreateExLayerInfo(cLayer1, cLayer1LocalURL);
    layer1Info.mRemoteURL               = cLayer1RemoteURL;
    mImageProvider.AddLayer(cLayer1, layer1Info);

    // Test data array for rebalancing scenarios
    std::vector<TestData> testItems
        = {TestItemRebalancing(), TestItemRebalancingPolicy(), TestItemRebalancingPrevNode()};

    for (size_t i = 0; i < testItems.size(); ++i) {
        const auto& testItem = testItems[i];

        LOG_INF();
        LOG_INF() << "Test case: " << testItem.mTestCaseName;

        // Initialize stubs.
        mNetworkManager.Init();
        mNodeManager.Init();
        mStorageState.Init();
        mStorage.Init(Array<storage::InstanceInfo>());
        mResourceManager.Init(testItem.mNodeConfigs);

        for (const auto& [nodeID, monitoring] : testItem.mMonitoring) {
            mNodeManager.AddMonitoring(nodeID.c_str(), monitoring);
        }

        for (const auto& [serviceID, config] : testItem.mServiceConfigs) {
            imageprovider::ServiceInfo info;
            ASSERT_TRUE(mImageProvider.GetServiceInfo(serviceID.c_str(), info).IsNone());

            info.mConfig = config;
            mImageProvider.AddService(serviceID.c_str(), info);
        }

        mStorage.Init(testItem.mStoredInstances);

        // Start launcher.
        ASSERT_TRUE(mLauncher
                        .Init(cfg, mStorage, mNodeInfoProvider, mNodeManager, mImageProvider, mResourceManager,
                            mStorageState, mNetworkManager)
                        .IsNone());

        ASSERT_TRUE(mLauncher.Start().IsNone());

        MockRunStatusListener runStatusListener;
        mLauncher.SetListener(runStatusListener);

        // Wait initial run status for nodes participating in rebalancing
        EXPECT_CALL(runStatusListener, OnRunStatusChanged(Array<nodemanager::InstanceStatus>())).RetiresOnSaturation();

        const std::vector<const char*> nodeIDs   = {cNodeIDLocalSM, cNodeIDRemoteSM1, cNodeIDRemoteSM2};
        const std::vector<const char*> nodeTypes = {cNodeTypeLocalSM, cNodeTypeRemoteSM, cNodeTypeRemoteSM};

        for (size_t nodeIdx = 0; nodeIdx < nodeIDs.size(); ++nodeIdx) {
            nodemanager::NodeRunInstanceStatus nodeRunStatus;

            nodeRunStatus.mNodeID   = nodeIDs[nodeIdx];
            nodeRunStatus.mNodeType = nodeTypes[nodeIdx];
            nodeRunStatus.mInstances.Clear();

            mNodeManager.SendRunStatus(nodeRunStatus);
        }

        // Run instances with rebalancing enabled
        StaticArray<nodemanager::InstanceStatus, cMaxNumInstances> actualStatus;

        EXPECT_CALL(runStatusListener, OnRunStatusChanged(_))
            .Times(AnyNumber())
            .WillRepeatedly(SaveArg<0>(&actualStatus));
        ASSERT_TRUE(mLauncher.RunInstances(testItem.mDesiredInstances, testItem.mRebalancing).IsNone());
        EXPECT_EQ(actualStatus, testItem.mExpectedRunStatus);

        ASSERT_TRUE(mNodeManager.CompareStartRequests(testItem.mExpectedRunRequests).IsNone());

        ASSERT_TRUE(mLauncher.Stop().IsNone());
    }
}

TEST_F(CMLauncherTest, StorageCleanup)
{
    Config cfg;
    cfg.mNodesConnectionTimeout = 1 * Time::cSeconds;

    mNodeInfoProvider.Init(cNodeIDLocalSM);

    // Set up local SM node info
    NodeInfo nodeInfoLocalSM;
    nodeInfoLocalSM.mNodeID   = cNodeIDLocalSM;
    nodeInfoLocalSM.mNodeType = cNodeTypeLocalSM;
    nodeInfoLocalSM.mStatus   = NodeStatusEnum::eProvisioned;
    nodeInfoLocalSM.mAttrs.PushBack(NodeAttribute {cNodeRunners, cRunnerRunc});
    mNodeInfoProvider.AddNodeInfo(cNodeIDLocalSM, nodeInfoLocalSM);

    // Set up runx SM node info
    NodeInfo nodeInfoRunxSM;
    nodeInfoRunxSM.mNodeID   = cNodeIDRunxSM;
    nodeInfoRunxSM.mNodeType = cNodeTypeRunxSM;
    nodeInfoRunxSM.mStatus   = NodeStatusEnum::eProvisioned;
    nodeInfoRunxSM.mAttrs.PushBack(NodeAttribute {cNodeRunners, cRunnerRunx});
    mNodeInfoProvider.AddNodeInfo(cNodeIDRunxSM, nodeInfoRunxSM);

    // Set up node configurations
    std::map<std::string, NodeConfig> nodeConfigs;
    nodeConfigs[cNodeTypeLocalSM] = CreateNodeConfig(cNodeTypeLocalSM, 100);
    nodeConfigs[cNodeTypeRunxSM]  = CreateNodeConfig(cNodeTypeRunxSM, 0);

    mImageProvider.Init();

    // Set up services with configurations
    imageprovider::ServiceInfo service1Info = CreateExServiceInfo(cService1, 5000, cService1LocalURL);
    service1Info.mRemoteURL                 = cService1RemoteURL;
    service1Info.mConfig.mRunners.PushBack(cRunnerRunc);
    mImageProvider.AddService(cService1, service1Info);

    imageprovider::ServiceInfo service2Info = CreateExServiceInfo(cService2, 5001, cService2LocalURL);
    service2Info.mRemoteURL                 = cService2RemoteURL;
    service2Info.mConfig.mRunners.PushBack(cRunnerRunc);
    mImageProvider.AddService(cService2, service2Info);

    imageprovider::ServiceInfo service3Info = CreateExServiceInfo(cService3, 5002, cService3LocalURL);
    service3Info.mRemoteURL                 = cService3RemoteURL;
    service3Info.mConfig.mRunners.PushBack(cRunnerRunx);
    mImageProvider.AddService(cService3, service3Info);

    // Initialize components
    mNetworkManager.Init();
    mNodeManager.Init();
    mStorageState.Init();
    mStorage.Init(Array<storage::InstanceInfo>());
    mResourceManager.Init(nodeConfigs);

    ASSERT_TRUE(mLauncher
                    .Init(cfg, mStorage, mNodeInfoProvider, mNodeManager, mImageProvider, mResourceManager,
                        mStorageState, mNetworkManager)
                    .IsNone());

    ASSERT_TRUE(mLauncher.Start().IsNone());

    MockRunStatusListener runStatusListener;
    mLauncher.SetListener(runStatusListener);

    // Wait initial run status for all nodes
    EXPECT_CALL(runStatusListener, OnRunStatusChanged(Array<nodemanager::InstanceStatus>()));

    const std::vector<const char*> nodeIDs   = {cNodeIDLocalSM, cNodeIDRunxSM};
    const std::vector<const char*> nodeTypes = {cNodeTypeLocalSM, cNodeTypeRunxSM};

    for (size_t nodeIdx = 0; nodeIdx < nodeIDs.size(); ++nodeIdx) {
        nodemanager::NodeRunInstanceStatus nodeRunStatus;

        nodeRunStatus.mNodeID   = nodeIDs[nodeIdx];
        nodeRunStatus.mNodeType = nodeTypes[nodeIdx];
        nodeRunStatus.mInstances.Clear();

        mNodeManager.SendRunStatus(nodeRunStatus);
    }

    // 1st run
    StaticArray<RunServiceRequest, cMaxNumInstances> desiredInstances1;

    desiredInstances1.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 2));
    desiredInstances1.PushBack(CreateRunServiceRequest(cService2, cSubject1, 100, 1));
    desiredInstances1.PushBack(CreateRunServiceRequest(cService3, cSubject1, 100, 1));

    std::map<std::string, nodemanager::StartRequest> expectedRunRequests1;

    nodemanager::StartRequest localSMRequest1;
    nodemanager::StartRequest runxSMRequest1;

    localSMRequest1.mServices.push_back(CreateServiceInfo(cService1, 5000, cService1LocalURL));
    localSMRequest1.mServices.push_back(CreateServiceInfo(cService2, 5001, cService2LocalURL));
    localSMRequest1.mInstances.push_back(CreateInstanceInfo(InstanceIdent {cService1, cSubject1, 0}, 5000, "2", 100));
    localSMRequest1.mInstances.push_back(CreateInstanceInfo(InstanceIdent {cService1, cSubject1, 1}, 5001, "3", 100));
    localSMRequest1.mInstances.push_back(CreateInstanceInfo(InstanceIdent {cService2, cSubject1, 0}, 5002, "4", 100));

    runxSMRequest1.mServices.push_back(CreateServiceInfo(cService3, 5002, cService3RemoteURL));
    runxSMRequest1.mInstances.push_back(CreateInstanceInfo(InstanceIdent {cService3, cSubject1, 0}, 5003, "5", 100));

    expectedRunRequests1[cNodeIDLocalSM] = localSMRequest1;
    expectedRunRequests1[cNodeIDRunxSM]  = runxSMRequest1;

    // Expected run status
    StaticArray<nodemanager::InstanceStatus, cMaxNumInstances> expectedRunStatus1;
    expectedRunStatus1.PushBack(
        CreateInstanceStatus(InstanceIdent {cService1, cSubject1, 0}, cNodeIDLocalSM, ErrorEnum::eNone));
    expectedRunStatus1.PushBack(
        CreateInstanceStatus(InstanceIdent {cService1, cSubject1, 1}, cNodeIDLocalSM, ErrorEnum::eNone));
    expectedRunStatus1.PushBack(
        CreateInstanceStatus(InstanceIdent {cService2, cSubject1, 0}, cNodeIDLocalSM, ErrorEnum::eNone));
    expectedRunStatus1.PushBack(
        CreateInstanceStatus(InstanceIdent {cService3, cSubject1, 0}, cNodeIDRunxSM, ErrorEnum::eNone));

    // Execute
    EXPECT_CALL(runStatusListener, OnRunStatusChanged(_)).Times(AnyNumber());
    EXPECT_CALL(runStatusListener, OnRunStatusChanged(expectedRunStatus1));
    ASSERT_TRUE(mLauncher.RunInstances(desiredInstances1, false).IsNone());

    ASSERT_TRUE(mNodeManager.CompareStartRequests(expectedRunRequests1).IsNone());

    // 2nd run
    StaticArray<RunServiceRequest, cMaxNumInstances> desiredInstances2;
    desiredInstances2.PushBack(CreateRunServiceRequest(cService1, cSubject1, 100, 1));

    // Expected run status
    StaticArray<nodemanager::InstanceStatus, cMaxNumInstances> expectedRunStatus2;
    expectedRunStatus2.PushBack(
        CreateInstanceStatus(InstanceIdent {cService1, cSubject1, 0}, cNodeIDLocalSM, ErrorEnum::eNone));

    StaticArray<nodemanager::InstanceStatus, cMaxNumInstances> actualStatus;
    EXPECT_CALL(runStatusListener, OnRunStatusChanged(_)).Times(AnyNumber()).WillRepeatedly(SaveArg<0>(&actualStatus));

    ASSERT_TRUE(mLauncher.RunInstances(desiredInstances2, false).IsNone());
    EXPECT_EQ(actualStatus, expectedRunStatus2);

    // Expected cleaned instances
    std::vector<InstanceIdent> expectedCleanInstances;
    expectedCleanInstances.push_back(InstanceIdent {cService1, cSubject1, 1});
    expectedCleanInstances.push_back(InstanceIdent {cService2, cSubject1, 0});
    expectedCleanInstances.push_back(InstanceIdent {cService3, cSubject1, 0});

    const auto actualCleanedInstances = mStorageState.GetCleanedInstances();
    ASSERT_EQ(expectedCleanInstances, actualCleanedInstances);

    ASSERT_TRUE(mLauncher.Stop().IsNone());
}

} // namespace aos::cm::launcher
