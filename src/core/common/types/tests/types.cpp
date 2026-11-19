/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <core/common/types/alerts.hpp>
#include <core/common/types/blobs.hpp>
#include <core/common/types/certificates.hpp>
#include <core/common/types/desiredstatus.hpp>
#include <core/common/types/envvars.hpp>
#include <core/common/types/instance.hpp>
#include <core/common/types/log.hpp>
#include <core/common/types/monitoring.hpp>
#include <core/common/types/network.hpp>
#include <core/common/types/permissions.hpp>
#include <core/common/types/provisioning.hpp>
#include <core/common/types/state.hpp>
#include <core/common/types/unitconfig.hpp>
#include <core/common/types/unitstatus.hpp>

using namespace aos;

TEST(CommonTest, Types)
{
    // InstanceIdent comparison
    EXPECT_TRUE((InstanceIdent {"service1", "1.0.0", "subject1", 2, UpdateItemTypeEnum::eService})
        == (InstanceIdent {"service1", "1.0.0", "subject1", 2, UpdateItemTypeEnum::eService}));
    EXPECT_FALSE((InstanceIdent {"service1", "1.0.0", "subject1", 2, UpdateItemTypeEnum::eService})
        != (InstanceIdent {"service1", "1.0.0", "subject1", 2, UpdateItemTypeEnum::eService}));

    // InstanceInfo comparison
    EXPECT_TRUE((InstanceInfo {{"service1", "1.0.0", "subject1", 2, UpdateItemTypeEnum::eService}, "image1", "runc", "",
                    {}, 2, 3, 4, "state", "storage", {}, {}, false})
        == (InstanceInfo {{"service1", "1.0.0", "subject1", 2, UpdateItemTypeEnum::eService}, "image1", "runc", "", {},
            2, 3, 4, "state", "storage", {}, {}, false}));
    EXPECT_FALSE((InstanceInfo {{"service1", "1.0.0", "subject1", 2, UpdateItemTypeEnum::eService}, "image1", "runc",
                     "", {}, 2, 3, 4, "state", "storage", {}, {}, false})
        != (InstanceInfo {{"service1", "1.0.0", "subject1", 2, UpdateItemTypeEnum::eService}, "image1", "runc", "", {},
            2, 3, 4, "state", "storage", {}, {}, false}));

    // InstanceStatus comparison
    EXPECT_TRUE((InstanceStatus {"service1", "3.0.0", "subject1", 2, UpdateItemTypeEnum::eService, "node0", "runc",
                    "image0", {}, {}, InstanceStateEnum::eActive, ErrorEnum::eNone, false})
        == (InstanceStatus {"service1", "3.0.0", "subject1", 2, UpdateItemTypeEnum::eService, "node0", "runc", "image0",
            {}, {}, InstanceStateEnum::eActive, ErrorEnum::eNone, false}));
    EXPECT_FALSE((InstanceStatus {"service1", "3.0.0", "subject1", 2, UpdateItemTypeEnum::eService, "node0", "runc",
                     "image0", {}, {}, InstanceStateEnum::eActive, ErrorEnum::eNone, false})
        != (InstanceStatus {"service1", "3.0.0", "subject1", 2, UpdateItemTypeEnum::eService, "node0", "runc", "image0",
            {}, {}, InstanceStateEnum::eActive, ErrorEnum::eNone, false}));
}

TEST(CommonTest, NodeRootCertificatesComparison)
{
    NodeRootCertificates rootCert1;
    NodeRootCertificates rootCert2;

    rootCert1.mNodeID = "node0";
    ASSERT_TRUE(rootCert1.mSHA256Fingerprints.EmplaceBack("fingerprint0").IsNone());

    rootCert2 = rootCert1;

    EXPECT_EQ(rootCert1, rootCert2);

    rootCert2.mNodeID = "node1";

    EXPECT_NE(rootCert1, rootCert2);
}

