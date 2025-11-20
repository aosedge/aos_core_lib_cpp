/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mutex>
#include <queue>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/monitoring/monitoring.hpp>
#include <core/common/tools/logger.hpp>

#include <core/common/tests/stubs/nodeinfoproviderstub.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

namespace aos {

/***********************************************************************************************************************
 * Operators
 **********************************************************************************************************************/

std::ostream& operator<<(std::ostream& os, const MonitoringData& data)
{
    os << "CPU: " << data.mCPU << ", RAM: " << data.mRAM << ", Download: " << data.mDownload
       << ", Upload: " << data.mUpload << ", Partitions: [";
    for (const auto& partition : data.mPartitions) {
        os << partition.mName.CStr() << ": " << partition.mUsedSize << " ";
    }
    os << "]";

    return os;
}

std::ostream& operator<<(std::ostream& os, const SystemQuotaAlert alert)
{
    return os << "{" << alert.mTimestamp.ToUTCString().mValue.CStr() << ":" << alert.mNodeID.CStr() << ":"
              << alert.mParameter.CStr() << ":" << alert.mValue << ":" << alert.mState << "}";
}

} // namespace aos

namespace aos::monitoring {

using namespace testing;

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cPollPeriod          = Time::cSeconds;
const auto     cWaitTimeout         = 1.5 * std::chrono::milliseconds(cPollPeriod.Milliseconds());
constexpr auto cNodeID              = "node1";
constexpr auto cStatesPartition     = "state";
constexpr auto cStatesTotalSize     = 1024;
constexpr auto cStoragePartition    = "storage";
constexpr auto cStorageTotalSize    = 2048;
constexpr auto cMaxDMIPS            = 10000;
constexpr auto cTotalRAM            = 8192;
const auto     cSysStatesRule       = aos::PartitionAlertRule {Time::cMilliseconds, 10, 20, cStatesPartition};
const auto     cSysStorageRule      = aos::PartitionAlertRule {Time::cMilliseconds, 15, 25, cStoragePartition};
const auto     cInstanceStatesRule  = PartitionAlertRule {Time::cMilliseconds, 512, 1000, cStatesPartition};
const auto     cInstanceStorageRule = PartitionAlertRule {Time::cMilliseconds, 512, 2000, cStoragePartition};

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

template <class T = SystemQuotaAlert>
struct TestData : public MonitoringData {
    TestData()
        : MonitoringData()
    {
        mTimestamp = Time::Now();
    }

    TestData& CPU(double cpu)
    {
        mCPU = cpu;
        return *this;
    }

    TestData& RAM(double ram)
    {
        mRAM = static_cast<size_t>(ram);
        return *this;
    }

    TestData& Download(size_t download)
    {
        mDownload = download;
        return *this;
    }

    TestData& Upload(size_t upload)
    {
        mUpload = upload;
        return *this;
    }

    TestData& Partition(const String& name, double usedSize)
    {
        mPartitions.EmplaceBack();
        mPartitions.Back().mName     = name;
        mPartitions.Back().mUsedSize = static_cast<size_t>(usedSize);

        return *this;
    }

    TestData& ExpectAlert(const String& paramName, QuotaAlertStateEnum state)
    {
        mExpectedAlerts.emplace_back();

        auto& alert = mExpectedAlerts.back();

        alert.mParameter = paramName;
        alert.mState     = state;

        if (paramName == "cpu") {
            alert.mValue = static_cast<size_t>(mCPU);
        } else if (paramName == "ram") {
            alert.mValue = mRAM;
        } else if (paramName == "download") {
            alert.mValue = mDownload;
        } else if (paramName == "upload") {
            alert.mValue = mUpload;
        } else {
            for (const auto& partition : mPartitions) {
                if (partition.mName == paramName) {
                    alert.mValue = partition.mUsedSize;
                    break;
                }
            }
        }

        return *this;
    }

    TestData& ExpectAlert(const String& nodeID, const String& paramName, QuotaAlertStateEnum state)
    {
        ExpectAlert(paramName, state);

        mExpectedAlerts.back().mNodeID = nodeID;

        return *this;
    }

    TestData& SetTime(const Time& time = Time::Now())
    {
        mTimestamp = time;

        for (auto& alert : mExpectedAlerts) {
            alert.mTimestamp = time;
        }

        return *this;
    }

    std::vector<T> mExpectedAlerts;
};

using SystemTestData   = TestData<SystemQuotaAlert>;
using InstanceTestData = TestData<InstanceQuotaAlert>;

struct TestMonitoringData {
    TestMonitoringData& SysData(const SystemTestData& data)
    {
        mSystemData = data;

        return *this;
    }

    TestMonitoringData& InstanceData(const InstanceIdent& instanceIdent, const InstanceTestData& data)
    {
        mInstancesData.emplace_back(instanceIdent, data);

        return *this;
    }

