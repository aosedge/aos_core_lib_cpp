/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "alertprocessor.hpp"

namespace aos::monitoring {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

class CreateAlertVisitor : public StaticVisitor<AlertVariant> {
public:
    CreateAlertVisitor(uint64_t currentValue, const Time& currentTime, QuotaAlertState state)
        : mCurrentVal(currentValue)
        , mCurrentTime(currentTime)
        , mState(state)
    {
    }

    Res Visit(const SystemQuotaAlert& val) const
    {
        auto systemQuotaAlert = val;

        systemQuotaAlert.mTimestamp = mCurrentTime;
        systemQuotaAlert.mValue     = mCurrentVal;
        systemQuotaAlert.mState     = mState;

        Res result;
        result.SetValue<SystemQuotaAlert>(systemQuotaAlert);

        return result;
    }

    Res Visit(const InstanceQuotaAlert& val) const
    {
        auto instanceQuotaAlert = val;

        instanceQuotaAlert.mTimestamp = mCurrentTime;
        instanceQuotaAlert.mValue     = mCurrentVal;
        instanceQuotaAlert.mState     = mState;

        Res result;
        result.SetValue<InstanceQuotaAlert>(instanceQuotaAlert);

        return result;
    }

    template <typename T>
    Res Visit(const T&) const
    {
        assert(false);

        return {};
    }

private:
    uint64_t        mCurrentVal {};
    Time            mCurrentTime;
    QuotaAlertState mState;
};

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error AlertProcessor::Init(
    ResourceIdentifier id, const AlertRulePoints& rule, alerts::SenderItf& sender, const AlertVariant& alertTemplate)
{
    mID           = id;
    mMinTimeout   = rule.mMinTimeout;
    mMinThreshold = rule.mMinThreshold;
    mMaxThreshold = rule.mMaxThreshold;

    LOG_DBG() << "Create alert processor" << Log::Field("id", mID) << Log::Field("minThreshold", mMinThreshold)
              << Log::Field("maxThreshold", mMaxThreshold) << Log::Field("minTimeout", mMinTimeout);

    mAlertSender   = &sender;
    mAlertTemplate = alertTemplate;

    return ErrorEnum::eNone;
}

Error AlertProcessor::CheckAlertDetection(const uint64_t currentValue, const Time& currentTime)
{
    if (!mAlertCondition) {
        return HandleMaxThreshold(currentValue, currentTime);
    } else {
        return HandleMinThreshold(currentValue, currentTime);
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error AlertProcessor::HandleMaxThreshold(uint64_t currentValue, const Time& currentTime)
{
    Error err = ErrorEnum::eNone;

    if (currentValue >= mMaxThreshold && mMaxThresholdTime.IsZero()) {
        LOG_INF() << "Max threshold crossed" << Log::Field("id", mID) << Log::Field("maxThreshold", mMaxThreshold)
                  << Log::Field("value", currentValue) << Log::Field("time", currentTime);

        mMaxThresholdTime = currentTime;
    }

    if (currentValue >= mMaxThreshold && !mMaxThresholdTime.IsZero()
        && currentTime.Sub(mMaxThresholdTime) >= mMinTimeout) {
        const QuotaAlertState state = QuotaAlertStateEnum::eRaise;

        LOG_INF() << "Resource alert" << Log::Field("id", mID) << Log::Field("value", currentValue)
                  << Log::Field("state", state) << Log::Field("time", currentTime);

        mAlertCondition   = true;
        mMaxThresholdTime = currentTime;
        mMinThresholdTime = Time();

        if (auto sendErr = SendAlert(currentValue, currentTime, state); err.IsNone() && !sendErr.IsNone()) {
            err = AOS_ERROR_WRAP(sendErr);
        }
    }

    if (currentValue < mMaxThreshold && !mMaxThresholdTime.IsZero()) {
        mMaxThresholdTime = Time();
    }

    return err;
}

Error AlertProcessor::HandleMinThreshold(uint64_t currentValue, const Time& currentTime)
{
    if (currentValue >= mMinThreshold) {
        mMinThresholdTime = Time();

        if (currentTime.Sub(mMaxThresholdTime) >= mMinTimeout) {
            const QuotaAlertState state = QuotaAlertStateEnum::eContinue;

            mMaxThresholdTime = currentTime;

            LOG_INF() << "Resource alert" << Log::Field("id", mID) << Log::Field("value", currentValue)
                      << Log::Field("state", state) << Log::Field("time", currentTime);

            if (auto err = SendAlert(currentValue, currentTime, state); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        return ErrorEnum::eNone;
    }

    if (mMinThresholdTime.IsZero()) {
        LOG_INF() << "Min threshold crossed" << Log::Field("id", mID) << Log::Field("value", currentValue)
                  << Log::Field("minThreshold", mMinThreshold) << Log::Field("time", currentTime);

        mMinThresholdTime = currentTime;

        return ErrorEnum::eNone;
    }

    if (currentTime.Sub(mMinThresholdTime) >= mMinTimeout) {
        const QuotaAlertState state = QuotaAlertStateEnum::eFall;

        LOG_INF() << "Resource alert" << Log::Field("id", mID) << Log::Field("value", currentValue)
                  << Log::Field("state", state) << Log::Field("time", currentTime);

        mAlertCondition   = false;
        mMinThresholdTime = currentTime;
        mMaxThresholdTime = Time();

        if (auto err = SendAlert(currentValue, currentTime, state); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error AlertProcessor::SendAlert(uint64_t currentValue, const Time& currentTime, QuotaAlertState state)
{
    CreateAlertVisitor visitor(currentValue, currentTime, state);

    auto alert = mAlertTemplate.ApplyVisitor(visitor);

    if (auto err = mAlertSender->SendAlert(alert); !err.IsNone()) {
        LOG_ERR() << "Failed to send alert" << Log::Field(err);

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::monitoring
