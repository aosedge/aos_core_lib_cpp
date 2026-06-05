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
    const ResourceIdentifier id           = {ResourceLevelEnum::eSystem, resourceType.GetValue(), {}, {}};

    AlertProcessor alertProcessor;

    {
        AlertVariant alertTemplate;
        alertTemplate.SetValue<SystemQuotaAlert>(CreateSystemQuotaAlert("node-id", resourceType.ToString(), 0));
        ASSERT_TRUE(alertProcessor.Init(id, rulePoints, mAlertSender, alertTemplate).IsNone());
    }

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

} // namespace aos::monitoring
