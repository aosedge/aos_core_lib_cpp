#include <mutex>

#include <gtest/gtest.h>

#include "aos/common/monitoring/resourcemonitor.hpp"
#include "log.hpp"

namespace aos::monitoring {

using namespace testing;

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

static constexpr auto cWaitTimeout = std::chrono::seconds {5};

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

class MockResourceUsageProvider : public ResourceUsageProviderItf {
public:
    Error GetNodeMonitoringData(const String& nodeID, MonitoringData& monitoringData) override
    {
        (void)nodeID;

        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] { return mDataProvided; })) {
            return ErrorEnum::eTimeout;
        }

        mDataProvided = false;

        monitoringData.mCPU      = mNodeMonitoringData.mCPU;
        monitoringData.mRAM      = mNodeMonitoringData.mRAM;
        monitoringData.mDownload = mNodeMonitoringData.mDownload;
        monitoringData.mUpload   = mNodeMonitoringData.mUpload;

        if (monitoringData.mDisk.Size() != mNodeMonitoringData.mDisk.Size()) {
            return ErrorEnum::eInvalidArgument;
        }

        for (size_t i = 0; i < monitoringData.mDisk.Size(); i++) {
            monitoringData.mDisk[i].mUsedSize = mNodeMonitoringData.mDisk[i].mUsedSize;
        }

        return ErrorEnum::eNone;
    }

    Error GetInstanceMonitoringData(const String& instanceID, MonitoringData& monitoringData) override
    {
        auto instanceMonitoringData = mInstancesMonitoringData.At(instanceID);
        if (!instanceMonitoringData.mError.IsNone()) {
            return AOS_ERROR_WRAP(instanceMonitoringData.mError);
        }

        monitoringData.mCPU      = instanceMonitoringData.mValue.mMonitoringData.mCPU;
        monitoringData.mRAM      = instanceMonitoringData.mValue.mMonitoringData.mRAM;
        monitoringData.mDownload = instanceMonitoringData.mValue.mMonitoringData.mDownload;
        monitoringData.mUpload   = instanceMonitoringData.mValue.mMonitoringData.mUpload;

        if (monitoringData.mDisk.Size() != instanceMonitoringData.mValue.mMonitoringData.mDisk.Size()) {
            return ErrorEnum::eInvalidArgument;
        }

        for (size_t i = 0; i < monitoringData.mDisk.Size(); i++) {
            monitoringData.mDisk[i].mUsedSize = instanceMonitoringData.mValue.mMonitoringData.mDisk[i].mUsedSize;
        }

        return ErrorEnum::eNone;
    }

    void ProvideMonitoringData(const MonitoringData&       nodeMonitoringData,
        const Array<Pair<String, InstanceMonitoringData>>& instancesMonitoringData)
    {
        std::lock_guard lock {mMutex};

        mNodeMonitoringData = nodeMonitoringData;
        mInstancesMonitoringData.Assign(instancesMonitoringData);
        mDataProvided = true;

        mCondVar.notify_one();
    }

private:
    std::mutex                                                  mMutex;
    std::condition_variable                                     mCondVar;
    bool                                                        mDataProvided = false;
    MonitoringData                                              mNodeMonitoringData {};
    StaticMap<String, InstanceMonitoringData, cMaxNumInstances> mInstancesMonitoringData {};
};

class MockSender : public SenderItf {
public:
    Error SendMonitoringData(const NodeMonitoringData& monitoringData) override
    {
        std::lock_guard lock {mMutex};

        mMonitoringData = monitoringData;
        mDataSent       = true;

        mCondVar.notify_one();

        return ErrorEnum::eNone;
    }

    Error WaitMonitoringData(NodeMonitoringData& monitoringData)
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] { return mDataSent; })) {
            return ErrorEnum::eTimeout;
        }

        mDataSent      = false;
        monitoringData = mMonitoringData;

        return ErrorEnum::eNone;
    }