TEST(CommonTest, UnitRootCertificatesComparison)
{
    UnitRootCertificates rootCert1;
    UnitRootCertificates rootCert2;

    rootCert1.mCorrelationID = "correlation0";
    rootCert1.mIsPartial     = true;
    ASSERT_TRUE(rootCert1.mNodeCertificates.EmplaceBack().IsNone());
    rootCert1.mNodeCertificates.Back().mNodeID = "node0";

    rootCert2 = rootCert1;

    EXPECT_EQ(rootCert1, rootCert2);

    rootCert2.mIsPartial = false;

    EXPECT_NE(rootCert1, rootCert2);
}

TEST(CommonTest, DesiredNodeRootCertificatesComparison)
{
    auto rootCert1 = std::make_unique<DesiredNodeRootCertificates>();
    auto rootCert2 = std::make_unique<DesiredNodeRootCertificates>();

    rootCert1->mNodeID = "node0";
    ASSERT_TRUE(rootCert1->mCertificates.EmplaceBack("pem0").IsNone());

    *rootCert2 = *rootCert1;

    EXPECT_EQ(*rootCert1, *rootCert2);

    rootCert2->mCertificates[0] = "pem1";

    EXPECT_NE(*rootCert1, *rootCert2);
}

TEST(CommonTest, DesiredUnitRootCertificatesComparison)
{
    auto rootCert1 = std::make_unique<DesiredUnitRootCertificates>();
    auto rootCert2 = std::make_unique<DesiredUnitRootCertificates>();

    rootCert1->mCorrelationID = "correlation0";
    ASSERT_TRUE(rootCert1->mNodeCertificates.EmplaceBack().IsNone());
    rootCert1->mNodeCertificates.Back().mNodeID = "node0";

    *rootCert2 = *rootCert1;

    EXPECT_EQ(*rootCert1, *rootCert2);

    rootCert2->mCorrelationID = "correlation1";

    EXPECT_NE(*rootCert1, *rootCert2);
}

TEST(CommonTest, IsMainNodeReturnsFalseOnEmptyAttrs)
{
    NodeInfo nodeInfo;

    EXPECT_FALSE(nodeInfo.IsMainNode()) << "Node has no main node attribute";
}

TEST(CommonTest, IsMainNodeReturnsTrue)
{
    NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({"MainNode", ""});

    EXPECT_TRUE(nodeInfo.IsMainNode()) << "Node has main node attribute";
}

TEST(CommonTest, IsMainNodeReturnsTrueCaseInsensitive)
{
    NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({"mainNODE", ""});

    EXPECT_TRUE(nodeInfo.IsMainNode()) << "Node has main node attribute";
}

TEST(CommonTest, NodeContainsComponent)
{
    NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({cAttrAosComponents, "cm,sm"});

    EXPECT_TRUE(nodeInfo.ContainsComponent(CoreComponentEnum::eCM)) << "Node has component CM";
    EXPECT_TRUE(nodeInfo.ContainsComponent(CoreComponentEnum::eSM)) << "Node has component SM";
    EXPECT_FALSE(nodeInfo.ContainsComponent(CoreComponentEnum::eIAM)) << "Node has no component IAM";
}

namespace {

std::string sLastLogMessage;

std::string LogToString(const std::function<void(Log&)>& write)
{
    Log::SetCallback([](const String&, LogLevel, const String& message) { sLastLogMessage = message.CStr(); });

    {
        Log log("test", LogLevelEnum::eDebug);

        write(log);
    }

    Log::SetCallback(nullptr);

    return sLastLogMessage;
}

template <typename T>
void CheckCompare(const T& base, const std::vector<std::function<void(T&)>>& mutators)
{
    EXPECT_TRUE(base == T(base));
    EXPECT_FALSE(base != T(base));

    for (size_t i = 0; i < mutators.size(); i++) {
        T other = base;

        mutators[i](other);

        EXPECT_TRUE(base != other) << "mutator " << i;
        EXPECT_FALSE(base == other) << "mutator " << i;
    }
}

} // namespace

