#include <gtest/gtest.h>

#include "aos/common/monitoring.hpp"

#include "log.hpp"

namespace aos {
namespace monitoring {

using namespace testing;

/***********************************************************************************************************************
 * Mocks
 **********************************************************************************************************************/

class MockNodeInfoProvider : public iam::nodeinfoprovider::NodeInfoProviderItf {
    Error GetNodeInfo(NodeInfo& nodeInfo) const override
    {
        nodeInfo.mNodeID = "node1";
        nodeInfo.mCPUs.Resize(1);
        nodeInfo.mTotalRAM = 4000;

        PartitionInfo partitionInfo {};
        partitionInfo.mName = "partitionName";
        partitionInfo.mPath = "partitionPath";
        partitionInfo.mTypes.PushBack("partitionType");
        partitionInfo.mTotalSize = 1000;

        nodeInfo.mPartitions.PushBack(partitionInfo);

        return ErrorEnum::eNone;
    }

    Error SetNodeStatus(const NodeStatus& status) override
    {
        (void)status;

        return ErrorEnum::eNone;
    }
};

class MockResourceUsageProvider : public ResourceUsageProviderItf {
public:
    Error Init() override { return ErrorEnum::eNone; }

    Error GetNodeMonitoringData(const String& nodeID, MonitoringData& monitoringData) override
    {
        EXPECT_TRUE(nodeID == "node1");

        monitoringData.mCPU = 1;
        monitoringData.mRAM = 1000;

        EXPECT_TRUE(monitoringData.mDisk.Size() == 1);
        EXPECT_TRUE(monitoringData.mDisk[0].mName == "partitionName");
        EXPECT_TRUE(monitoringData.mDisk[0].mPath == "partitionPath");

        monitoringData.mDisk[0].mUsedSize = 100;

        mNodeMonitoringCounter++;

        return ErrorEnum::eNone;
    }

    Error GetInstanceMonitoringData(const String& instanceID, MonitoringData& monitoringData) override
    {
        EXPECT_TRUE(instanceID == "instance1");

        monitoringData.mCPU = 1;
        monitoringData.mRAM = 1000;

        EXPECT_TRUE(monitoringData.mDisk.Size() == 1);
        EXPECT_TRUE(monitoringData.mDisk[0].mName == "partitionInstanceName");
        EXPECT_TRUE(monitoringData.mDisk[0].mPath == "partitionInstancePath");

        monitoringData.mDisk[0].mUsedSize = 100;

        return ErrorEnum::eNone;
    }

    int GetNodeMonitoringCounter() const { return mNodeMonitoringCounter; }

private:
    int mNodeMonitoringCounter {};
};

class MockSender : public SenderItf {
public:
    Error SendMonitoringData(const NodeMonitoringData& monitoringData) override
    {
        mSendMonitoringCounter++;

        EXPECT_TRUE(monitoringData.mNodeID == "node1");

        EXPECT_TRUE(monitoringData.mMonitoringData.mCPU == 1);
        EXPECT_TRUE(monitoringData.mMonitoringData.mRAM == 1000);

        EXPECT_TRUE(monitoringData.mMonitoringData.mDisk.Size() == 1);

        EXPECT_TRUE(monitoringData.mMonitoringData.mDisk[0].mName == "partitionName");
        EXPECT_TRUE(monitoringData.mMonitoringData.mDisk[0].mPath == "partitionPath");

        EXPECT_TRUE(monitoringData.mMonitoringData.mDisk[0].mUsedSize == 100);

        if (!mExpectedInstanceMonitoring) {
            EXPECT_TRUE(monitoringData.mServiceInstances.Size() == 0);

            return ErrorEnum::eNone;
        }

        EXPECT_TRUE(monitoringData.mServiceInstances.Size() == 1);

        aos::InstanceIdent instanceIdent {};
        instanceIdent.mInstance  = 1;
        instanceIdent.mServiceID = "serviceID";
        instanceIdent.mSubjectID = "subjectID";

        EXPECT_TRUE(monitoringData.mServiceInstances[0].mInstanceIdent == instanceIdent);

        EXPECT_TRUE(monitoringData.mServiceInstances[0].mInstanceID == "instance1");

        EXPECT_TRUE(monitoringData.mServiceInstances[0].mMonitoringData.mCPU == 1);
        EXPECT_TRUE(monitoringData.mServiceInstances[0].mMonitoringData.mRAM == 1000);

        EXPECT_TRUE(monitoringData.mServiceInstances[0].mMonitoringData.mDisk.Size() == 1);

        EXPECT_TRUE(monitoringData.mServiceInstances[0].mMonitoringData.mDisk[0].mName == "partitionInstanceName");

        EXPECT_TRUE(monitoringData.mServiceInstances[0].mMonitoringData.mDisk[0].mPath == "partitionInstancePath");

        EXPECT_TRUE(monitoringData.mServiceInstances[0].mMonitoringData.mDisk[0].mUsedSize == 100);

        return ErrorEnum::eNone;
    }