    std::vector<InstanceQuotaAlert> GetExpectedAlerts() const
    {
        std::vector<InstanceQuotaAlert> result;

        for (const auto& [ident, data] : mInstancesData) {
            result.insert(result.end(), data.mExpectedAlerts.begin(), data.mExpectedAlerts.end());
        }

        return result;
    }

    std::vector<std::pair<InstanceIdent, TestData<InstanceQuotaAlert>>> mInstancesData;
    SystemTestData                                                      mSystemData;
};

/***********************************************************************************************************************
 * Mocks
 **********************************************************************************************************************/

class NodeConfigProviderStub : public nodeconfig::NodeConfigProviderItf {
public:
    Error GetNodeConfig(NodeConfig& nodeConfig) override
    {
        nodeConfig = mNodeConfig;

        return ErrorEnum::eNone;
    }

    Error SetNodeConfig(const NodeConfig& nodeConfig)
    {
        mNodeConfig = nodeConfig;

        if (mListener) {
            return mListener->OnNodeConfigChanged(mNodeConfig);
        }

        return ErrorEnum::eNone;
    }

    Error SubscribeListener(nodeconfig::NodeConfigListenerItf& listener) override
    {
        mListener = &listener;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeListener(nodeconfig::NodeConfigListenerItf& listener) override
    {
        (void)listener;

        mListener = nullptr;

        return ErrorEnum::eNone;
    }

private:
    nodeconfig::NodeConfigListenerItf* mListener {};
    NodeConfig                         mNodeConfig;
};

class CurrentNodeInfoProviderItfStub : public iamclient::CurrentNodeInfoProviderItf {
public:
    Error GetCurrentNodeInfo(NodeInfo& nodeInfo) const override
    {
        nodeInfo = mNodeInfo;

        return ErrorEnum::eNone;
    }

    Error SetCurrentNodeInfo(const NodeInfo& nodeInfo)
    {
        mNodeInfo = nodeInfo;

        if (mListener) {
            mListener->OnCurrentNodeInfoChanged(mNodeInfo);
        }

        return ErrorEnum::eNone;
    }

    Error SubscribeListener(iamclient::CurrentNodeInfoListenerItf& listener) override
    {
        mListener = &listener;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeListener(iamclient::CurrentNodeInfoListenerItf& listener) override
    {
        (void)listener;

        mListener = nullptr;

        return ErrorEnum::eNone;
    }

private:
    iamclient::CurrentNodeInfoListenerItf* mListener {nullptr};
    NodeInfo                               mNodeInfo;
};

class SenderStub : public SenderItf {
public:
    Error SendMonitoringData(const NodeMonitoringData& monitoringData) override
    {
        std::lock_guard lock {mMutex};

        std::clog << "Sent monitoring data: system:" << monitoringData.mMonitoringData << std::endl;

        for (const auto& instance : monitoringData.mInstances) {
            std::clog << "{instance: " << instance.mInstanceIdent.mInstance << ", data:" << instance.mMonitoringData
                      << "}" << std::endl;
        }

        mData.push_back(monitoringData);
        mCondVar.notify_all();

        return ErrorEnum::eNone;
    }

    Error GetMonitoringData(NodeMonitoringData& monitoringData)
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] { return !mData.empty(); })) {
            return ErrorEnum::eTimeout;
        }

        monitoringData = std::move(mData.front());
        mData.erase(mData.begin());

        return ErrorEnum::eNone;
    }

private:
    std::mutex                      mMutex;
    std::condition_variable         mCondVar;
    std::vector<NodeMonitoringData> mData;
};

class GetAlertTag : public StaticVisitor<AlertTagEnum> {
public:
    Res Visit(const AlertItem& alert) const { return alert.mTag; }
};

/**
 * Alert sender mock.
 */
class AlertSenderStub : public alerts::SenderItf {
public:
    Error SendAlert(const AlertVariant& alert) override
    {
        std::lock_guard lock {mMutex};

        mData.push_back(alert);
        mCondVar.notify_all();

        return ErrorEnum::eNone;
    }

    template <class T>
    Error GetAlert(T& alert)
    {
        std::unique_lock lock {mMutex};

        const auto tag = alert.mTag;
        auto       it  = mData.cend();

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] {
                it = std::find_if(mData.cbegin(), mData.cend(),
                    [tag](const AlertVariant& variant) { return variant.ApplyVisitor(GetAlertTag()) == tag; });

                return it != mData.cend();
            })) {
            return ErrorEnum::eTimeout;
        }

        alert = it->GetValue<T>();

        mData.erase(it);

        return ErrorEnum::eNone;
    }

private:
    std::mutex              mMutex;
    std::condition_variable mCondVar;

    std::vector<AlertVariant> mData;
};

