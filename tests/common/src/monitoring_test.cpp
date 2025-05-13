/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mutex>
#include <queue>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "aos/common/monitoring/resourcemonitor.hpp"
#include "aos/test/log.hpp"

namespace aos::monitoring {

using namespace testing;

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

static constexpr auto cWaitTimeout = std::chrono::seconds {5};

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

void SetInstancesMonitoringData(
    NodeMonitoringData& nodeMonitoringData, const Array<Pair<String, InstanceMonitoringData>>& instancesData)
{
    nodeMonitoringData.mServiceInstances.Clear();

    for (const auto& [instanceID, instanceData] : instancesData) {
        nodeMonitoringData.mServiceInstances.PushBack(instanceData);
    }
}

class GetAlertVariantVisitor : public StaticVisitor<void> {
public:
    GetAlertVariantVisitor(std::vector<cloudprotocol::SystemQuotaAlert>& systemAlerts,
        std::vector<cloudprotocol::InstanceQuotaAlert>&                  instanceAlerts)
        : mSystemAlerts(systemAlerts)
        , mInstanceAlerts(instanceAlerts)
    {
    }

    void Visit(const cloudprotocol::SystemQuotaAlert& alert) const { mSystemAlerts.push_back(alert); }

    void Visit(const cloudprotocol::InstanceQuotaAlert& alert) const { mInstanceAlerts.push_back(alert); }

    void Visit(...) const { }

private:
    std::vector<cloudprotocol::SystemQuotaAlert>&   mSystemAlerts;
    std::vector<cloudprotocol::InstanceQuotaAlert>& mInstanceAlerts;
};

struct AlertData {
    std::string                mTag;
    std::string                mParameter;
    uint64_t                   mValue;
    cloudprotocol::AlertStatus mStatus;
};

template <typename T>
bool operator==(const AlertData& lhs, const T& rhs)
{
    return lhs.mTag == lhs.mTag && lhs.mParameter == rhs.mParameter.CStr() && lhs.mValue == rhs.mValue
        && lhs.mStatus == rhs.mStatus;
}

/***********************************************************************************************************************
 * Mocks
 **********************************************************************************************************************/

class MockNodeInfoProvider : public iam::nodeinfoprovider::NodeInfoProviderItf {
public:
    MockNodeInfoProvider(const NodeInfo& nodeInfo)
        : mNodeInfo(nodeInfo)
    {
    }

    Error GetNodeInfo(NodeInfo& nodeInfo) const override
    {
        nodeInfo = mNodeInfo;

        return ErrorEnum::eNone;
    }

    Error SetNodeStatus(const NodeStatus& status) override
    {
        (void)status;

        return ErrorEnum::eNone;
    }

    Error SubscribeNodeStatusChanged(iam::nodeinfoprovider::NodeStatusObserverItf& observer)
    {
        (void)observer;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeNodeStatusChanged(iam::nodeinfoprovider::NodeStatusObserverItf& observer)
    {
        (void)observer;

        return ErrorEnum::eNone;
    }

private:
    NodeInfo mNodeInfo {};
};

class MockResourceManager : public sm::resourcemanager::ResourceManagerItf {
public:
    MockResourceManager(Optional<AlertRules> alertRules = {}) { mConfig.mNodeConfig.mAlertRules = alertRules; }

    RetWithError<StaticString<cVersionLen>> GetNodeConfigVersion() const override { return mConfig.mVersion; }

    Error GetNodeConfig(aos::NodeConfig& nodeConfig) const override
    {
        nodeConfig = mConfig.mNodeConfig;

        return ErrorEnum::eNone;
    }

    Error GetDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const override
    {
        (void)deviceName;
        (void)deviceInfo;

        return ErrorEnum::eNone;
    }

    Error GetResourceInfo(const String& resourceName, ResourceInfo& resourceInfo) const override
    {
        (void)resourceName;
        (void)resourceInfo;

        return ErrorEnum::eNone;
    }

    Error AllocateDevice(const String& deviceName, const String& instanceID) override
    {
        (void)deviceName;
        (void)instanceID;

        return ErrorEnum::eNone;
    }

    Error ReleaseDevice(const String& deviceName, const String& instanceID) override
    {
        (void)deviceName;
        (void)instanceID;

        return ErrorEnum::eNone;
    }

    Error ReleaseDevices(const String& instanceID) override
    {
        (void)instanceID;

        return ErrorEnum::eNone;
    }

    Error ResetAllocatedDevices() override { return ErrorEnum::eNone; }

    Error GetDeviceInstances(const String& deviceName, Array<StaticString<cInstanceIDLen>>& instanceIDs) const override
    {
        (void)deviceName;
        (void)instanceIDs;

        return ErrorEnum::eNone;
    }

    Error CheckNodeConfig(const String& version, const String& config) const override
    {
        (void)version;
        (void)config;

        return ErrorEnum::eNone;
    }

