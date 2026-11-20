/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mutex>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/monitoring/alertprocessor.hpp>
#include <core/common/tests/mocks/alertsmock.hpp>
#include <core/common/tests/utils/log.hpp>

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
SystemQuotaAlert CreateSystemQuotaAlert(const String& nodeID, const String& parameter, uint64_t value,
    std::optional<QuotaAlertState> state = std::nullopt, const Time& timestamp = Time())
{
    SystemQuotaAlert systemQuotaAlert;

    systemQuotaAlert.mTimestamp = timestamp;
    systemQuotaAlert.mNodeID    = nodeID;
    systemQuotaAlert.mParameter = parameter;
    systemQuotaAlert.mValue     = value;

    if (state) {
        systemQuotaAlert.mState = *state;
    }

    return systemQuotaAlert;
}

template <class T>
class CompareAlertVisitor : public StaticVisitor<bool> {
public:
    CompareAlertVisitor(const T& expectedVal)
        : mExpected(expectedVal)
    {
    }

    Res Visit(const T& val) const { return val == mExpected; }

    Res Visit(...) const { return false; }

private:
    T mExpected;
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class AlertProcessorTest : public Test {
protected:
    void SetUp() override { tests::utils::InitLog(); }

    alerts::SenderMock mAlertSender;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(AlertProcessorTest, CheckRulePointAlertDetection)
{
    const ResourceType       resourceType = ResourceTypeEnum::eDownload;
    const AlertRulePoints    rulePoints   = {Time::cSeconds, 90, 95};
    const String             nodeID       = "node-id";
    const ResourceIdentifier id           = {nodeID, ResourceLevelEnum::eSystem, resourceType.GetValue(), {}};

    AlertProcessor alertProcessor;

    ASSERT_TRUE(alertProcessor.Init(id, rulePoints, mAlertSender).IsNone());

    Time currentTime = Time::Now();

    struct {
        uint64_t                       mCurrentValue;
        Duration                       mTimeDelta;
        std::optional<QuotaAlertState> mExpectedState;
    } testCases[] = {
        {1, 0, {}},
        {2, rulePoints.mMinTimeout, {}},
        {90, 2 * rulePoints.mMinTimeout, {}},
        {91, 2 * rulePoints.mMinTimeout, {}},
        {95, 2 * rulePoints.mMinTimeout, {}},
        {96, 2 * rulePoints.mMinTimeout, {QuotaAlertStateEnum::eRaise}},
        {90, 2 * rulePoints.mMinTimeout, {QuotaAlertStateEnum::eContinue}},
        {80, 2 * rulePoints.mMinTimeout, {}},
        {80, 2 * rulePoints.mMinTimeout, {QuotaAlertStateEnum::eFall}},
    };

    for (const auto& testCase : testCases) {
        currentTime = currentTime.Add(testCase.mTimeDelta);

        const auto expectedAlert = CreateSystemQuotaAlert(
            "node-id", resourceType.ToString(), testCase.mCurrentValue, testCase.mExpectedState, currentTime);

        if (testCase.mExpectedState) {
            EXPECT_CALL(mAlertSender, SendAlert).WillOnce(Invoke([&expectedAlert](const auto& alert) {
                CompareAlertVisitor<SystemQuotaAlert> visitor(expectedAlert);

                EXPECT_TRUE(alert.ApplyVisitor(visitor));

                return ErrorEnum::eNone;
            }));
        } else {
            EXPECT_CALL(mAlertSender, SendAlert).Times(0);
        }

        EXPECT_TRUE(alertProcessor.CheckAlertDetection(testCase.mCurrentValue, currentTime).IsNone());
    }
}

TEST_F(AlertProcessorTest, MaxThresholdTimerResetWhenValueDrops)
{
    const AlertRulePoints    rulePoints = {Time::cSeconds, 90, 95};
    const ResourceIdentifier id         = {"node-id", ResourceLevelEnum::eSystem, ResourceTypeEnum::eCPU, {}};

    AlertProcessor alertProcessor;

    ASSERT_TRUE(alertProcessor.Init(id, rulePoints, mAlertSender).IsNone());

    EXPECT_CALL(mAlertSender, SendAlert).Times(0);

    const auto now = Time::Now();

    EXPECT_TRUE(alertProcessor.CheckAlertDetection(96, now).IsNone());
    EXPECT_TRUE(alertProcessor.CheckAlertDetection(10, now.Add(rulePoints.mMinTimeout / 2)).IsNone());

    // The max threshold timer was restarted, so the timeout counts from this moment.
    EXPECT_TRUE(alertProcessor.CheckAlertDetection(96, now.Add(2 * rulePoints.mMinTimeout)).IsNone());
}

TEST_F(AlertProcessorTest, SendAlertFailures)
{
    const AlertRulePoints    rulePoints = {Time::cSeconds, 90, 95};
    const ResourceIdentifier id         = {"node-id", ResourceLevelEnum::eSystem, ResourceTypeEnum::eRAM, {}};

    AlertProcessor alertProcessor;

    ASSERT_TRUE(alertProcessor.Init(id, rulePoints, mAlertSender).IsNone());

    EXPECT_CALL(mAlertSender, SendAlert).Times(3).WillRepeatedly(Return(ErrorEnum::eFailed));

    auto currentTime = Time::Now();

    EXPECT_TRUE(alertProcessor.CheckAlertDetection(96, currentTime).IsNone());

    currentTime = currentTime.Add(2 * rulePoints.mMinTimeout);
    EXPECT_TRUE(alertProcessor.CheckAlertDetection(96, currentTime).Is(ErrorEnum::eFailed));

    currentTime = currentTime.Add(2 * rulePoints.mMinTimeout);
    EXPECT_TRUE(alertProcessor.CheckAlertDetection(90, currentTime).Is(ErrorEnum::eFailed));

    EXPECT_TRUE(alertProcessor.CheckAlertDetection(10, currentTime).IsNone());

    currentTime = currentTime.Add(2 * rulePoints.mMinTimeout);
    EXPECT_TRUE(alertProcessor.CheckAlertDetection(10, currentTime).Is(ErrorEnum::eFailed));
}

TEST_F(AlertProcessorTest, InvalidResourceLevel)
{
    const AlertRulePoints    rulePoints = {Time::cSeconds, 90, 95};
    const ResourceIdentifier id
        = {"node-id", ResourceLevel(static_cast<ResourceLevelEnum>(100)), ResourceTypeEnum::eUpload, {}};

    AlertProcessor alertProcessor;

    ASSERT_TRUE(alertProcessor.Init(id, rulePoints, mAlertSender).IsNone());

    EXPECT_CALL(mAlertSender, SendAlert).Times(0);

    const auto now = Time::Now();

    EXPECT_TRUE(alertProcessor.CheckAlertDetection(96, now).IsNone());
    EXPECT_TRUE(
        alertProcessor.CheckAlertDetection(96, now.Add(2 * rulePoints.mMinTimeout)).Is(ErrorEnum::eInvalidArgument));
}

} // namespace aos::monitoring