private:
    static constexpr auto cWaitTimeout = std::chrono::seconds {5};

    std::mutex              mMutex;
    std::condition_variable mCondVar;
    bool                    mDataSent = false;
    NodeMonitoringData      mMonitoringData {};
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

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class MonitoringTest : public Test {
protected:
    void SetUp() override { InitLog(); }
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

    MockNodeInfoProvider nodeInfoProvider {nodeInfo};

    MockResourceUsageProvider resourceUsageProvider {};
    MockSender                sender {};
    MockConnectionPublisher   connectionPublisher {};
    ResourceMonitor           monitor {};

    EXPECT_TRUE(monitor.Init(nodeInfoProvider, resourceUsageProvider, sender, connectionPublisher).IsNone());

    connectionPublisher.NotifyConnect();

    PartitionInfo instancePartitionsInfo[] = {{"state", {}, "", 512, 256}, {"storage", {}, "", 1024, 512}};
    auto          instancePartitions = Array<PartitionInfo>(instancePartitionsInfo, ArraySize(instancePartitionsInfo));

    InstanceIdent instance0Ident {"service0", "subject0", 0};
    InstanceIdent instance1Ident {"service1", "subject1", 1};

    Pair<String, InstanceMonitoringData> instancesMonitoringData[] = {
        {"instance0", {instance0Ident, {10000, 2048, instancePartitions, 10, 20}}},
        {"instance1", {instance1Ident, {15000, 1024, instancePartitions, 20, 40}}},
    };

    NodeMonitoringData providedNodeMonitoringData {"node1", {}, {30000, 8192, nodePartitions, 120, 240}, {}};

    SetInstancesMonitoringData(providedNodeMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));

    EXPECT_TRUE(monitor.StartInstanceMonitoring("instance0", {instance0Ident, instancePartitions}).IsNone());
    EXPECT_TRUE(monitor.StartInstanceMonitoring("instance1", {instance1Ident, instancePartitions}).IsNone());

    NodeMonitoringData receivedNodeMonitoringData {};

    resourceUsageProvider.ProvideMonitoringData(providedNodeMonitoringData.mMonitoringData,
        Array<Pair<String, InstanceMonitoringData>>(instancesMonitoringData, ArraySize(instancesMonitoringData)));
    EXPECT_TRUE(sender.WaitMonitoringData(receivedNodeMonitoringData).IsNone());

    providedNodeMonitoringData.mMonitoringData.mCPU
        = providedNodeMonitoringData.mMonitoringData.mCPU * nodeInfo.mMaxDMIPS / 100.0;

    for (auto& instanceMonitoring : providedNodeMonitoringData.mServiceInstances) {
        instanceMonitoring.mMonitoringData.mCPU = instanceMonitoring.mMonitoringData.mCPU * nodeInfo.mMaxDMIPS / 100.0;
    }

    receivedNodeMonitoringData.mTimestamp = providedNodeMonitoringData.mTimestamp;
    EXPECT_EQ(providedNodeMonitoringData, receivedNodeMonitoringData);
}