    Error UpdateNodeConfig(const String& version, const String& config) override
    {
        (void)version;
        (void)config;

        return ErrorEnum::eNone;
    }

    Error SubscribeCurrentNodeConfigChange(sm::resourcemanager::NodeConfigReceiverItf& receiver) override
    {
        (void)receiver;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeCurrentNodeConfigChange(sm::resourcemanager::NodeConfigReceiverItf& receiver) override
    {
        (void)receiver;

        return ErrorEnum::eNone;
    }

private:
    sm::resourcemanager::NodeConfig mConfig {};
};

class MockResourceUsageProvider : public ResourceUsageProviderItf {
public:
    Error GetNodeMonitoringData(const String& nodeID, MonitoringData& monitoringData) override
    {
        (void)nodeID;

        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] { return !mNodeMonitoringData.empty(); })) {
            return ErrorEnum::eTimeout;
        }

        auto release = DeferRelease(&mNodeMonitoringData, [](auto* data) { data->pop(); });

        monitoringData.mCPU      = mNodeMonitoringData.front().mCPU;
        monitoringData.mRAM      = mNodeMonitoringData.front().mRAM;
        monitoringData.mDownload = mNodeMonitoringData.front().mDownload;
        monitoringData.mUpload   = mNodeMonitoringData.front().mUpload;

        if (monitoringData.mPartitions.Size() != mNodeMonitoringData.front().mPartitions.Size()) {
            return ErrorEnum::eInvalidArgument;
        }

        for (size_t i = 0; i < monitoringData.mPartitions.Size(); i++) {
            monitoringData.mPartitions[i].mUsedSize = mNodeMonitoringData.front().mPartitions[i].mUsedSize;
        }

        return ErrorEnum::eNone;
    }

    Error GetInstanceMonitoringData(const String& instanceID, InstanceMonitoringData& instanceMonitoringData) override
    {
        std::unique_lock lock {mMutex};

        auto findData = [&instanceID](const auto& data) { return data.first == instanceID.CStr(); };

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] {
                return std::find_if(mInstancesMonitoringData.begin(), mInstancesMonitoringData.end(), findData)
                    != mInstancesMonitoringData.end();
            })) {
            return ErrorEnum::eTimeout;
        }

        const auto it = std::find_if(mInstancesMonitoringData.begin(), mInstancesMonitoringData.end(), findData);
        if (it == mInstancesMonitoringData.end()) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        auto release = DeferRelease(&mInstancesMonitoringData, [&it](auto* data) { data->erase(it); });

        const auto& data = it->second;

        instanceMonitoringData.mMonitoringData.mCPU      = data.mMonitoringData.mCPU;
        instanceMonitoringData.mMonitoringData.mRAM      = data.mMonitoringData.mRAM;
        instanceMonitoringData.mMonitoringData.mDownload = data.mMonitoringData.mDownload;
        instanceMonitoringData.mMonitoringData.mUpload   = data.mMonitoringData.mUpload;

        if (instanceMonitoringData.mMonitoringData.mPartitions.Size() != data.mMonitoringData.mPartitions.Size()) {
            return ErrorEnum::eInvalidArgument;
        }

        for (size_t i = 0; i < instanceMonitoringData.mMonitoringData.mPartitions.Size(); i++) {
            instanceMonitoringData.mMonitoringData.mPartitions[i].mUsedSize
                = data.mMonitoringData.mPartitions[i].mUsedSize;
        }

        return ErrorEnum::eNone;
    }

    void ProvideMonitoringData(const MonitoringData&       nodeMonitoringData,
        const Array<Pair<String, InstanceMonitoringData>>& instancesMonitoringData)
    {
        std::lock_guard lock {mMutex};

        mNodeMonitoringData.push(nodeMonitoringData);

        for (const auto& instanceData : instancesMonitoringData) {
            mInstancesMonitoringData.push_back({instanceData.mFirst.CStr(), instanceData.mSecond});
        }

        mCondVar.notify_one();
    }

    void ProvideInitialInstancesData(const std::vector<std::string>& instanceIDs)
    {
        std::lock_guard lock {mMutex};

        for (const auto& instanceID : instanceIDs) {
            mInstancesMonitoringData.resize(mInstancesMonitoringData.size() + 1);
            mInstancesMonitoringData.back().first = instanceID;
        }

        mCondVar.notify_one();
    }

private:
    std::mutex                                                  mMutex;
    std::condition_variable                                     mCondVar;
    std::queue<MonitoringData>                                  mNodeMonitoringData {};
    std::vector<std::pair<std::string, InstanceMonitoringData>> mInstancesMonitoringData {};
};

class MockSender : public SenderItf {
public:
    Error SendMonitoringData(const NodeMonitoringData& monitoringData) override
    {
        std::lock_guard lock {mMutex};

        mMonitoringData.push(monitoringData);

        mCondVar.notify_one();

        return ErrorEnum::eNone;
    }

