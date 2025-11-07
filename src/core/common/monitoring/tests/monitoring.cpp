/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mutex>
#include <queue>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/monitoring/resourcemonitor.hpp>
#include <core/common/tests/stubs/cloudconnectionstub.hpp>
#include <core/common/tests/utils/log.hpp>

#include "stubs/alertsenderstub.hpp"
#include "stubs/nodeinfoproviderstub.hpp"
#include "stubs/resourcemanagerstub.hpp"
#include "stubs/senderstub.hpp"

namespace aos::monitoring {

using namespace testing;

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

static constexpr auto cWaitTimeout = std::chrono::seconds {5};

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

struct AlertData {
    std::string     mTag;
    std::string     mParameter;
    uint64_t        mValue;
    QuotaAlertState mState;
};

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

void SetInstancesMonitoringData(
    NodeMonitoringData& nodeMonitoringData, const Array<Pair<String, InstanceMonitoringData>>& instancesData)
{
    nodeMonitoringData.mServiceInstances.Clear();

    for (const auto& [instanceID, instanceData] : instancesData) {
        nodeMonitoringData.mServiceInstances.PushBack(instanceData);
    }
}

std::unique_ptr<NodeInfo> CreateNodeInfo(const Array<PartitionInfo>& partitions)
{
    auto nodeInfo = std::make_unique<NodeInfo>();

    nodeInfo->mNodeID     = "node1";
    nodeInfo->mNodeType   = "type1";
    nodeInfo->mTitle      = "name1";
    nodeInfo->mState      = NodeStateEnum::eOnline;
    nodeInfo->mOSInfo.mOS = "linux";
    nodeInfo->mTotalRAM   = 8192;
    nodeInfo->mMaxDMIPS   = 10000;

    for (const auto& partition : partitions) {
        if (auto err = nodeInfo->mPartitions.EmplaceBack(); !err.IsNone()) {
            throw std::runtime_error("Failed to add partition to node info");
        }

        auto& nodePartition = nodeInfo->mPartitions.Back();

        nodePartition.mName      = partition.mName;
        nodePartition.mPath      = partition.mPath;
        nodePartition.mTotalSize = partition.mTotalSize;
    }

    return nodeInfo;
}

template <typename T>
bool operator==(const AlertData& lhs, const T& rhs)
{
    return lhs.mTag == lhs.mTag && lhs.mParameter == rhs.mParameter.CStr() && lhs.mValue == rhs.mValue
        && lhs.mState == rhs.mState;
}

/***********************************************************************************************************************
 * Mocks
 **********************************************************************************************************************/

class MockResourceUsageProvider : public ResourceUsageProviderItf {
public:
    Error GetNodeMonitoringData(const Array<PartitionInfo>& partitionInfos, MonitoringData& monitoringData) override
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] { return !mNodeMonitoringData.empty(); })) {
            return ErrorEnum::eTimeout;
        }

        auto release = DeferRelease(&mNodeMonitoringData, [](auto* data) { data->pop(); });

        monitoringData.mCPU      = mNodeMonitoringData.front().mCPU;
        monitoringData.mRAM      = mNodeMonitoringData.front().mRAM;
        monitoringData.mDownload = mNodeMonitoringData.front().mDownload;
        monitoringData.mUpload   = mNodeMonitoringData.front().mUpload;

        if (partitionInfos.Size() != mNodeMonitoringData.front().mPartitions.Size()) {
            return ErrorEnum::eInvalidArgument;
        }

        for (const auto& param : mNodeMonitoringData.front().mPartitions) {
            if (auto err = monitoringData.mPartitions.EmplaceBack(); !err.IsNone()) {
                return err;
            }

            monitoringData.mPartitions.Back().mName     = param.mName;
            monitoringData.mPartitions.Back().mUsedSize = param.mUsedSize;
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

        if (instanceMonitoringData.mPartitions.Size() != data.mMonitoringData.mPartitions.Size()) {
            return ErrorEnum::eInvalidArgument;
        }

        instanceMonitoringData.mMonitoringData.mPartitions.Resize(data.mMonitoringData.mPartitions.Size());

        for (size_t i = 0; i < data.mMonitoringData.mPartitions.Size(); i++) {
            const auto& in  = data.mMonitoringData.mPartitions[i];
            auto&       out = instanceMonitoringData.mMonitoringData.mPartitions[i];

            out.mName     = in.mName;
            out.mUsedSize = in.mUsedSize;
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

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class MonitoringTest : public Test {
protected:
    void SetUp() override { tests::utils::InitLog(); }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(MonitoringTest, GetNodeMonitoringData)
{
    PartitionInfo nodePartitionsInfo[] = {{"disk1", {}, "", 512}, {"disk2", {}, "", 1024}};
    auto          nodePartitions       = Array<PartitionInfo>(nodePartitionsInfo, ArraySize(nodePartitionsInfo));

    auto nodeInfo = CreateNodeInfo(nodePartitions);

    PartitionUsage nodePartitionsUsagesData[] = {{"disk1", 256}, {"disk2", 512}};
    auto nodePartitionsUsages = Array<PartitionUsage>(nodePartitionsUsagesData, ArraySize(nodePartitionsUsagesData));

    AlertRules alertRules;
    alertRules.mCPU.SetValue(AlertRulePercents {Time::cMilliseconds, 10, 20});
    alertRules.mRAM.SetValue(AlertRulePercents {Time::cMilliseconds, 20, 30});
    alertRules.mDownload.SetValue(AlertRulePoints {Time::cMilliseconds, 10, 20});
    alertRules.mUpload.SetValue(AlertRulePoints {Time::cMilliseconds, 10, 20});

    for (const auto& partition : nodePartitionsInfo) {
        ASSERT_TRUE(alertRules.mPartitions.PushBack(PartitionAlertRule {Time::cMilliseconds, 10, 20, partition.mName})
                        .IsNone());
    }

    auto nodeInfoProvider      = std::make_unique<NodeInfoProviderStub>(*nodeInfo);
    auto resourceManager       = std::make_unique<ResourceManagerStub>(Optional<AlertRules> {alertRules});
    auto resourceUsageProvider = std::make_unique<MockResourceUsageProvider>();
    auto sender                = std::make_unique<SenderStub>();
    auto alertSender           = std::make_unique<AlertSenderStub>();
    auto cloudConnection       = std::make_unique<cloudconnection::CloudConnectionStub>();

    auto   monitor = std::make_unique<ResourceMonitor>();
    Config config {100 * Time::cMilliseconds, 100 * Time::cMilliseconds};

    EXPECT_TRUE(monitor
                    ->Init(config, *nodeInfoProvider, *resourceManager, *resourceUsageProvider, *sender, *alertSender,
                        *cloudConnection)
                    .IsNone());
    EXPECT_TRUE(monitor->Start().IsNone());

    cloudConnection->NotifyConnect();

    PartitionInfo instancePartitionInfosData[] = {{"state", {}, "", 0}, {"storage", {}, "", 0}};
    auto          instancePartitionInfos
        = Array<PartitionInfo>(instancePartitionInfosData, ArraySize(instancePartitionInfosData));

    PartitionUsage instancePartitionsData[] = {{"state", 256}, {"storage", 512}};
    auto instancePartitions = Array<PartitionUsage>(instancePartitionsData, ArraySize(instancePartitionsData));

    InstanceIdent instance0Ident {"service0", "subject0", 0};
    InstanceIdent instance1Ident {"service1", "subject1", 1};

    Pair<String, InstanceMonitoringData> instancesMonitoringData[] = {
        {"instance0", {instance0Ident, {{}, 10000, 2048, instancePartitions, 10, 20}, instancePartitionInfos}},
        {"instance1", {instance1Ident, {{}, 15000, 1024, instancePartitions, 20, 40}, instancePartitionInfos}},
    };

    auto providedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

    *providedNodeMonitoringData = {};

    providedNodeMonitoringData->mNodeID         = "node1";
    providedNodeMonitoringData->mMonitoringData = {{}, 30000, 8192, nodePartitionsUsages, 120, 240};

    SetInstancesMonitoringData(*providedNodeMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));

    resourceUsageProvider->ProvideInitialInstancesData({"instance0", "instance1"});

    EXPECT_TRUE(
        monitor->StartInstanceMonitoring("instance0", {instance0Ident, instancePartitionInfos, 0, 0, {}}).IsNone());
    EXPECT_TRUE(
        monitor->StartInstanceMonitoring("instance1", {instance1Ident, instancePartitionInfos, 0, 0, {}}).IsNone());

    auto receivedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

    resourceUsageProvider->ProvideMonitoringData(providedNodeMonitoringData->mMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));
    EXPECT_TRUE(sender->WaitMonitoringData(*receivedNodeMonitoringData).IsNone());

    providedNodeMonitoringData->mMonitoringData.mCPU
        = providedNodeMonitoringData->mMonitoringData.mCPU * nodeInfo->mMaxDMIPS / 100.0;

    for (auto& instanceMonitoring : providedNodeMonitoringData->mServiceInstances) {
        instanceMonitoring.mMonitoringData.mCPU = instanceMonitoring.mMonitoringData.mCPU * nodeInfo->mMaxDMIPS / 100.0;
    }

    receivedNodeMonitoringData->mTimestamp = providedNodeMonitoringData->mTimestamp;
    EXPECT_EQ(*providedNodeMonitoringData, *receivedNodeMonitoringData);

    EXPECT_TRUE(monitor->Stop().IsNone());
}

TEST_F(MonitoringTest, GetAverageMonitoringData)
{
    PartitionInfo nodePartitionsInfo[] = {{"disk", {}, "", 512}};
    auto          nodePartitions       = Array<PartitionInfo>(nodePartitionsInfo, ArraySize(nodePartitionsInfo));
    auto          nodeInfo             = CreateNodeInfo(nodePartitions);

    auto nodeInfoProvider      = std::make_unique<NodeInfoProviderStub>(*nodeInfo);
    auto resourceManager       = std::make_unique<ResourceManagerStub>();
    auto resourceUsageProvider = std::make_unique<MockResourceUsageProvider>();
    auto sender                = std::make_unique<SenderStub>();
    auto alertSender           = std::make_unique<AlertSenderStub>();
    auto cloudConnection       = std::make_unique<cloudconnection::CloudConnectionStub>();

    auto   monitor = std::make_unique<ResourceMonitor>();
    Config config {};

    EXPECT_TRUE(monitor
                    ->Init(config, *nodeInfoProvider, *resourceManager, *resourceUsageProvider, *sender, *alertSender,
                        *cloudConnection)
                    .IsNone());
    EXPECT_TRUE(monitor->Start().IsNone());

    cloudConnection->NotifyConnect();

    InstanceIdent instance0Ident {"service0", "subject0", 0};
    PartitionInfo instancePartitionInfosData[] = {{"disk", {}, "", 0}};
    auto          instancePartitionInfos
        = Array<PartitionInfo>(instancePartitionInfosData, ArraySize(instancePartitionInfosData));

    resourceUsageProvider->ProvideInitialInstancesData({"instance0"});

    EXPECT_TRUE(
        monitor->StartInstanceMonitoring("instance0", {instance0Ident, instancePartitionInfos, 0, 0, {}}).IsNone());

    PartitionUsage providedNodeDiskData[][1] = {
        {{"disk", 100}},
        {{"disk", 400}},
        {{"disk", 500}},
    };

    PartitionUsage averageNodeDiskData[][1] = {
        {{"disk", 100}},
        {{"disk", 200}},
        {{"disk", 300}},
    };

    PartitionUsage providedInstanceDiskData[][1] = {
        {{"disk", 300}},
        {{"disk", 0}},
        {{"disk", 800}},
    };

    PartitionUsage averageInstanceDiskData[][1] = {
        {{"disk", 300}},
        {{"disk", 200}},
        {{"disk", 400}},
    };

    std::vector<NodeMonitoringData> providedNodeMonitoringData;

    providedNodeMonitoringData.emplace_back(NodeMonitoringData {"node1", {}, {{}, 0, 600, {}, 300, 300}, {}});
    providedNodeMonitoringData.emplace_back(NodeMonitoringData {"node1", {}, {{}, 900, 300, {}, 0, 300}, {}});
    providedNodeMonitoringData.emplace_back(NodeMonitoringData {"node1", {}, {{}, 1200, 200, {}, 200, 0}, {}});

    std::vector<NodeMonitoringData> averageNodeMonitoringData;

    averageNodeMonitoringData.emplace_back(NodeMonitoringData {"node1", {}, {{}, 0, 600, {}, 300, 300}, {}});
    averageNodeMonitoringData.emplace_back(NodeMonitoringData {"node1", {}, {{}, 300, 500, {}, 200, 300}, {}});
    averageNodeMonitoringData.emplace_back(NodeMonitoringData {"node1", {}, {{}, 600, 400, {}, 200, 200}, {}});

    std::vector<Pair<String, InstanceMonitoringData>> providedInstanceMonitoringData;

    providedInstanceMonitoringData.emplace_back(
        "instance0", InstanceMonitoringData {instance0Ident, {{}, 600, 0, {}, 300, 300}, instancePartitionInfos});
    providedInstanceMonitoringData.emplace_back(
        "instance0", InstanceMonitoringData {instance0Ident, {{}, 300, 900, {}, 300, 0}, instancePartitionInfos});
    providedInstanceMonitoringData.emplace_back(
        "instance0", InstanceMonitoringData {instance0Ident, {{}, 200, 1200, {}, 0, 200}, instancePartitionInfos});

    std::vector<Pair<String, InstanceMonitoringData>> averageInstanceMonitoringData;

    averageInstanceMonitoringData.emplace_back(
        "instance0", InstanceMonitoringData {instance0Ident, {{}, 600, 0, {}, 300, 300}, instancePartitionInfos});
    averageInstanceMonitoringData.emplace_back(
        "instance0", InstanceMonitoringData {instance0Ident, {{}, 500, 300, {}, 300, 200}, instancePartitionInfos});
    averageInstanceMonitoringData.emplace_back(
        "instance0", InstanceMonitoringData {instance0Ident, {{}, 400, 600, {}, 200, 200}, instancePartitionInfos});

    for (size_t i = 0; i < 3 /*providedNodeMonitoringData.size()*/; i++) {
        std::cout << "Iteration " << i << std::endl;

        auto receivedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

        *receivedNodeMonitoringData = {};

        providedInstanceMonitoringData[i].mSecond.mMonitoringData.mPartitions
            = Array<PartitionUsage>(providedInstanceDiskData[i], ArraySize(providedInstanceDiskData[i]));
        providedNodeMonitoringData[i].mMonitoringData.mPartitions
            = Array<PartitionUsage>(providedNodeDiskData[i], ArraySize(providedNodeDiskData[i]));

        SetInstancesMonitoringData(providedNodeMonitoringData[i],
            Array<Pair<String, InstanceMonitoringData>>(&providedInstanceMonitoringData[i], 1));

        resourceUsageProvider->ProvideMonitoringData(providedNodeMonitoringData[i].mMonitoringData,
            Array<Pair<String, InstanceMonitoringData>>(&providedInstanceMonitoringData[i], 1));

        EXPECT_TRUE(sender->WaitMonitoringData(*receivedNodeMonitoringData).IsNone());
        EXPECT_TRUE(monitor->GetAverageMonitoringData(*receivedNodeMonitoringData).IsNone());

        averageInstanceMonitoringData[i].mSecond.mMonitoringData.mPartitions
            = Array<PartitionUsage>(averageInstanceDiskData[i], ArraySize(averageInstanceDiskData[i]));
        averageNodeMonitoringData[i].mMonitoringData.mPartitions
            = Array<PartitionUsage>(averageNodeDiskData[i], ArraySize(averageNodeDiskData[i]));

        SetInstancesMonitoringData(averageNodeMonitoringData[i],
            Array<Pair<String, InstanceMonitoringData>>(&averageInstanceMonitoringData[i], 1));

        averageNodeMonitoringData[i].mMonitoringData.mCPU
            = averageNodeMonitoringData[i].mMonitoringData.mCPU * nodeInfo->mMaxDMIPS / 100.0;

        for (auto& instanceMonitoring : averageNodeMonitoringData[i].mServiceInstances) {
            instanceMonitoring.mMonitoringData.mCPU
                = instanceMonitoring.mMonitoringData.mCPU * nodeInfo->mMaxDMIPS / 100.0;
        }

        receivedNodeMonitoringData->mTimestamp = averageNodeMonitoringData[i].mTimestamp;

        EXPECT_EQ(averageNodeMonitoringData[i], *receivedNodeMonitoringData);
    }

    EXPECT_TRUE(monitor->Stop().IsNone());
}

TEST_F(MonitoringTest, QuotaAlertsAreSent)
{
    PartitionInfo nodePartitionsInfo[] = {
        {"disk1", {}, "", 512},
        {"disk2", {}, "", 1024},
        {"state", {}, "", 512},
        {"storage", {}, "", 1024},
    };
    auto nodePartitions = Array<PartitionInfo>(nodePartitionsInfo, ArraySize(nodePartitionsInfo));
    auto nodeInfo       = CreateNodeInfo(nodePartitions);

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
    auto   nodeInfoProvider      = std::make_unique<NodeInfoProviderStub>(*nodeInfo);
    auto   resourceManager       = std::make_unique<ResourceManagerStub>(Optional<AlertRules> {alertRules});
    auto   resourceUsageProvider = std::make_unique<MockResourceUsageProvider>();
    auto   sender                = std::make_unique<SenderStub>();
    auto   alertSender           = std::make_unique<AlertSenderStub>();
    auto   cloudConnection       = std::make_unique<cloudconnection::CloudConnectionStub>();

    auto monitor = std::make_unique<ResourceMonitor>();

    EXPECT_TRUE(monitor
                    ->Init(config, *nodeInfoProvider, *resourceManager, *resourceUsageProvider, *sender, *alertSender,
                        *cloudConnection)
                    .IsNone());
    EXPECT_TRUE(monitor->Start().IsNone());

    cloudConnection->NotifyConnect();

    auto           instancePartitionsInfos  = Array<PartitionInfo>(&nodePartitionsInfo[2], 2);
    PartitionUsage instancePartitionsData[] = {{"state", 128}, {"storage", 256}};
    auto instancePartitions = Array<PartitionUsage>(instancePartitionsData, ArraySize(instancePartitionsData));

    PartitionUsage nodePartitionsUsagesData[]
        = {{"disk1", 256}, {"disk2", 512}, instancePartitionsData[0], instancePartitionsData[1]};
    auto nodePartitionsUsages = Array<PartitionUsage>(nodePartitionsUsagesData, ArraySize(nodePartitionsUsagesData));

    auto instanceAlertRules = alertRules;
    instanceAlertRules.mPartitions.Clear();
    instanceAlertRules.mPartitions.PushBack({Time::cMilliseconds, 10.0, 20.0, "state"});
    instanceAlertRules.mPartitions.PushBack({Time::cMilliseconds, 10.0, 20.0, "storage"});

    InstanceIdent instance0Ident {"service0", "subject0", 0};
    InstanceIdent instance1Ident {"service1", "subject1", 1};

    Pair<String, InstanceMonitoringData> instancesMonitoringData[] = {
        {"instance0", {instance0Ident, {{}, 100, 2048, instancePartitions, 10, 20}, instancePartitionsInfos}},
        {"instance1", {instance1Ident, {{}, 150, 1024, instancePartitions, 20, 40}, instancePartitionsInfos}},
    };

    const std::vector<Pair<InstanceIdent, AlertData>> expectedInstanceAlerts = {
        {instance0Ident, {"instanceQuotaAlert", "cpu", 100 * nodeInfo->mMaxDMIPS / 100, QuotaAlertStateEnum::eRaise}},
        {instance0Ident, {"instanceQuotaAlert", "state", 128, QuotaAlertStateEnum::eRaise}},
        {instance0Ident, {"instanceQuotaAlert", "storage", 256, QuotaAlertStateEnum::eRaise}},
        {instance0Ident, {"instanceQuotaAlert", "download", 20, QuotaAlertStateEnum::eRaise}},
        {instance1Ident, {"instanceQuotaAlert", "cpu", 150 * nodeInfo->mMaxDMIPS / 100, QuotaAlertStateEnum::eRaise}},
        {instance1Ident, {"instanceQuotaAlert", "state", 128, QuotaAlertStateEnum::eRaise}},
        {instance1Ident, {"instanceQuotaAlert", "storage", 256, QuotaAlertStateEnum::eRaise}},
        {instance1Ident, {"instanceQuotaAlert", "download", 20, QuotaAlertStateEnum::eRaise}},
    };

    auto providedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

    *providedNodeMonitoringData = {};

    providedNodeMonitoringData->mNodeID         = "node1";
    providedNodeMonitoringData->mTimestamp      = Time::Now().Add(Time::cSeconds);
    providedNodeMonitoringData->mMonitoringData = {{}, 100, 8192, nodePartitionsUsages, 120, 240};

    const std::vector<AlertData> expectedSystemAlerts = {
        {"systemQuotaAlert", "cpu", nodeInfo->mMaxDMIPS, QuotaAlertStateEnum::eRaise},
        {"systemQuotaAlert", "ram", 8192, QuotaAlertStateEnum::eRaise},
        {"systemQuotaAlert", "disk1", 256, QuotaAlertStateEnum::eRaise},
        {"systemQuotaAlert", "disk2", 512, QuotaAlertStateEnum::eRaise},
        {"systemQuotaAlert", "download", 120, QuotaAlertStateEnum::eRaise},
        {"systemQuotaAlert", "upload", 240, QuotaAlertStateEnum::eRaise},
    };

    SetInstancesMonitoringData(*providedNodeMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));

    resourceUsageProvider->ProvideInitialInstancesData({"instance0", "instance1"});

    EXPECT_TRUE(monitor
                    ->StartInstanceMonitoring("instance0",
                        {instance0Ident, instancePartitionsInfos, 0, 0, Optional<AlertRules> {instanceAlertRules}})
                    .IsNone());
    EXPECT_TRUE(monitor
                    ->StartInstanceMonitoring("instance1",
                        {instance1Ident, instancePartitionsInfos, 0, 0, Optional<AlertRules> {instanceAlertRules}})
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
                          return expected.mFirst == static_cast<const InstanceIdent&>(received)
                              && expected.mSecond == received;
                      }),
            expectedInstanceAlerts.end());
    }
}