class NodeMonitoringProviderItfStub : public NodeMonitoringProviderItf {
public:
    Error GetNodeMonitoringData(MonitoringData& monitoringData) override
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] { return !mData.empty(); })) {
            return ErrorEnum::eTimeout;
        }

        monitoringData = std::move(mData.front());
        mData.pop();

        return ErrorEnum::eNone;
    }

    void SetMonitoringData(const MonitoringData& monitoringData)
    {
        std::lock_guard lock {mMutex};

        mData.push(monitoringData);
        mCondVar.notify_one();
    }

private:
    std::mutex              mMutex;
    std::condition_variable mCondVar;

    std::queue<MonitoringData> mData;
};

class InstanceInfoProviderItfStub : public InstanceInfoProviderItf {
public:
    Error GetInstancesStatuses(Array<InstanceStatus>& statuses) override
    {
        std::lock_guard lock {mMutex};

        return statuses.Assign(mStatuses);
    }

    Error SubscribeListener(instancestatusprovider::ListenerItf& listener) override
    {
        mListener = &listener;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeListener(instancestatusprovider::ListenerItf& listener) override
    {
        (void)listener;
        mListener = nullptr;

        return ErrorEnum::eNone;
    }

    Error SetInstanceStatus(const InstanceIdent& ident, const InstanceStateEnum state)
    {
        std::lock_guard lock {mMutex};

        InstanceStatus status;
        static_cast<InstanceIdent&>(status) = ident;
        status.mState                       = state;

        mStatuses.RemoveIf([&ident](const auto& existingStatus) {
            return static_cast<const InstanceIdent&>(existingStatus) == ident;
        });

        return mStatuses.PushBack(status);
    }

    Error OnInstancesStatusesChanged(const InstanceIdent& ident, const InstanceStateEnum state)
    {
        std::lock_guard lock {mMutex};

        if (auto err = SetInstanceStatus(ident, state); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (!mListener) {
            return ErrorEnum::eWrongState;
        }

        mListener->OnInstancesStatusesChanged(mStatuses);

        return ErrorEnum::eNone;
    }

    Error GetInstanceMonitoringParams(const InstanceIdent& instanceIdent, InstanceParams& params) const override
    {
        std::lock_guard lock {mMutex};

        auto it = mInstanceParams.find(instanceIdent);
        if (it == mInstanceParams.end()) {
            return ErrorEnum::eNotFound;
        }

        params = it->second;

        return ErrorEnum::eNone;
    }

    void SetInstanceMonitoringParams(const InstanceIdent& instanceIdent, const Optional<AlertRules>& rules)
    {
        std::lock_guard lock {mMutex};

        mInstanceParams[instanceIdent].mAlertRules = rules;
    }

    Error GetInstanceMonitoringData(const InstanceIdent& instanceIdent, InstanceMonitoringData& monitoringData) override
    {
        std::unique_lock lock {mMutex};

        auto it = mInstancesMonitoringData.cend();

        if (!mCondVar.wait_for(lock, cWaitTimeout, [&] {
                it = std::find_if(mInstancesMonitoringData.cbegin(), mInstancesMonitoringData.cend(),
                    [&instanceIdent](const auto& data) { return data.mInstanceIdent == instanceIdent; });

                return it != mInstancesMonitoringData.cend();
            })) {
            return ErrorEnum::eTimeout;
        }

        monitoringData = std::move(*it);

        mInstancesMonitoringData.erase(it);

        return ErrorEnum::eNone;
    }

    void SetInstancesMonitoringData(const InstanceIdent& ident, const MonitoringData& data)
    {
        std::lock_guard lock {mMutex};

        mInstancesMonitoringData.emplace_back();
        mInstancesMonitoringData.back().mInstanceIdent  = ident;
        mInstancesMonitoringData.back().mMonitoringData = data;

        mCondVar.notify_all();
    }

private:
    mutable std::mutex                      mMutex;
    std::condition_variable                 mCondVar;
    InstanceStatusArray                     mStatuses;
    std::map<InstanceIdent, InstanceParams> mInstanceParams;
    std::vector<InstanceMonitoringData>     mInstancesMonitoringData;
    instancestatusprovider::ListenerItf*    mListener {};
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class MonitoringTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        SetNodeInfo();
        SetNodeConfig();
        SetInstanceAlertRules();
    }

    void SetNodeInfo()
    {
        mNodeInfo.mNodeID   = cNodeID;
        mNodeInfo.mMaxDMIPS = cMaxDMIPS;
        mNodeInfo.mTotalRAM = cTotalRAM;

        mNodeInfo.mPartitions.EmplaceBack();
        mNodeInfo.mPartitions.Back().mName      = cStatesPartition;
        mNodeInfo.mPartitions.Back().mTotalSize = cStatesTotalSize;

        mNodeInfo.mPartitions.EmplaceBack();
        mNodeInfo.mPartitions.Back().mName      = cStoragePartition;
        mNodeInfo.mPartitions.Back().mTotalSize = cStorageTotalSize;

        mCurrentNodeInfoProvider.SetCurrentNodeInfo(mNodeInfo);
    }

    void SetNodeConfig()
    {
        mNodeConfig.mNodeID = cNodeID;

        mNodeConfig.mAlertRules.EmplaceValue();

        mNodeConfig.mAlertRules->mCPU.SetValue(AlertRulePercents {cPollPeriod / 3, 10, 20});
        mNodeConfig.mAlertRules->mRAM.SetValue(AlertRulePercents {cPollPeriod / 3, 20, 30});
        mNodeConfig.mAlertRules->mDownload.SetValue(AlertRulePoints {cPollPeriod / 3, 10, 20});
        mNodeConfig.mAlertRules->mUpload.SetValue(AlertRulePoints {cPollPeriod / 3, 10, 20});

        mNodeConfig.mAlertRules->mPartitions.EmplaceBack(cSysStatesRule);
        mNodeConfig.mAlertRules->mPartitions.EmplaceBack(cSysStorageRule);

        mNodeConfigProvider.SetNodeConfig(mNodeConfig);
    }

    void SetInstanceAlertRules()
    {
        mInstanceAlertRules.mCPU.SetValue(AlertRulePoints {cPollPeriod / 3, 1000, 2000});
        mInstanceAlertRules.mRAM.SetValue(AlertRulePoints {cPollPeriod / 3, 1024, 2048});
        mInstanceAlertRules.mDownload.SetValue(AlertRulePoints {cPollPeriod / 3, 30, 40});
        mInstanceAlertRules.mUpload.SetValue(AlertRulePoints {cPollPeriod / 3, 30, 40});
        mInstanceAlertRules.mPartitions.EmplaceBack(cInstanceStatesRule);
        mInstanceAlertRules.mPartitions.EmplaceBack(cInstanceStorageRule);
    }

    NodeInfo   mNodeInfo;
    NodeConfig mNodeConfig;
    AlertRules mInstanceAlertRules;

    Config                         mConfig {cPollPeriod, 3 * cPollPeriod};
    NodeConfigProviderStub         mNodeConfigProvider;
    CurrentNodeInfoProviderItfStub mCurrentNodeInfoProvider;
    SenderStub                     mSender;
    AlertSenderStub                mAlertSender;
    NodeMonitoringProviderItfStub  mNodeMonitoringProvider;
    InstanceInfoProviderItfStub    mInstanceInfoProvider;
    Monitoring                     mMonitoring;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(MonitoringTest, SystemMonitoringAlerts)
{
    const auto& cRules = mNodeConfig.mAlertRules.GetValue();

    const std::vector cMonitoringData = {
        TestData()
            .CPU(cMaxDMIPS * cRules.mCPU->mMaxThreshold / 100 - 1)
            .RAM(cTotalRAM * cRules.mRAM->mMaxThreshold / 100 - 1)
            .Download(cRules.mDownload->mMaxThreshold - 1)
            .Upload(cRules.mUpload->mMaxThreshold - 1)
            .Partition(cStatesPartition, cStatesTotalSize * cSysStatesRule.mMaxThreshold / 100 - 1)
            .Partition(cStoragePartition, cStorageTotalSize * cSysStorageRule.mMaxThreshold / 100 - 1)
            .SetTime(Time::Now()),

        TestData()
            .CPU(cMaxDMIPS * cRules.mCPU->mMaxThreshold / 100)
            .RAM(cTotalRAM * cRules.mRAM->mMaxThreshold / 100)
            .Download(cRules.mDownload->mMaxThreshold)
            .Upload(cRules.mUpload->mMaxThreshold)
            .Partition(cStatesPartition, cStatesTotalSize * cSysStatesRule.mMaxThreshold / 100)
            .Partition(cStoragePartition, cStorageTotalSize * cSysStorageRule.mMaxThreshold / 100)
            .SetTime(Time::Now().Add(1 * cPollPeriod)),

        TestData()
            .CPU(cMaxDMIPS * cRules.mCPU->mMaxThreshold / 100 + 1)
            .RAM(cTotalRAM * cRules.mRAM->mMaxThreshold / 100 + 1)
            .Download(cRules.mDownload->mMaxThreshold + 1)
            .Upload(cRules.mUpload->mMaxThreshold + 1)
            .Partition(cStatesPartition, cStatesTotalSize * cSysStatesRule.mMaxThreshold / 100 + 1)
            .Partition(cStoragePartition, cStorageTotalSize * cSysStorageRule.mMaxThreshold / 100 + 1)
            .ExpectAlert(cNodeID, "cpu", QuotaAlertStateEnum::eRaise)
            .ExpectAlert(cNodeID, "ram", QuotaAlertStateEnum::eRaise)
            .ExpectAlert(cNodeID, "download", QuotaAlertStateEnum::eRaise)
            .ExpectAlert(cNodeID, "upload", QuotaAlertStateEnum::eRaise)
            .ExpectAlert(cNodeID, "state", QuotaAlertStateEnum::eRaise)
            .ExpectAlert(cNodeID, "storage", QuotaAlertStateEnum::eRaise)
            .SetTime(Time::Now().Add(2 * cPollPeriod)),

        TestData()
            .CPU(cMaxDMIPS * cRules.mCPU->mMaxThreshold / 100)
            .RAM(cTotalRAM * cRules.mRAM->mMaxThreshold / 100)
            .Download(cRules.mDownload->mMaxThreshold)
            .Upload(cRules.mUpload->mMaxThreshold)
            .Partition(cStatesPartition, cStatesTotalSize * cSysStatesRule.mMaxThreshold / 100)
            .Partition(cStoragePartition, cStorageTotalSize * cSysStorageRule.mMaxThreshold / 100)
            .ExpectAlert(cNodeID, "cpu", QuotaAlertStateEnum::eContinue)
            .ExpectAlert(cNodeID, "ram", QuotaAlertStateEnum::eContinue)
            .ExpectAlert(cNodeID, "download", QuotaAlertStateEnum::eContinue)
            .ExpectAlert(cNodeID, "upload", QuotaAlertStateEnum::eContinue)
            .ExpectAlert(cNodeID, "state", QuotaAlertStateEnum::eContinue)
            .ExpectAlert(cNodeID, "storage", QuotaAlertStateEnum::eContinue)
            .SetTime(Time::Now().Add(3 * cPollPeriod)),

        TestData()
            .CPU(cMaxDMIPS * cRules.mCPU->mMaxThreshold / 100)
            .RAM(cTotalRAM * cRules.mRAM->mMaxThreshold / 100)
            .Download(cRules.mDownload->mMaxThreshold)
            .Upload(cRules.mUpload->mMaxThreshold)
            .Partition(cStatesPartition, cStatesTotalSize * cSysStatesRule.mMaxThreshold / 100)
            .Partition(cStoragePartition, cStorageTotalSize * cSysStorageRule.mMaxThreshold / 100)
            .ExpectAlert(cNodeID, "cpu", QuotaAlertStateEnum::eContinue)
            .ExpectAlert(cNodeID, "ram", QuotaAlertStateEnum::eContinue)
            .ExpectAlert(cNodeID, "download", QuotaAlertStateEnum::eContinue)
            .ExpectAlert(cNodeID, "upload", QuotaAlertStateEnum::eContinue)
            .ExpectAlert(cNodeID, "state", QuotaAlertStateEnum::eContinue)
            .ExpectAlert(cNodeID, "storage", QuotaAlertStateEnum::eContinue)
            .SetTime(Time::Now().Add(4 * cPollPeriod)),

        TestData()
            .CPU(cMaxDMIPS * cRules.mCPU->mMinThreshold / 100 - 1)
            .RAM(cTotalRAM * cRules.mRAM->mMinThreshold / 100 - 1)
            .Download(cRules.mDownload->mMinThreshold - 1)
            .Upload(cRules.mUpload->mMinThreshold - 1)
            .Partition(cStatesPartition, cStatesTotalSize * cSysStatesRule.mMinThreshold / 100 - 1)
            .Partition(cStoragePartition, cStorageTotalSize * cSysStorageRule.mMinThreshold / 100 - 1)
            .SetTime(Time::Now().Add(5 * cPollPeriod)),

        TestData()
            .CPU(cMaxDMIPS * cRules.mCPU->mMinThreshold / 100 - 1)
            .RAM(cTotalRAM * cRules.mRAM->mMinThreshold / 100 - 1)
            .Download(cRules.mDownload->mMinThreshold - 1)
            .Upload(cRules.mUpload->mMinThreshold - 1)
            .Partition(cStatesPartition, cStatesTotalSize * cSysStatesRule.mMinThreshold / 100 - 1)
            .Partition(cStoragePartition, cStorageTotalSize * cSysStorageRule.mMinThreshold / 100 - 1)
            .ExpectAlert(cNodeID, "cpu", QuotaAlertStateEnum::eFall)
            .ExpectAlert(cNodeID, "ram", QuotaAlertStateEnum::eFall)
            .ExpectAlert(cNodeID, "download", QuotaAlertStateEnum::eFall)
            .ExpectAlert(cNodeID, "upload", QuotaAlertStateEnum::eFall)
            .ExpectAlert(cNodeID, "state", QuotaAlertStateEnum::eFall)
            .ExpectAlert(cNodeID, "storage", QuotaAlertStateEnum::eFall)
            .SetTime(Time::Now().Add(6 * cPollPeriod)),
    };

    auto err = mMonitoring.Init(mConfig, mNodeConfigProvider, mCurrentNodeInfoProvider, mSender, mAlertSender,
        mNodeMonitoringProvider, &mInstanceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mMonitoring.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    std::vector<NodeMonitoringData> avgMonitoring;
    for (size_t i = 0; i < cMonitoringData.size(); ++i) {
        LOG_DBG() << "######################### Running test case" << Log::Field("case", i + 1);

        const auto& monitoringData = cMonitoringData[i];

        mNodeMonitoringProvider.SetMonitoringData(monitoringData);

        avgMonitoring.resize(avgMonitoring.size() + 1);

        err = mSender.GetMonitoringData(avgMonitoring.back());
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        std::vector<SystemQuotaAlert> receivedAlerts(monitoringData.mExpectedAlerts.size());
        for (auto& receivedAlert : receivedAlerts) {
            err = mAlertSender.GetAlert(receivedAlert);
            EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
        }

        EXPECT_EQ(monitoringData.mExpectedAlerts, receivedAlerts);
    }

    for (size_t i = 0; i < cMonitoringData.size(); ++i) {
        EXPECT_EQ(cMonitoringData[i], avgMonitoring[i].mMonitoringData);
    }

    err = mMonitoring.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(MonitoringTest, InstanceMonitoringAlerts)
{
    mNodeConfig.mAlertRules.Reset();
    mNodeConfigProvider.SetNodeConfig(mNodeConfig);

    const std::vector cMonitoringData = {
        TestMonitoringData().SysData({}).InstanceData(InstanceIdent {"item1", "subject1", 1},
            InstanceTestData()
                .CPU(mInstanceAlertRules.mCPU->mMaxThreshold + 1)
                .RAM(mInstanceAlertRules.mRAM->mMaxThreshold + 1)
                .Download(mInstanceAlertRules.mDownload->mMaxThreshold + 1)
                .Upload(mInstanceAlertRules.mUpload->mMaxThreshold + 1)
                .Partition(cStatesPartition, cInstanceStatesRule.mMaxThreshold + 1)
                .Partition(cStoragePartition, cInstanceStorageRule.mMaxThreshold + 1)
                .SetTime(Time::Now())),

        TestMonitoringData().SysData({}).InstanceData(InstanceIdent {"item1", "subject1", 1},
            InstanceTestData()
                .CPU(mInstanceAlertRules.mCPU->mMaxThreshold + 1)
                .RAM(mInstanceAlertRules.mRAM->mMaxThreshold + 1)
                .Download(mInstanceAlertRules.mDownload->mMaxThreshold + 1)
                .Upload(mInstanceAlertRules.mUpload->mMaxThreshold + 1)
                .Partition(cStatesPartition, cInstanceStatesRule.mMaxThreshold + 1)
                .Partition(cStoragePartition, cInstanceStorageRule.mMaxThreshold + 1)
                .ExpectAlert("cpu", QuotaAlertStateEnum::eRaise)
                .ExpectAlert("ram", QuotaAlertStateEnum::eRaise)
                .ExpectAlert("download", QuotaAlertStateEnum::eRaise)
                .ExpectAlert("upload", QuotaAlertStateEnum::eRaise)
                .ExpectAlert("state", QuotaAlertStateEnum::eRaise)
                .ExpectAlert("storage", QuotaAlertStateEnum::eRaise)
                .SetTime(Time::Now().Add(cPollPeriod))),

    };

    auto err = mMonitoring.Init(mConfig, mNodeConfigProvider, mCurrentNodeInfoProvider, mSender, mAlertSender,
        mNodeMonitoringProvider, &mInstanceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // initiliaze resource provider before starting monitoring
    std::vector<NodeMonitoringData> avgMonitoring;
    std::vector<InstanceQuotaAlert> expectedAlerts;
    std::vector<InstanceQuotaAlert> receivedAlerts;

    for (size_t i = 0; i < cMonitoringData.size(); ++i) {
        const auto& data = cMonitoringData[i];

        mNodeMonitoringProvider.SetMonitoringData(data.mSystemData);

        for (auto& [ident, monitoringData] : data.mInstancesData) {
            mInstanceInfoProvider.SetInstancesMonitoringData(ident, monitoringData);

            err = mInstanceInfoProvider.SetInstanceStatus(ident, InstanceStateEnum::eActive);
            EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

            mInstanceInfoProvider.SetInstanceMonitoringParams(ident, mInstanceAlertRules);

            expectedAlerts.insert(
                expectedAlerts.end(), monitoringData.mExpectedAlerts.begin(), monitoringData.mExpectedAlerts.end());
        }

        avgMonitoring.resize(avgMonitoring.size() + 1);
    }

    err = mMonitoring.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    size_t i = 0;

    for (auto& received : avgMonitoring) {
        LOG_DBG() << "######################### Running test case" << Log::Field("case", i + 1);

        err = mSender.GetMonitoringData(received);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    receivedAlerts.resize(expectedAlerts.size());

    for (auto& receivedAlert : receivedAlerts) {
        err = mAlertSender.GetAlert(receivedAlert);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    for (size_t i = 0; i < avgMonitoring.size(); ++i) {
        const auto& receivedData          = avgMonitoring[i];
        const auto& receivedInstancesData = receivedData.mInstances;

        const auto& cExpectedInstancesData = cMonitoringData[i];

        ASSERT_EQ(receivedInstancesData.Size(), cExpectedInstancesData.mInstancesData.size());

        for (size_t j = 0; j < cExpectedInstancesData.mInstancesData.size(); ++j) {
            EXPECT_EQ(receivedInstancesData[j].mInstanceIdent, cExpectedInstancesData.mInstancesData[j].first);
            EXPECT_EQ(receivedInstancesData[j].mMonitoringData, cExpectedInstancesData.mInstancesData[j].second);
        }
    }

    err = mMonitoring.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(MonitoringTest, SystemMonitoringAccumulatesInstancesMonitoring)
{
    mNodeConfig.mAlertRules.Reset();
    mNodeConfigProvider.SetNodeConfig(mNodeConfig);

    const std::vector cMonitoringData = {
        TestMonitoringData()
            .SysData({})
            .InstanceData(InstanceIdent {"item1", "subject1", 1},
                InstanceTestData()
                    .CPU(1000)
                    .RAM(2048)
                    .Download(100)
                    .Upload(50)
                    .Partition(cStatesPartition, 101)
                    .Partition(cStoragePartition, 102)
                    .SetTime(Time::Now()))
            .InstanceData(InstanceIdent {"item2", "subject2", 2},
                InstanceTestData()
                    .CPU(1500)
                    .RAM(1024)
                    .Download(200)
                    .Upload(150)
                    .Partition(cStatesPartition, 201)
                    .Partition(cStoragePartition, 202)
                    .SetTime(Time::Now())),
    };

    auto err = mMonitoring.Init(mConfig, mNodeConfigProvider, mCurrentNodeInfoProvider, mSender, mAlertSender,
        mNodeMonitoringProvider, &mInstanceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // initiliaze resource provider before starting monitoring
    std::vector<NodeMonitoringData> avgMonitoring;

    for (size_t i = 0; i < cMonitoringData.size(); ++i) {
        const auto& data = cMonitoringData[i];

        mNodeMonitoringProvider.SetMonitoringData(data.mSystemData);

        for (auto& [ident, monitoringData] : data.mInstancesData) {
            mInstanceInfoProvider.SetInstancesMonitoringData(ident, monitoringData);

            err = mInstanceInfoProvider.SetInstanceStatus(ident, InstanceStateEnum::eActive);
            EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

            mInstanceInfoProvider.SetInstanceMonitoringParams(ident, {});
        }

        avgMonitoring.resize(avgMonitoring.size() + 1);
    }

    err = mMonitoring.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    size_t i = 0;

    for (auto& received : avgMonitoring) {
        LOG_DBG() << "######################### Running test case" << Log::Field("case", i + 1);

        err = mSender.GetMonitoringData(received);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    for (size_t i = 0; i < avgMonitoring.size(); ++i) {
        const auto& receivedData          = avgMonitoring[i];
        const auto& receivedInstancesData = receivedData.mInstances;

        const auto& cExpectedInstancesData = cMonitoringData[i];

        ASSERT_EQ(receivedInstancesData.Size(), cExpectedInstancesData.mInstancesData.size());

        MonitoringData accumulatedData;

        for (size_t j = 0; j < cExpectedInstancesData.mInstancesData.size(); ++j) {
            EXPECT_EQ(receivedInstancesData[j].mInstanceIdent, cExpectedInstancesData.mInstancesData[j].first);
            EXPECT_EQ(receivedInstancesData[j].mMonitoringData, cExpectedInstancesData.mInstancesData[j].second);

            accumulatedData.mCPU += receivedInstancesData[j].mMonitoringData.mCPU;
            accumulatedData.mRAM += receivedInstancesData[j].mMonitoringData.mRAM;
            accumulatedData.mDownload += receivedInstancesData[j].mMonitoringData.mDownload;
            accumulatedData.mUpload += receivedInstancesData[j].mMonitoringData.mUpload;

            for (const auto& partitionData : receivedInstancesData[j].mMonitoringData.mPartitions) {
                auto it = accumulatedData.mPartitions.FindIf([&partitionData](const auto& existingPartition) {
                    return existingPartition.mName == partitionData.mName;
                });

                if (it != accumulatedData.mPartitions.end()) {
                    it->mUsedSize = Max(it->mUsedSize, partitionData.mUsedSize);
                } else {
                    accumulatedData.mPartitions.EmplaceBack();
                    accumulatedData.mPartitions.Back().mName     = partitionData.mName;
                    accumulatedData.mPartitions.Back().mUsedSize = partitionData.mUsedSize;
                }
            }
        }

        EXPECT_EQ(receivedData.mMonitoringData, accumulatedData);
    }

    err = mMonitoring.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(MonitoringTest, GetAverageMonitoringData)
{
    mConfig.mPollPeriod    = cPollPeriod;
    mConfig.mAverageWindow = cPollPeriod * 3;

    mNodeConfig.mAlertRules.Reset();
    mNodeConfigProvider.SetNodeConfig(mNodeConfig);

    const std::vector cMonitoringData = {
        TestMonitoringData()
            .SysData(SystemTestData().CPU(0).RAM(600).Download(300).Upload(300).Partition(cStatesPartition, 100))
            .InstanceData(InstanceIdent {"", "", 1},
                InstanceTestData().CPU(600).RAM(0).Download(300).Upload(300).Partition(cStatesPartition, 300)),

        TestMonitoringData()
            .SysData(SystemTestData().CPU(900).RAM(300).Download(0).Upload(300).Partition(cStatesPartition, 400))
            .InstanceData(InstanceIdent {"", "", 1},
                InstanceTestData().CPU(300).RAM(900).Download(300).Upload(0).Partition(cStatesPartition, 0)),

        TestMonitoringData()
            .SysData(SystemTestData().CPU(1200).RAM(200).Download(200).Upload(0).Partition(cStatesPartition, 500))
            .InstanceData(InstanceIdent {"", "", 1},
                InstanceTestData().CPU(200).RAM(1200).Download(0).Upload(200).Partition(cStatesPartition, 800)),
    };

    const std::vector cExpectedAverage = {
        TestMonitoringData()
            .SysData(SystemTestData().CPU(0).RAM(600).Download(300).Upload(300).Partition(cStatesPartition, 100))
            .InstanceData(InstanceIdent {"", "", 1},
                InstanceTestData().CPU(600).RAM(0).Download(300).Upload(300).Partition(cStatesPartition, 300)),

        TestMonitoringData()
            .SysData(SystemTestData().CPU(300).RAM(500).Download(200).Upload(300).Partition(cStatesPartition, 200))
            .InstanceData(InstanceIdent {"", "", 1},
                InstanceTestData().CPU(500).RAM(300).Download(300).Upload(200).Partition(cStatesPartition, 200)),

        TestMonitoringData()
            .SysData(SystemTestData().CPU(600).RAM(400).Download(200).Upload(200).Partition(cStatesPartition, 300))
            .InstanceData(InstanceIdent {"", "", 1},
                InstanceTestData().CPU(400).RAM(600).Download(200).Upload(200).Partition(cStatesPartition, 400)),
    };

    auto err = mMonitoring.Init(mConfig, mNodeConfigProvider, mCurrentNodeInfoProvider, mSender, mAlertSender,
        mNodeMonitoringProvider, &mInstanceInfoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // initiliaze resource provider before starting monitoring
    std::vector<NodeMonitoringData> avgMonitoring;

    for (size_t i = 0; i < cMonitoringData.size(); ++i) {
        const auto& data = cMonitoringData[i];

        mNodeMonitoringProvider.SetMonitoringData(data.mSystemData);

        for (auto& [ident, monitoringData] : data.mInstancesData) {
            mInstanceInfoProvider.SetInstancesMonitoringData(ident, monitoringData);

            err = mInstanceInfoProvider.SetInstanceStatus(ident, InstanceStateEnum::eActive);
            EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

            mInstanceInfoProvider.SetInstanceMonitoringParams(ident, {});
        }

        avgMonitoring.resize(avgMonitoring.size() + 1);
    }

    err = mMonitoring.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    size_t i = 0;

    for (auto& avgData : avgMonitoring) {
        LOG_DBG() << "######################### Running test case" << Log::Field("case", i + 1);

        auto sentData = std::make_unique<NodeMonitoringData>();

        err = mSender.GetMonitoringData(*sentData);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        err = mMonitoring.GetAverageMonitoringData(avgData);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    for (size_t i = 0; i < avgMonitoring.size(); ++i) {
        const auto& receivedData          = avgMonitoring[i];
        const auto& receivedInstancesData = receivedData.mInstances;

        const auto& cExpectedInstancesData = cExpectedAverage[i];

        ASSERT_EQ(receivedInstancesData.Size(), cExpectedInstancesData.mInstancesData.size());

        for (size_t j = 0; j < cExpectedInstancesData.mInstancesData.size(); ++j) {
            EXPECT_EQ(receivedInstancesData[j].mInstanceIdent, cExpectedInstancesData.mInstancesData[j].first);
            EXPECT_EQ(receivedInstancesData[j].mMonitoringData, cExpectedInstancesData.mInstancesData[j].second);
        }

        EXPECT_EQ(receivedData.mMonitoringData, cExpectedAverage[i].mSystemData);
    }

    err = mMonitoring.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

} // namespace aos::monitoring