    Error WaitMonitoringData(NodeMonitoringData& monitoringData)
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] { return !mMonitoringData.empty(); })) {
            return ErrorEnum::eTimeout;
        }

        monitoringData = mMonitoringData.front();
        mMonitoringData.pop();

        return ErrorEnum::eNone;
    }

private:
    static constexpr auto cWaitTimeout = std::chrono::seconds {5};

    std::mutex                     mMutex;
    std::condition_variable        mCondVar;
    std::queue<NodeMonitoringData> mMonitoringData {};
};

class AlertSenderStub : public alerts::SenderItf {
public:
    Error SendAlert(const cloudprotocol::AlertVariant& alert) override
    {
        std::lock_guard lock {mMutex};

        LOG_DBG() << "Send alert: alert=" << alert;

        GetAlertVariantVisitor visitor {mSystemQuotaAlerts, mInstanceQuotaAlerts};

        alert.ApplyVisitor(visitor);

        return ErrorEnum::eNone;
    }

    std::vector<cloudprotocol::SystemQuotaAlert> GetSystemQuotaAlerts() const
    {
        std::lock_guard lock {mMutex};

        return mSystemQuotaAlerts;
    }

    std::vector<cloudprotocol::InstanceQuotaAlert> GetInstanceQuotaAlerts() const
    {
        std::lock_guard lock {mMutex};

        return mInstanceQuotaAlerts;
    }

private:
    mutable std::mutex                             mMutex;
    std::vector<cloudprotocol::SystemQuotaAlert>   mSystemQuotaAlerts;
    std::vector<cloudprotocol::InstanceQuotaAlert> mInstanceQuotaAlerts;
};

class MockConnectionPublisher : public ConnectionPublisherItf {
public:
    aos::Error Subscribe(ConnectionSubscriberItf& subscriber) override
    {
        mSubscriber = &subscriber;

        return ErrorEnum::eNone;
    }

    void Unsubscribe(ConnectionSubscriberItf& subscriber) override
    {
        EXPECT_TRUE(&subscriber == mSubscriber);

        mSubscriber = nullptr;

        return;
    }

    void NotifyConnect() const
    {

        EXPECT_TRUE(mSubscriber != nullptr);

        mSubscriber->OnConnect();

        return;
    }

private:
    ConnectionSubscriberItf* mSubscriber {};
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class MonitoringTest : public Test {
protected:
    void SetUp() override { test::InitLog(); }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(MonitoringTest, GetNodeMonitoringData)
{
    PartitionInfo nodePartitionsInfo[] = {{"disk1", {}, "", 512, 256}, {"disk2", {}, "", 1024, 512}};
    auto          nodePartitions       = Array<PartitionInfo>(nodePartitionsInfo, ArraySize(nodePartitionsInfo));
    auto          nodeInfo             = NodeInfo {
        "node1", "type1", "name1", NodeStatusEnum::eProvisioned, "linux", {}, nodePartitions, {}, 10000, 8192};

    AlertRules alertRules;
    alertRules.mCPU.SetValue(AlertRulePercents {Time::cMilliseconds, 10, 20});
    alertRules.mRAM.SetValue(AlertRulePercents {Time::cMilliseconds, 20, 30});
    alertRules.mDownload.SetValue(AlertRulePoints {Time::cMilliseconds, 10, 20});
    alertRules.mUpload.SetValue(AlertRulePoints {Time::cMilliseconds, 10, 20});

    for (const auto& partition : nodePartitionsInfo) {
        ASSERT_TRUE(alertRules.mPartitions.PushBack(PartitionAlertRule {Time::cMilliseconds, 10, 20, partition.mName})
                        .IsNone());
    }

    auto nodeInfoProvider      = std::make_unique<MockNodeInfoProvider>(nodeInfo);
    auto resourceManager       = std::make_unique<MockResourceManager>(Optional<AlertRules> {alertRules});
    auto resourceUsageProvider = std::make_unique<MockResourceUsageProvider>();
    auto sender                = std::make_unique<MockSender>();
    auto alertSender           = std::make_unique<AlertSenderStub>();
    auto connectionPublisher   = std::make_unique<MockConnectionPublisher>();

    auto   monitor = std::make_unique<ResourceMonitor>();
    Config config {100 * Time::cMilliseconds, 100 * Time::cMilliseconds};

    EXPECT_TRUE(monitor
                    ->Init(config, *nodeInfoProvider, *resourceManager, *resourceUsageProvider, *sender, *alertSender,
                        *connectionPublisher)
                    .IsNone());
    EXPECT_TRUE(monitor->Start().IsNone());

    connectionPublisher->NotifyConnect();

    PartitionParam partitionParamsData[] = {{"state", ""}, {"storage", ""}};
    auto           partitionParams       = Array<PartitionParam>(partitionParamsData, ArraySize(partitionParamsData));

    PartitionInfo instancePartitionsData[] = {{"state", {}, "", 0, 256}, {"storage", {}, "", 0, 512}};
    auto          instancePartitions = Array<PartitionInfo>(instancePartitionsData, ArraySize(instancePartitionsData));

    InstanceIdent instance0Ident {"service0", "subject0", 0};
    InstanceIdent instance1Ident {"service1", "subject1", 1};

    Pair<String, InstanceMonitoringData> instancesMonitoringData[] = {
        {"instance0", {instance0Ident, {10000, 2048, instancePartitions, 10, 20}}},
        {"instance1", {instance1Ident, {15000, 1024, instancePartitions, 20, 40}}},
    };

    auto providedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

    *providedNodeMonitoringData = {};

    providedNodeMonitoringData->mNodeID         = "node1";
    providedNodeMonitoringData->mMonitoringData = {30000, 8192, nodePartitions, 120, 240};

    SetInstancesMonitoringData(*providedNodeMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));

    resourceUsageProvider->ProvideInitialInstancesData({"instance0", "instance1"});

    EXPECT_TRUE(monitor->StartInstanceMonitoring("instance0", {instance0Ident, partitionParams, 0, 0, {}}).IsNone());
    EXPECT_TRUE(monitor->StartInstanceMonitoring("instance1", {instance1Ident, partitionParams, 0, 0, {}}).IsNone());

    auto receivedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

    resourceUsageProvider->ProvideMonitoringData(providedNodeMonitoringData->mMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));
    EXPECT_TRUE(sender->WaitMonitoringData(*receivedNodeMonitoringData).IsNone());