TEST(CommonTest, AlertsCompareAndPrint)
{
    SystemAlert systemAlert;
    systemAlert.mNodeID  = "node";
    systemAlert.mMessage = "sysmsg";

    CheckCompare<SystemAlert>(systemAlert,
        {[](SystemAlert& a) { a.mNodeID = "other"; }, [](SystemAlert& a) { a.mMessage = "other"; },
            [](SystemAlert& a) { a.mTimestamp = a.mTimestamp.Add(Time::cSeconds); }});

    CoreAlert coreAlert;
    coreAlert.mNodeID        = "node";
    coreAlert.mCoreComponent = CoreComponentEnum::eSM;
    coreAlert.mMessage       = "coremsg";

    CheckCompare<CoreAlert>(coreAlert,
        {[](CoreAlert& a) { a.mNodeID = "other"; }, [](CoreAlert& a) { a.mCoreComponent = CoreComponentEnum::eCM; },
            [](CoreAlert& a) { a.mMessage = "other"; }});

    ResourceAllocateAlert resourceAlert;
    resourceAlert.mItemID    = "item";
    resourceAlert.mSubjectID = "subject";
    resourceAlert.mNodeID    = "node";
    resourceAlert.mResource  = "resource";
    resourceAlert.mMessage   = "resmsg";

    CheckCompare<ResourceAllocateAlert>(resourceAlert,
        {[](ResourceAllocateAlert& a) { a.mItemID = "other"; }, [](ResourceAllocateAlert& a) { a.mNodeID = "other"; },
            [](ResourceAllocateAlert& a) { a.mResource = "other"; },
            [](ResourceAllocateAlert& a) { a.mMessage = "other"; }});

    SystemQuotaAlert systemQuotaAlert;
    systemQuotaAlert.mNodeID    = "node";
    systemQuotaAlert.mParameter = "cpu";
    systemQuotaAlert.mValue     = 42;
    systemQuotaAlert.mState     = QuotaAlertStateEnum::eRaise;

    CheckCompare<SystemQuotaAlert>(systemQuotaAlert,
        {[](SystemQuotaAlert& a) { a.mNodeID = "other"; }, [](SystemQuotaAlert& a) { a.mParameter = "other"; },
            [](SystemQuotaAlert& a) { a.mValue = 1; },
            [](SystemQuotaAlert& a) { a.mState = QuotaAlertStateEnum::eFall; }});

    InstanceQuotaAlert instanceQuotaAlert;
    instanceQuotaAlert.mItemID    = "item";
    instanceQuotaAlert.mParameter = "ram";
    instanceQuotaAlert.mValue     = 7;
    instanceQuotaAlert.mState     = QuotaAlertStateEnum::eContinue;

    CheckCompare<InstanceQuotaAlert>(instanceQuotaAlert,
        {[](InstanceQuotaAlert& a) { a.mItemID = "other"; }, [](InstanceQuotaAlert& a) { a.mParameter = "other"; },
            [](InstanceQuotaAlert& a) { a.mValue = 1; },
            [](InstanceQuotaAlert& a) { a.mState = QuotaAlertStateEnum::eFall; }});

    DownloadAlert downloadAlert;
    downloadAlert.mDigest          = "sha256:abc";
    downloadAlert.mURL             = "http://url";
    downloadAlert.mDownloadedBytes = 10;
    downloadAlert.mTotalBytes      = 20;
    downloadAlert.mState           = DownloadStateEnum::ePaused;
    downloadAlert.mReason.SetValue("reason");
    downloadAlert.mError = ErrorEnum::eFailed;

    CheckCompare<DownloadAlert>(downloadAlert,
        {[](DownloadAlert& a) { a.mDigest = "other"; }, [](DownloadAlert& a) { a.mURL = "other"; },
            [](DownloadAlert& a) { a.mDownloadedBytes = 1; }, [](DownloadAlert& a) { a.mTotalBytes = 1; },
            [](DownloadAlert& a) { a.mState = DownloadStateEnum::eFinished; },
            [](DownloadAlert& a) { a.mReason.Reset(); }, [](DownloadAlert& a) { a.mError = ErrorEnum::eNone; }});

    InstanceAlert instanceAlert;
    instanceAlert.mItemID    = "item";
    instanceAlert.mSubjectID = "subject";
    instanceAlert.mVersion   = "1.0.0";
    instanceAlert.mMessage   = "instmsg";

    CheckCompare<InstanceAlert>(instanceAlert,
        {[](InstanceAlert& a) { a.mItemID = "other"; }, [](InstanceAlert& a) { a.mVersion = "other"; },
            [](InstanceAlert& a) { a.mMessage = "other"; }});

    EXPECT_NE(LogToString([&](Log& log) { log << systemAlert; }).find("sysmsg"), std::string::npos);
    EXPECT_NE(LogToString([&](Log& log) { log << coreAlert; }).find("coremsg"), std::string::npos);
    EXPECT_NE(LogToString([&](Log& log) { log << resourceAlert; }).find("resmsg"), std::string::npos);
    EXPECT_NE(LogToString([&](Log& log) { log << systemQuotaAlert; }).find("42"), std::string::npos);
    EXPECT_NE(LogToString([&](Log& log) { log << instanceQuotaAlert; }).find("ram"), std::string::npos);
    EXPECT_NE(LogToString([&](Log& log) { log << downloadAlert; }).find("reason"), std::string::npos);
    EXPECT_NE(LogToString([&](Log& log) { log << instanceAlert; }).find("instmsg"), std::string::npos);

    EXPECT_STREQ(DownloadState(DownloadStateEnum::eInterrupted).ToString().CStr(), "interrupted");
    EXPECT_STREQ(StateResult(StateResultEnum::eRejected).ToString().CStr(), "rejected");
}

