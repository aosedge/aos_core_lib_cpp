/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mutex>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "aos/common/monitoring/alertprocessor.hpp"
#include "aos/test/log.hpp"

#include "mocks/alertsmock.hpp"

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

cloudprotocol::SystemQuotaAlert CreateSystemQuotaAlert(const String& nodeID, const String& parameter, uint64_t value,
    std::optional<cloudprotocol::AlertStatus> status = std::nullopt, const Time& timestamp = Time())
{
    cloudprotocol::SystemQuotaAlert systemQuotaAlert(timestamp);

    systemQuotaAlert.mNodeID    = nodeID;
    systemQuotaAlert.mParameter = parameter;
    systemQuotaAlert.mValue     = value;

    if (status) {
        systemQuotaAlert.mStatus = *status;
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
    void SetUp() override { test::InitLog(); }

    alerts::AlertSenderMock mAlertSender;
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
        cloudprotocol::AlertVariant alertTemplate;
        alertTemplate.SetValue<cloudprotocol::SystemQuotaAlert>(
            CreateSystemQuotaAlert("node-id", resourceType.ToString(), 0));
        ASSERT_TRUE(alertProcessor.Init(id, rulePoints, mAlertSender, alertTemplate).IsNone());
    }

    Time currentTime = Time::Now();

    struct {
        uint64_t                                  mCurrentValue;
        Duration                                  mTimeDelta;
        std::optional<cloudprotocol::AlertStatus> mExpectedStatus;
    } testCases[] = {
        {1, 0, {}},
        {2, rulePoints.mMinTimeout, {}},
        {90, 2 * rulePoints.mMinTimeout, {}},
        {91, 2 * rulePoints.mMinTimeout, {}},
        {95, 2 * rulePoints.mMinTimeout, {}},
        {96, 2 * rulePoints.mMinTimeout, {cloudprotocol::AlertStatusEnum::eRaise}},
        {90, 2 * rulePoints.mMinTimeout, {cloudprotocol::AlertStatusEnum::eContinue}},
        {80, 2 * rulePoints.mMinTimeout, {}},
        {80, 2 * rulePoints.mMinTimeout, {cloudprotocol::AlertStatusEnum::eFall}},
    };

    for (const auto& testCase : testCases) {

        currentTime = currentTime.Add(testCase.mTimeDelta);

        const auto expectedAlert = CreateSystemQuotaAlert(
            "node-id", resourceType.ToString(), testCase.mCurrentValue, testCase.mExpectedStatus, currentTime);

        if (testCase.mExpectedStatus) {
            EXPECT_CALL(mAlertSender, SendAlert).WillOnce(Invoke([&expectedAlert](const auto& alert) {
                CompareAlertVisitor<cloudprotocol::SystemQuotaAlert> visitor(expectedAlert);

                EXPECT_TRUE(alert.ApplyVisitor(visitor));

                return ErrorEnum::eNone;
            }));
        } else {
            EXPECT_CALL(mAlertSender, SendAlert).Times(0);
        }

        EXPECT_TRUE(alertProcessor.CheckAlertDetection(testCase.mCurrentValue, currentTime).IsNone());
    }
}

TEST_F(AlertProcessorTest, CheckRulePercentAlertDetection)
{
    const ResourceType       resourceType = ResourceTypeEnum::eCPU;
    const uint64_t           maxDMIPS     = 10000;
    const AlertRulePercents  rulePercents = {Time::cSeconds, 90, 95};
    const ResourceIdentifier id           = {ResourceLevelEnum::eSystem, resourceType.GetValue(), {}, {}};

    AlertProcessor alertProcessor;

    {
        cloudprotocol::AlertVariant alertTemplate;
        alertTemplate.SetValue<cloudprotocol::SystemQuotaAlert>(
            CreateSystemQuotaAlert("node-id", resourceType.ToString(), 0));
        ASSERT_TRUE(alertProcessor.Init(id, maxDMIPS, rulePercents, mAlertSender, alertTemplate).IsNone());
    }

    Time currentTime = Time::Now();

    struct {
        uint64_t                                  mCurrentValue;
        Duration                                  mTimeDelta;
        std::optional<cloudprotocol::AlertStatus> mExpectedStatus;
    } testCases[] = {
        {1 * maxDMIPS / 100, 0, {}},
        {2 * maxDMIPS / 100, rulePercents.mMinTimeout, {}},
        {90 * maxDMIPS / 100, 2 * rulePercents.mMinTimeout, {}},
        {91 * maxDMIPS / 100, 2 * rulePercents.mMinTimeout, {}},
        {95 * maxDMIPS / 100, 2 * rulePercents.mMinTimeout, {}},
        {96 * maxDMIPS / 100, 2 * rulePercents.mMinTimeout, {cloudprotocol::AlertStatusEnum::eRaise}},
        {90 * maxDMIPS / 100, 2 * rulePercents.mMinTimeout, {cloudprotocol::AlertStatusEnum::eContinue}},
        {80 * maxDMIPS / 100, 2 * rulePercents.mMinTimeout, {}},
        {70 * maxDMIPS / 100, 2 * rulePercents.mMinTimeout, {cloudprotocol::AlertStatusEnum::eFall}},
    };

    for (const auto& testCase : testCases) {

        currentTime = currentTime.Add(testCase.mTimeDelta);

        const auto expectedAlert = CreateSystemQuotaAlert(
            "node-id", resourceType.ToString(), testCase.mCurrentValue, testCase.mExpectedStatus, currentTime);

        if (testCase.mExpectedStatus) {
            EXPECT_CALL(mAlertSender, SendAlert).WillOnce(Invoke([&expectedAlert](const auto& alert) {
                CompareAlertVisitor<cloudprotocol::SystemQuotaAlert> visitor(expectedAlert);

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