    providedNodeMonitoringData->mMonitoringData.mCPU
        = providedNodeMonitoringData->mMonitoringData.mCPU * nodeInfo.mMaxDMIPS / 100.0;

    for (auto& instanceMonitoring : providedNodeMonitoringData->mServiceInstances) {
        instanceMonitoring.mMonitoringData.mCPU = instanceMonitoring.mMonitoringData.mCPU * nodeInfo.mMaxDMIPS / 100.0;
    }

    receivedNodeMonitoringData->mTimestamp = providedNodeMonitoringData->mTimestamp;
    EXPECT_EQ(*providedNodeMonitoringData, *receivedNodeMonitoringData);

    EXPECT_TRUE(monitor->Stop().IsNone());
}

TEST_F(MonitoringTest, GetAverageMonitoringData)
{
    PartitionInfo nodePartitionsInfo[] = {{"disk", {}, "", 512, 256}};
    auto          nodePartitions       = Array<PartitionInfo>(nodePartitionsInfo, ArraySize(nodePartitionsInfo));
    auto          nodeInfo             = NodeInfo {
        "node1", "type1", "name1", NodeStatusEnum::eProvisioned, "linux", {}, nodePartitions, {}, 10000, 8192};

    auto nodeInfoProvider      = std::make_unique<MockNodeInfoProvider>(nodeInfo);
    auto resourceManager       = std::make_unique<MockResourceManager>();
    auto resourceUsageProvider = std::make_unique<MockResourceUsageProvider>();
    auto sender                = std::make_unique<MockSender>();
    auto alertSender           = std::make_unique<AlertSenderStub>();
    auto connectionPublisher   = std::make_unique<MockConnectionPublisher>();

    auto   monitor = std::make_unique<ResourceMonitor>();
    Config config {};

    EXPECT_TRUE(monitor
                    ->Init(config, *nodeInfoProvider, *resourceManager, *resourceUsageProvider, *sender, *alertSender,
                        *connectionPublisher)
                    .IsNone());
    EXPECT_TRUE(monitor->Start().IsNone());

    connectionPublisher->NotifyConnect();

    InstanceIdent  instance0Ident {"service0", "subject0", 0};
    PartitionParam partitionParmsData[] = {{"disk", ""}};
    auto           partitionParams      = Array<PartitionParam>(partitionParmsData, ArraySize(partitionParmsData));

    resourceUsageProvider->ProvideInitialInstancesData({"instance0"});

    EXPECT_TRUE(monitor->StartInstanceMonitoring("instance0", {instance0Ident, partitionParams, 0, 0, {}}).IsNone());

    PartitionInfo providedNodeDiskData[][1] = {
        {{"disk", {}, "", 512, 100}},
        {{"disk", {}, "", 512, 400}},
        {{"disk", {}, "", 512, 500}},
    };

    PartitionInfo averageNodeDiskData[][1] = {
        {{"disk", {}, "", 512, 100}},
        {{"disk", {}, "", 512, 200}},
        {{"disk", {}, "", 512, 300}},
    };

    PartitionInfo providedInstanceDiskData[][1] = {
        {{"disk", {}, "", 0, 300}},
        {{"disk", {}, "", 0, 0}},
        {{"disk", {}, "", 0, 800}},
    };

    PartitionInfo averageInstanceDiskData[][1] = {
        {{"disk", {}, "", 0, 300}},
        {{"disk", {}, "", 0, 200}},
        {{"disk", {}, "", 0, 400}},
    };

    std::vector<NodeMonitoringData> providedNodeMonitoringData {
        {"node1", {}, {0, 600, {}, 300, 300}, {}},
        {"node1", {}, {900, 300, {}, 0, 300}, {}},
        {"node1", {}, {1200, 200, {}, 200, 0}, {}},
    };

    std::vector<NodeMonitoringData> averageNodeMonitoringData {
        {"node1", {}, {0, 600, {}, 300, 300}, {}},
        {"node1", {}, {300, 500, {}, 200, 300}, {}},
        {"node1", {}, {600, 400, {}, 200, 200}, {}},
    };

    std::vector<Pair<String, InstanceMonitoringData>> providedInstanceMonitoringData {
        {"instance0", {instance0Ident, {600, 0, {}, 300, 300}}},
        {"instance0", {instance0Ident, {300, 900, {}, 300, 0}}},
        {"instance0", {instance0Ident, {200, 1200, {}, 0, 200}}},
    };

    std::vector<Pair<String, InstanceMonitoringData>> averageInstanceMonitoringData {
        {"instance0", {instance0Ident, {600, 0, {}, 300, 300}}},
        {"instance0", {instance0Ident, {500, 300, {}, 300, 200}}},
        {"instance0", {instance0Ident, {400, 600, {}, 200, 200}}},
    };

    for (size_t i = 0; i < providedNodeMonitoringData.size(); i++) {
        auto receivedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

        *receivedNodeMonitoringData = {};

        providedInstanceMonitoringData[i].mSecond.mMonitoringData.mPartitions
            = Array<PartitionInfo>(providedInstanceDiskData[i], ArraySize(providedInstanceDiskData[i]));
        providedNodeMonitoringData[i].mMonitoringData.mPartitions
            = Array<PartitionInfo>(providedNodeDiskData[i], ArraySize(providedNodeDiskData[i]));

        SetInstancesMonitoringData(providedNodeMonitoringData[i],
            Array<Pair<String, InstanceMonitoringData>>(&providedInstanceMonitoringData[i], 1));

        resourceUsageProvider->ProvideMonitoringData(providedNodeMonitoringData[i].mMonitoringData,
            Array<Pair<String, InstanceMonitoringData>>(&providedInstanceMonitoringData[i], 1));

        EXPECT_TRUE(sender->WaitMonitoringData(*receivedNodeMonitoringData).IsNone());
        EXPECT_TRUE(monitor->GetAverageMonitoringData(*receivedNodeMonitoringData).IsNone());

        averageInstanceMonitoringData[i].mSecond.mMonitoringData.mPartitions
            = Array<PartitionInfo>(averageInstanceDiskData[i], ArraySize(averageInstanceDiskData[i]));
        averageNodeMonitoringData[i].mMonitoringData.mPartitions
            = Array<PartitionInfo>(averageNodeDiskData[i], ArraySize(averageNodeDiskData[i]));

        SetInstancesMonitoringData(averageNodeMonitoringData[i],
            Array<Pair<String, InstanceMonitoringData>>(&averageInstanceMonitoringData[i], 1));

        averageNodeMonitoringData[i].mMonitoringData.mCPU
            = averageNodeMonitoringData[i].mMonitoringData.mCPU * nodeInfo.mMaxDMIPS / 100.0;

        for (auto& instanceMonitoring : averageNodeMonitoringData[i].mServiceInstances) {
            instanceMonitoring.mMonitoringData.mCPU
                = instanceMonitoring.mMonitoringData.mCPU * nodeInfo.mMaxDMIPS / 100.0;
        }

        receivedNodeMonitoringData->mTimestamp = averageNodeMonitoringData[i].mTimestamp;

        EXPECT_EQ(averageNodeMonitoringData[i], *receivedNodeMonitoringData);
    }

    EXPECT_TRUE(monitor->Stop().IsNone());
}