TEST(CommonTest, StructsCompare)
{
    InstanceFilter filter;
    filter.mItemID.SetValue("item");
    filter.mSubjectID.SetValue("subject");
    filter.mInstance.SetValue(1);

    CheckCompare<InstanceFilter>(filter,
        {[](InstanceFilter& f) { f.mItemID.SetValue("other"); }, [](InstanceFilter& f) { f.mSubjectID.Reset(); },
            [](InstanceFilter& f) { f.mInstance.SetValue(2); }});

    InstanceIdent ident;
    ident.mItemID    = "a";
    ident.mSubjectID = "b";
    ident.mInstance  = 1;

    InstanceIdent other = ident;

    other.mSubjectID = "c";
    EXPECT_TRUE(ident < other);
    EXPECT_FALSE(other < ident);

    AlertRulePoints points;
    points.mMinTimeout   = Time::cSeconds;
    points.mMinThreshold = 1;
    points.mMaxThreshold = 2;

    CheckCompare<AlertRulePoints>(points,
        {[](AlertRulePoints& p) { p.mMinTimeout = Time::cMinutes; }, [](AlertRulePoints& p) { p.mMinThreshold = 5; },
            [](AlertRulePoints& p) { p.mMaxThreshold = 5; }});

    PartitionAlertRule partitionRule;
    partitionRule.mName         = "part";
    partitionRule.mMinThreshold = 1.0;

    CheckCompare<PartitionAlertRule>(partitionRule,
        {[](PartitionAlertRule& r) { r.mName = "other"; }, [](PartitionAlertRule& r) { r.mMinThreshold = 2.0; }});

    ResourceRatios ratios;
    ratios.mCPU.SetValue(1.0);
    ratios.mRAM.SetValue(2.0);
    ratios.mStorage.SetValue(3.0);
    ratios.mState.SetValue(4.0);

    CheckCompare<ResourceRatios>(ratios,
        {[](ResourceRatios& r) { r.mCPU.SetValue(9.0); }, [](ResourceRatios& r) { r.mRAM.SetValue(9.0); },
            [](ResourceRatios& r) { r.mStorage.SetValue(9.0); }, [](ResourceRatios& r) { r.mState.SetValue(9.0); }});

    CPUInfo cpu;
    cpu.mModelName              = "model";
    cpu.mNumCores               = 2;
    cpu.mNumThreads             = 4;
    cpu.mArchInfo.mArchitecture = "arm64";
    cpu.mMaxDMIPS.SetValue(100);

    CheckCompare<CPUInfo>(cpu,
        {[](CPUInfo& c) { c.mModelName = "other"; }, [](CPUInfo& c) { c.mNumCores = 1; },
            [](CPUInfo& c) { c.mNumThreads = 1; }, [](CPUInfo& c) { c.mArchInfo.mArchitecture = "x86"; },
            [](CPUInfo& c) { c.mMaxDMIPS.Reset(); }});

    PartitionInfo partition;
    partition.mName = "part";
    partition.mPath = "/path";
    partition.mTypes.PushBack("ext4");
    partition.mTotalSize = 100;

    CheckCompare<PartitionInfo>(partition,
        {[](PartitionInfo& p) { p.mName = "other"; }, [](PartitionInfo& p) { p.mPath = "/other"; },
            [](PartitionInfo& p) { p.mTypes.Clear(); }, [](PartitionInfo& p) { p.mTotalSize = 1; }});

    NodeAttribute attr;
    attr.mName  = "name";
    attr.mValue = "value";

    CheckCompare<NodeAttribute>(
        attr, {[](NodeAttribute& a) { a.mName = "other"; }, [](NodeAttribute& a) { a.mValue = "other"; }});

    EnvVarInfo envVar;
    envVar.mName  = "name";
    envVar.mValue = "value";
    envVar.mTTL.SetValue(Time::Unix(100));

    CheckCompare<EnvVarInfo>(envVar, {[](EnvVarInfo& e) { e.mName = "other"; }, [](EnvVarInfo& e) { e.mTTL.Reset(); }});

    EnvVarsInstanceInfo envVars(filter, Array<EnvVarInfo>(&envVar, 1));

    CheckCompare<EnvVarsInstanceInfo>(envVars,
        {[](EnvVarsInstanceInfo& e) { e.mItemID.SetValue("other"); },
            [](EnvVarsInstanceInfo& e) { e.mVariables.Clear(); }});

    DesiredNodeStateInfo desiredState;
    desiredState.mNodeID = "node";
    desiredState.mState  = DesiredNodeStateEnum::eProvisioned;

    CheckCompare<DesiredNodeStateInfo>(desiredState,
        {[](DesiredNodeStateInfo& d) { d.mNodeID = "other"; },
            [](DesiredNodeStateInfo& d) { d.mState = DesiredNodeStateEnum::ePaused; }});

    NodeConfig nodeConfig;
    nodeConfig.mNodeID   = "node";
    nodeConfig.mNodeType = "type";
    nodeConfig.mVersion  = "1.0.0";
    nodeConfig.mLabels.PushBack("label");
    nodeConfig.mPriority = 1;

    CheckCompare<NodeConfig>(nodeConfig,
        {[](NodeConfig& c) { c.mNodeID = "other"; }, [](NodeConfig& c) { c.mNodeType = "other"; },
            [](NodeConfig& c) { c.mVersion = "other"; }, [](NodeConfig& c) { c.mAlertRules.EmplaceValue(); },
            [](NodeConfig& c) { c.mResourceRatios.EmplaceValue(); }, [](NodeConfig& c) { c.mLabels.Clear(); },
            [](NodeConfig& c) { c.mPriority = 2; }});
}