TEST_F(MonitoringTest, GetAverageMonitoringData)
{
    PartitionInfo nodePartitionsInfo[] = {{"disk", {}, "", 512, 256}};
    auto          nodePartitions       = Array<PartitionInfo>(nodePartitionsInfo, ArraySize(nodePartitionsInfo));
    auto          nodeInfo             = NodeInfo {
        "node1", "type1", "name1", NodeStatusEnum::eProvisioned, "linux", {}, nodePartitions, {}, 10000, 8192};

    MockNodeInfoProvider nodeInfoProvider {nodeInfo};

    MockResourceUsageProvider resourceUsageProvider {};
    MockSender                sender {};
    MockConnectionPublisher   connectionPublisher {};
    ResourceMonitor           monitor {};

    EXPECT_TRUE(monitor.Init(nodeInfoProvider, resourceUsageProvider, sender, connectionPublisher).IsNone());

    connectionPublisher.NotifyConnect();

    InstanceIdent instance0Ident {"service0", "subject0", 0};
    PartitionInfo instancePartitionsInfo[] = {{"disk", {}, "", 512, 256}};
    auto          instancePartitions       = Array<PartitionInfo>(nodePartitionsInfo, ArraySize(nodePartitionsInfo));

    EXPECT_TRUE(monitor.StartInstanceMonitoring("instance0", {instance0Ident, instancePartitions}).IsNone());

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
        {{"disk", {}, "", 512, 300}},
        {{"disk", {}, "", 512, 0}},
        {{"disk", {}, "", 512, 800}},
    };

    PartitionInfo averageInstanceDiskData[][1] = {
        {{"disk", {}, "", 512, 300}},
        {{"disk", {}, "", 512, 200}},
        {{"disk", {}, "", 512, 400}},
    };

    NodeMonitoringData providedNodeMonitoringData[] {
        {"node1", {}, {0, 600, {}, 300, 300}, {}},
        {"node1", {}, {900, 300, {}, 0, 300}, {}},
        {"node1", {}, {1200, 200, {}, 200, 0}, {}},
    };

    NodeMonitoringData averageNodeMonitoringData[] {
        {"node1", {}, {0, 600, {}, 300, 300}, {}},
        {"node1", {}, {300, 500, {}, 200, 300}, {}},
        {"node1", {}, {600, 400, {}, 200, 200}, {}},
    };

    Pair<String, InstanceMonitoringData> providedInstanceMonitoringData[] {
        {"instance0", {instance0Ident, {600, 0, {}, 300, 300}}},
        {"instance0", {instance0Ident, {300, 900, {}, 300, 0}}},
        {"instance0", {instance0Ident, {200, 1200, {}, 0, 200}}},
    };

    Pair<String, InstanceMonitoringData> averageInstanceMonitoringData[] {
        {"instance0", {instance0Ident, {600, 0, {}, 300, 300}}},
        {"instance0", {instance0Ident, {500, 300, {}, 300, 200}}},
        {"instance0", {instance0Ident, {400, 600, {}, 200, 200}}},
    };

    for (uint64_t i = 0; i < ArraySize(providedNodeMonitoringData); i++) {
        NodeMonitoringData receivedNodeMonitoringData {};

        providedInstanceMonitoringData[i].mSecond.mMonitoringData.mDisk
            = Array<PartitionInfo>(providedInstanceDiskData[i], ArraySize(providedInstanceDiskData[i]));
        providedNodeMonitoringData[i].mMonitoringData.mDisk
            = Array<PartitionInfo>(providedNodeDiskData[i], ArraySize(providedNodeDiskData[i]));

        SetInstancesMonitoringData(providedNodeMonitoringData[i],
            Array<Pair<String, InstanceMonitoringData>>(&providedInstanceMonitoringData[i], 1));

        resourceUsageProvider.ProvideMonitoringData(providedNodeMonitoringData[i].mMonitoringData,
            Array<Pair<String, InstanceMonitoringData>>(&providedInstanceMonitoringData[i], 1));

        EXPECT_TRUE(sender.WaitMonitoringData(receivedNodeMonitoringData).IsNone());
        EXPECT_TRUE(monitor.GetAverageMonitoringData(receivedNodeMonitoringData).IsNone());

        averageInstanceMonitoringData[i].mSecond.mMonitoringData.mDisk
            = Array<PartitionInfo>(averageInstanceDiskData[i], ArraySize(averageInstanceDiskData[i]));
        averageNodeMonitoringData[i].mMonitoringData.mDisk
            = Array<PartitionInfo>(averageNodeDiskData[i], ArraySize(averageNodeDiskData[i]));

        SetInstancesMonitoringData(averageNodeMonitoringData[i],
            Array<Pair<String, InstanceMonitoringData>>(&averageInstanceMonitoringData[i], 1));

        averageNodeMonitoringData[i].mMonitoringData.mCPU
            = averageNodeMonitoringData[i].mMonitoringData.mCPU * nodeInfo.mMaxDMIPS / 100.0;

        for (auto& instanceMonitoring : averageNodeMonitoringData[i].mServiceInstances) {
            instanceMonitoring.mMonitoringData.mCPU
                = instanceMonitoring.mMonitoringData.mCPU * nodeInfo.mMaxDMIPS / 100.0;
        }

        receivedNodeMonitoringData.mTimestamp = averageNodeMonitoringData[i].mTimestamp;

        EXPECT_EQ(averageNodeMonitoringData[i], receivedNodeMonitoringData);
    }
} // namespace aos::monitoring

} // namespace aos::monitoring