TEST_F(MonitoringTest, QuotaAlertsAreSent)
{
    const auto    maxDmips             = 10000;
    PartitionInfo nodePartitionsInfo[] = {{"disk1", {}, "", 512, 256}, {"disk2", {}, "", 1024, 512},
        {"state", {}, "", 512, 128}, {"storage", {}, "", 1024, 256}};
    auto          nodePartitions       = Array<PartitionInfo>(nodePartitionsInfo, ArraySize(nodePartitionsInfo));
    auto          nodeInfo             = NodeInfo {
        "node1", "type1", "name1", NodeStatusEnum::eProvisioned, "linux", {}, nodePartitions, {}, maxDmips, 8192};

    AlertRules alertRules;
    alertRules.mCPU.SetValue(AlertRulePercents {Time::cMilliseconds, 10.0, 20.0});
    alertRules.mRAM.SetValue(AlertRulePercents {Time::cMilliseconds, 20.0, 30.0});
    alertRules.mDownload.SetValue(AlertRulePoints {Time::cMilliseconds, 10, 20});
    alertRules.mUpload.SetValue(AlertRulePoints {Time::cMilliseconds, 10, 20});

    for (const auto& partition : nodePartitionsInfo) {
        ASSERT_TRUE(
            alertRules.mPartitions.PushBack(PartitionAlertRule {Time::cMilliseconds, 40.0, 50.0, partition.mName})
                .IsNone());
    }

    Config config {100 * Time::cMilliseconds, 100 * Time::cMilliseconds};
    auto   nodeInfoProvider      = std::make_unique<MockNodeInfoProvider>(nodeInfo);
    auto   resourceManager       = std::make_unique<MockResourceManager>(Optional<AlertRules> {alertRules});
    auto   resourceUsageProvider = std::make_unique<MockResourceUsageProvider>();
    auto   sender                = std::make_unique<MockSender>();
    auto   alertSender           = std::make_unique<AlertSenderStub>();
    auto   connectionPublisher   = std::make_unique<MockConnectionPublisher>();

    auto monitor = std::make_unique<ResourceMonitor>();

    EXPECT_TRUE(monitor
                    ->Init(config, *nodeInfoProvider, *resourceManager, *resourceUsageProvider, *sender, *alertSender,
                        *connectionPublisher)
                    .IsNone());
    EXPECT_TRUE(monitor->Start().IsNone());

    connectionPublisher->NotifyConnect();

    auto instancePartitions = Array<PartitionInfo>(&nodePartitionsInfo[2], 2);

    PartitionParam partitionParamsData[] = {{instancePartitions[0].mName, ""}, {instancePartitions[1].mName, ""}};
    auto           partitionParams       = Array<PartitionParam>(partitionParamsData, ArraySize(partitionParamsData));

    auto instanceAlertRules = alertRules;
    instanceAlertRules.mPartitions.Clear();
    instanceAlertRules.mPartitions.PushBack({Time::cMilliseconds, 10.0, 20.0, "state"});
    instanceAlertRules.mPartitions.PushBack({Time::cMilliseconds, 10.0, 20.0, "storage"});

    InstanceIdent instance0Ident {"service0", "subject0", 0};
    InstanceIdent instance1Ident {"service1", "subject1", 1};

    Pair<String, InstanceMonitoringData> instancesMonitoringData[] = {
        {"instance0", {instance0Ident, {100, 2048, instancePartitions, 10, 20}}},
        {"instance1", {instance1Ident, {150, 1024, instancePartitions, 20, 40}}},
    };

    const std::vector<Pair<InstanceIdent, AlertData>> expectedInstanceAlerts = {
        {instance0Ident, {"instanceQuotaAlert", "cpu", 100 * maxDmips / 100, cloudprotocol::AlertStatusEnum::eRaise}},
        {instance0Ident, {"instanceQuotaAlert", "state", 128, cloudprotocol::AlertStatusEnum::eRaise}},
        {instance0Ident, {"instanceQuotaAlert", "storage", 256, cloudprotocol::AlertStatusEnum::eRaise}},
        {instance0Ident, {"instanceQuotaAlert", "download", 20, cloudprotocol::AlertStatusEnum::eRaise}},
        {instance1Ident, {"instanceQuotaAlert", "cpu", 150 * maxDmips / 100, cloudprotocol::AlertStatusEnum::eRaise}},
        {instance1Ident, {"instanceQuotaAlert", "state", 128, cloudprotocol::AlertStatusEnum::eRaise}},
        {instance1Ident, {"instanceQuotaAlert", "storage", 256, cloudprotocol::AlertStatusEnum::eRaise}},
        {instance1Ident, {"instanceQuotaAlert", "download", 20, cloudprotocol::AlertStatusEnum::eRaise}},
    };

    auto providedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

    *providedNodeMonitoringData = {};

    providedNodeMonitoringData->mNodeID         = "node1";
    providedNodeMonitoringData->mTimestamp      = Time::Now().Add(Time::cSeconds);
    providedNodeMonitoringData->mMonitoringData = {100, 8192, nodePartitions, 120, 240};

    const std::vector<AlertData> expectedSystemAlerts = {
        {"systemQuotaAlert", "cpu", maxDmips, cloudprotocol::AlertStatusEnum::eRaise},
        {"systemQuotaAlert", "ram", 8192, cloudprotocol::AlertStatusEnum::eRaise},
        {"systemQuotaAlert", "disk1", 256, cloudprotocol::AlertStatusEnum::eRaise},
        {"systemQuotaAlert", "disk2", 512, cloudprotocol::AlertStatusEnum::eRaise},
        {"systemQuotaAlert", "download", 120, cloudprotocol::AlertStatusEnum::eRaise},
        {"systemQuotaAlert", "upload", 240, cloudprotocol::AlertStatusEnum::eRaise},
    };

    SetInstancesMonitoringData(*providedNodeMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));

    resourceUsageProvider->ProvideInitialInstancesData({"instance0", "instance1"});

    EXPECT_TRUE(monitor
                    ->StartInstanceMonitoring(
                        "instance0", {instance0Ident, partitionParams, 0, 0, Optional<AlertRules> {instanceAlertRules}})
                    .IsNone());
    EXPECT_TRUE(monitor
                    ->StartInstanceMonitoring(
                        "instance1", {instance1Ident, partitionParams, 0, 0, Optional<AlertRules> {instanceAlertRules}})
                    .IsNone());

    auto receivedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

    resourceUsageProvider->ProvideMonitoringData(providedNodeMonitoringData->mMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));
    EXPECT_TRUE(sender->WaitMonitoringData(*receivedNodeMonitoringData).IsNone());

    providedNodeMonitoringData->mTimestamp = providedNodeMonitoringData->mTimestamp.Add(Time::cSeconds);

    resourceUsageProvider->ProvideMonitoringData(providedNodeMonitoringData->mMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));
    EXPECT_TRUE(sender->WaitMonitoringData(*receivedNodeMonitoringData).IsNone());

    EXPECT_TRUE(monitor->Stop().IsNone());

    auto systemQuotaAlerts = alertSender->GetSystemQuotaAlerts();
    ASSERT_FALSE(systemQuotaAlerts.empty());

    ASSERT_EQ(systemQuotaAlerts.size(), expectedSystemAlerts.size());
    for (const auto& received : systemQuotaAlerts) {
        EXPECT_THAT(expectedSystemAlerts, Contains(received));
    }

    auto instanceQuotaAlerts = alertSender->GetInstanceQuotaAlerts();
    ASSERT_FALSE(instanceQuotaAlerts.empty());

    for (const auto& received : instanceQuotaAlerts) {
        EXPECT_NE(std::find_if(expectedInstanceAlerts.begin(), expectedInstanceAlerts.end(),
                      [&](const Pair<InstanceIdent, AlertData>& expected) {
                          return expected.mFirst == received.mInstanceIdent && expected.mSecond == received;
                      }),
            expectedInstanceAlerts.end());
    }
}