TEST(CommonTest, UnitInstancesStatusesCompare)
{
    UnitInstanceStatus status1;
    UnitInstanceStatus status2;

    status1.mInstance = 1;
    status2.mInstance = 2;

    UnitInstancesStatuses lhs("item", UpdateItemTypeEnum::eService, "subject", "1.0.0", false);
    UnitInstancesStatuses rhs("item", UpdateItemTypeEnum::eService, "subject", "1.0.0", false);

    EXPECT_TRUE(lhs == rhs);

    ASSERT_TRUE(lhs.mInstances.PushBack(&status1).IsNone());
    EXPECT_TRUE(lhs != rhs);

    ASSERT_TRUE(rhs.mInstances.PushBack(&status2).IsNone());
    EXPECT_TRUE(lhs != rhs);

    rhs.mInstances.Clear();
    ASSERT_TRUE(rhs.mInstances.PushBack(&status1).IsNone());
    EXPECT_TRUE(lhs == rhs);

    rhs.mVersion = "2.0.0";
    EXPECT_TRUE(lhs != rhs);
}

TEST(CommonTest, NodeContainsComponentTooManyComponents)
{
    NodeInfo nodeInfo;

    nodeInfo.mAttrs.PushBack({cAttrAosComponents, "cm,sm,iam,cm,sm,iam,cm,sm,iam,cm,sm,iam"});

    EXPECT_FALSE(nodeInfo.ContainsComponent(CoreComponentEnum::eCM));
}