    void SetExpectedInstanceMonitoring(bool expectedInstanceMonitoring)
    {
        mExpectedInstanceMonitoring = expectedInstanceMonitoring;
    }

    int GetSendMonitoringCounter() const { return mSendMonitoringCounter; }

private:
    bool mExpectedInstanceMonitoring {};
    int  mSendMonitoringCounter {};
};

class MockConnectionPublisher : public ConnectionPublisherItf {
public:
    aos::Error Subscribes(ConnectionSubscriberItf& subscriber) override
    {
        mSubscriber = &subscriber;

        return ErrorEnum::eNone;
    }

    void Unsubscribes(ConnectionSubscriberItf& subscriber) override
    {
        EXPECT_TRUE(&subscriber == mSubscriber);

        mSubscriber = nullptr;

        return;
    }

    void Notify() const
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

TEST_F(MonitoringTest, Init)
{
    MockNodeInfoProvider      nodeInfoProvider {};
    MockConnectionPublisher   connectionPublisher {};
    MockResourceUsageProvider resourceUsageProvider {};
    MockSender                sender {};
    ResourceMonitor           monitor {};

    EXPECT_TRUE(monitor.Init(nodeInfoProvider, resourceUsageProvider, sender, connectionPublisher).IsNone());
}

TEST_F(MonitoringTest, GetNodeMonitoringData)
{
    MockNodeInfoProvider      nodeInfoProvider {};
    MockResourceUsageProvider resourceUsageProvider {};
    MockSender                sender {};
    MockConnectionPublisher   connectionPublisher {};
    ResourceMonitor           monitor {};

    EXPECT_TRUE(monitor.Init(nodeInfoProvider, resourceUsageProvider, sender, connectionPublisher).IsNone());

    connectionPublisher.Notify();

    sleep(AOS_CONFIG_MONITORING_POLL_PERIOD_SEC + 1);

    EXPECT_TRUE(resourceUsageProvider.GetNodeMonitoringCounter() > 0);

    EXPECT_TRUE(sender.GetSendMonitoringCounter() > 0);
}

TEST_F(MonitoringTest, GetInstanceMonitoringData)
{
    MockNodeInfoProvider      nodeInfoProvider {};
    MockResourceUsageProvider resourceUsageProvider {};
    MockSender                sender {};
    MockConnectionPublisher   connectionPublisher {};
    ResourceMonitor           monitor {};

    EXPECT_TRUE(monitor.Init(nodeInfoProvider, resourceUsageProvider, sender, connectionPublisher).IsNone());

    connectionPublisher.Notify();

    InstanceMonitorParams instanceMonitorParams {};

    instanceMonitorParams.mInstanceIdent.mInstance  = 1;
    instanceMonitorParams.mInstanceIdent.mServiceID = "serviceID";
    instanceMonitorParams.mInstanceIdent.mSubjectID = "subjectID";

    PartitionInfo partitionInstanceParam {};

    partitionInstanceParam.mName = "partitionInstanceName";
    partitionInstanceParam.mPath = "partitionInstancePath";

    instanceMonitorParams.mPartitions.PushBack(partitionInstanceParam);

    sender.SetExpectedInstanceMonitoring(true);

    EXPECT_TRUE(monitor.StartInstanceMonitoring("instance1", instanceMonitorParams).IsNone());

    sleep(AOS_CONFIG_MONITORING_POLL_PERIOD_SEC + 1);

    EXPECT_TRUE(resourceUsageProvider.GetNodeMonitoringCounter() > 0);
}

} // namespace monitoring
} // namespace aos