TEST_F(MonitoringTest, GetNodeMonitoringDataOnInstanceSpikes)
{
    constexpr auto cNodeDMIPS          = 10000;
    constexpr auto cNodeCPUUsage       = 30.0;
    constexpr auto cInstance0CPUUsage  = 50.0;
    constexpr auto cInstance1CPUUsage  = 25.0;
    constexpr auto cExpectedDMIPSUsage = cNodeDMIPS * (cInstance0CPUUsage + cInstance1CPUUsage) / 100.0;

    constexpr auto cNodeRAMUsage      = 1024;
    constexpr auto cInstance0RAMUsage = 2048;
    constexpr auto cInstance1RAMUsage = 4096;
    constexpr auto cExpectedRAMUsage  = cInstance0RAMUsage + cInstance1RAMUsage;

    constexpr auto cNodeDownload      = 10;
    constexpr auto cInstance0Download = 20;
    constexpr auto cInstance1Download = 40;
    constexpr auto cExpectedDownload  = cInstance0Download + cInstance1Download;

    constexpr auto cNodeUpload      = 20;
    constexpr auto cInstance0Upload = 40;
    constexpr auto cInstance1Upload = 80;
    constexpr auto cExpectedUpload  = cInstance0Upload + cInstance1Upload;

    PartitionInfo nodePartitionsInfo[] = {{"states", {}, "", 1024, 128}, {"storages", {}, "", 1024, 64}};
    auto          nodePartitions       = Array<PartitionInfo>(nodePartitionsInfo, ArraySize(nodePartitionsInfo));
    auto          nodeInfo             = NodeInfo {
        "node1", "type1", "name1", NodeStatusEnum::eProvisioned, "linux", {}, nodePartitions, {}, cNodeDMIPS, 8192};

    auto nodeInfoProvider      = std::make_unique<MockNodeInfoProvider>(nodeInfo);
    auto resourceManager       = std::make_unique<MockResourceManager>();
    auto resourceUsageProvider = std::make_unique<MockResourceUsageProvider>();
    auto sender                = std::make_unique<MockSender>();
    auto alertSender           = std::make_unique<AlertSenderStub>();
    auto connectionPublisher   = std::make_unique<MockConnectionPublisher>();

    auto   monitor = std::make_unique<ResourceMonitor>();
    Config config {100 * Time::cMilliseconds, 100 * Time::cMilliseconds};

    EXPECT_TRUE(monitor
                    ->Init(config, *nodeInfoProvider, *resourceManager, *resourceUsageProvider, *sender, *alertSender,
                        *connectionPublisher)
                    .IsNone());
    EXPECT_TRUE(monitor->Start().IsNone());

    connectionPublisher->NotifyConnect();

    PartitionParam partitionParamsData[] = {{"states", ""}, {"storages", ""}};
    auto           partitionParams       = Array<PartitionParam>(partitionParamsData, ArraySize(partitionParamsData));

    PartitionInfo instancePartitionsData[] = {{"states", {}, "", 0, 256}, {"storages", {}, "", 0, 512}};
    auto          instancePartitions = Array<PartitionInfo>(instancePartitionsData, ArraySize(instancePartitionsData));

    InstanceIdent instance0Ident {"service0", "subject0", 0};
    InstanceIdent instance1Ident {"service1", "subject1", 1};

    Pair<String, InstanceMonitoringData> instancesMonitoringData[] = {
        {"instance0",
            {instance0Ident,
                {cInstance0CPUUsage, cInstance0RAMUsage, instancePartitions, cInstance0Download, cInstance0Upload}}},
        {"instance1",
            {instance1Ident,
                {cInstance1CPUUsage, cInstance1RAMUsage, instancePartitions, cInstance1Download, cInstance1Upload}}},

    };

    auto providedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

    providedNodeMonitoringData->mNodeID = "node1";
    providedNodeMonitoringData->mMonitoringData
        = {cNodeCPUUsage, cNodeRAMUsage, nodePartitions, cNodeDownload, cNodeUpload};

    SetInstancesMonitoringData(*providedNodeMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));

    resourceUsageProvider->ProvideInitialInstancesData({"instance0", "instance1"});

    EXPECT_TRUE(monitor->StartInstanceMonitoring("instance0", {instance0Ident, partitionParams, 0, 0, {}}).IsNone());
    EXPECT_TRUE(monitor->StartInstanceMonitoring("instance1", {instance1Ident, partitionParams, 0, 0, {}}).IsNone());

    auto toDMIPS = [&](double usage) { return usage * cNodeDMIPS / 100.0; };

    auto expectedNodeMonitoringData = std::make_unique<NodeMonitoringData>(*providedNodeMonitoringData);
    expectedNodeMonitoringData->mMonitoringData.mCPU      = cExpectedDMIPSUsage;
    expectedNodeMonitoringData->mMonitoringData.mRAM      = cExpectedRAMUsage;
    expectedNodeMonitoringData->mMonitoringData.mDownload = cExpectedDownload;
    expectedNodeMonitoringData->mMonitoringData.mUpload   = cExpectedUpload;

    for (auto& instanceMonitoring : expectedNodeMonitoringData->mServiceInstances) {
        instanceMonitoring.mMonitoringData.mCPU = toDMIPS(instanceMonitoring.mMonitoringData.mCPU);
    }

    for (auto& partition : expectedNodeMonitoringData->mMonitoringData.mPartitions) {
        if (auto it = instancePartitions.FindIf([&](const PartitionInfo& p) { return p.mName == partition.mName; });
            it != instancePartitions.end()) {
            partition.mUsedSize = it->mUsedSize;
        }
    }

    auto receivedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

    resourceUsageProvider->ProvideMonitoringData(providedNodeMonitoringData->mMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));
    EXPECT_TRUE(sender->WaitMonitoringData(*receivedNodeMonitoringData).IsNone());

    LOG_DBG() << "Received monitoring data: cpu=" << receivedNodeMonitoringData->mMonitoringData.mCPU
              << ", ram=" << receivedNodeMonitoringData->mMonitoringData.mRAM
              << ", download=" << receivedNodeMonitoringData->mMonitoringData.mDownload
              << ", upload=" << receivedNodeMonitoringData->mMonitoringData.mUpload;

    EXPECT_EQ(receivedNodeMonitoringData->mMonitoringData.mCPU, cExpectedDMIPSUsage);
    EXPECT_EQ(receivedNodeMonitoringData->mMonitoringData.mRAM, cExpectedRAMUsage);
    EXPECT_EQ(receivedNodeMonitoringData->mMonitoringData.mDownload, cExpectedDownload);
    EXPECT_EQ(receivedNodeMonitoringData->mMonitoringData.mUpload, cExpectedUpload);

    receivedNodeMonitoringData->mTimestamp = providedNodeMonitoringData->mTimestamp;
    EXPECT_EQ(*expectedNodeMonitoringData, *receivedNodeMonitoringData);

    EXPECT_TRUE(monitor->Stop().IsNone());
}

} // namespace aos::monitoring