TEST_F(MonitoringTest, GetNodeMonitoringDataOnInstanceSpikes)
{
    PartitionInfo nodePartitionsInfo[] = {{"disk1", {}, "", 512}, {"disk2", {}, "", 1024}};
    auto          nodePartitions       = Array<PartitionInfo>(nodePartitionsInfo, ArraySize(nodePartitionsInfo));
    auto          nodeInfo             = CreateNodeInfo(nodePartitions);

    const auto     cNodeDMIPS          = nodeInfo->mMaxDMIPS;
    constexpr auto cNodeCPUUsage       = 30.0;
    constexpr auto cInstance0CPUUsage  = 50.0;
    constexpr auto cInstance1CPUUsage  = 25.0;
    const auto     cExpectedDMIPSUsage = cNodeDMIPS * (cInstance0CPUUsage + cInstance1CPUUsage) / 100.0;

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

    auto nodeInfoProvider      = std::make_unique<NodeInfoProviderStub>(*nodeInfo);
    auto resourceManager       = std::make_unique<ResourceManagerStub>();
    auto resourceUsageProvider = std::make_unique<MockResourceUsageProvider>();
    auto sender                = std::make_unique<SenderStub>();
    auto alertSender           = std::make_unique<AlertSenderStub>();
    auto cloudConnection       = std::make_unique<cloudconnection::CloudConnectionStub>();

    auto   monitor = std::make_unique<ResourceMonitor>();
    Config config {100 * Time::cMilliseconds, 100 * Time::cMilliseconds};

    EXPECT_TRUE(monitor
                    ->Init(config, *nodeInfoProvider, *resourceManager, *resourceUsageProvider, *sender, *alertSender,
                        *cloudConnection)
                    .IsNone());
    EXPECT_TRUE(monitor->Start().IsNone());

    cloudConnection->NotifyConnect();

    PartitionInfo instancePartitionInfosData[] = {{"states", {}, "", 0}, {"storages", {}, "", 0}};
    auto          instancePartitionInfos
        = Array<PartitionInfo>(instancePartitionInfosData, ArraySize(instancePartitionInfosData));

    PartitionUsage instancePartitionsUsageData[] = {{"states", 256}, {"storages", 512}};
    auto           instancePartitionsUsage
        = Array<PartitionUsage>(instancePartitionsUsageData, ArraySize(instancePartitionsUsageData));

    PartitionUsage nodePartitionsUsagesData[] = {{"disk1", 256}, {"disk2", 512}};
    auto nodePartitionsUsages = Array<PartitionUsage>(nodePartitionsUsagesData, ArraySize(nodePartitionsUsagesData));

    InstanceIdent instance0Ident {"service0", "subject0", 0};
    InstanceIdent instance1Ident {"service1", "subject1", 1};

    Pair<String, InstanceMonitoringData> instancesMonitoringData[] = {
        {"instance0",
            {instance0Ident,
                {{}, cInstance0CPUUsage, cInstance0RAMUsage, instancePartitionsUsage, cInstance0Download,
                    cInstance0Upload},
                instancePartitionInfos}},
        {"instance1",
            {instance1Ident,
                {{}, cInstance1CPUUsage, cInstance1RAMUsage, instancePartitionsUsage, cInstance1Download,
                    cInstance1Upload},
                instancePartitionInfos}},

    };

    auto providedNodeMonitoringData = std::make_unique<NodeMonitoringData>();

    providedNodeMonitoringData->mNodeID = "node1";
    providedNodeMonitoringData->mMonitoringData
        = {{}, cNodeCPUUsage, cNodeRAMUsage, nodePartitionsUsages, cNodeDownload, cNodeUpload};

    SetInstancesMonitoringData(*providedNodeMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));

    resourceUsageProvider->ProvideInitialInstancesData({"instance0", "instance1"});

    EXPECT_TRUE(
        monitor->StartInstanceMonitoring("instance0", {instance0Ident, instancePartitionInfos, 0, 0, {}}).IsNone());
    EXPECT_TRUE(
        monitor->StartInstanceMonitoring("instance1", {instance1Ident, instancePartitionInfos, 0, 0, {}}).IsNone());

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
        if (auto it = instancePartitionsUsage.FindIf([&](const auto& p) { return p.mName == partition.mName; });
            it != instancePartitionsUsage.end()) {
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
