/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/monitoring/alertprocessor.hpp"

#include "log.hpp"

namespace aos::monitoring {

namespace {

class CreateAlertVisitor : public StaticVisitor<cloudprotocol::AlertVariant> {
public:
    CreateAlertVisitor(uint64_t currentValue, const Time& currentTime, cloudprotocol::AlertStatus status)
        : mCurrentVal(currentValue)
        , mCurrentTime(currentTime)
        , mStatus(status)
    {
    }

    Res Visit(const cloudprotocol::SystemQuotaAlert& val) const
    {
        auto systemQuotaAlert = val;

        systemQuotaAlert.mTimestamp = mCurrentTime;
        systemQuotaAlert.mValue     = mCurrentVal;
        systemQuotaAlert.mStatus    = mStatus;

        Res result;
        result.SetValue<cloudprotocol::SystemQuotaAlert>(systemQuotaAlert);

        return result;
    }

    Res Visit(const cloudprotocol::InstanceQuotaAlert& val) const
    {
        auto instanceQuotaAlert = val;

        instanceQuotaAlert.mTimestamp = mCurrentTime;
        instanceQuotaAlert.mValue     = mCurrentVal;
        instanceQuotaAlert.mStatus    = mStatus;

        Res result;
        result.SetValue<cloudprotocol::InstanceQuotaAlert>(instanceQuotaAlert);

        return result;
    }

    template <typename T>
    Res Visit(const T&) const
    {
        assert(false);

        return {};
    }

private:
    uint64_t                   mCurrentVal;
    Time                       mCurrentTime;
    cloudprotocol::AlertStatus mStatus;
};

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error AlertProcessor::Init(ResourceIdentifier id, uint64_t maxValue, const AlertRulePercents& rule,
    alerts::SenderItf& sender, const cloudprotocol::AlertVariant& alertTemplate)
{
    mID           = id;
    mMinTimeout   = rule.mMinTimeout;
    mMinThreshold = static_cast<uint64_t>(static_cast<double>(maxValue) * rule.mMinThreshold / 100.0);
    mMaxThreshold = static_cast<uint64_t>(static_cast<double>(maxValue) * rule.mMaxThreshold / 100.0);

    LOG_DBG() << "Create alert processor: id=" << mID << ", minThreshold=" << mMinThreshold
              << ", maxThreshold=" << mMaxThreshold << ", minTimeout=" << mMinTimeout;

    mAlertSender   = &sender;
    mAlertTemplate = alertTemplate;

    return ErrorEnum::eNone;
}

Error AlertProcessor::Init(ResourceIdentifier id, const AlertRulePoints& rule, alerts::SenderItf& sender,
    const cloudprotocol::AlertVariant& alertTemplate)
{
    mID           = id;
    mMinTimeout   = rule.mMinTimeout;
    mMinThreshold = rule.mMinThreshold;
    mMaxThreshold = rule.mMaxThreshold;

    LOG_DBG() << "Create alert processor: id=" << mID << ", minThreshold=" << mMinThreshold
              << ", maxThreshold=" << mMaxThreshold << ", minTimeout=" << mMinTimeout;

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
        LOG_INF() << "Max threshold crossed: id=" << mID << ", maxThreshold=" << mMaxThreshold
                  << ", value=" << currentValue << ", time=" << currentTime;

        mMaxThresholdTime = currentTime;
    }

    if (currentValue >= mMaxThreshold && !mMaxThresholdTime.IsZero()
        && currentTime.Sub(mMaxThresholdTime) >= mMinTimeout) {
        const cloudprotocol::AlertStatus status = cloudprotocol::AlertStatusEnum::eRaise;

        LOG_INF() << "Resource alert: id=" << mID << ", value=" << currentValue << ", status=" << status
                  << ", time=" << currentTime;

        mAlertCondition   = true;
        mMaxThresholdTime = currentTime;
        mMinThresholdTime = Time();

        if (auto sendErr = SendAlert(currentValue, currentTime, status); err.IsNone() && !sendErr.IsNone()) {
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
            const cloudprotocol::AlertStatus status = cloudprotocol::AlertStatusEnum::eContinue;

            mMaxThresholdTime = currentTime;

            LOG_INF() << "Resource alert: id=" << mID << ", value=" << currentValue << ", status=" << status
                      << ", time=" << currentTime;

            if (auto err = SendAlert(currentValue, currentTime, status); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        return ErrorEnum::eNone;
    }

    if (mMinThresholdTime.IsZero()) {
        LOG_INF() << "Min threshold crossed: id=" << mID << ", value=" << currentValue
                  << ", minThreshold=" << mMinThreshold << ", time=" << currentTime;

        mMinThresholdTime = currentTime;

        return ErrorEnum::eNone;
    }

    if (currentTime.Sub(mMinThresholdTime) >= mMinTimeout) {
        const cloudprotocol::AlertStatus status = cloudprotocol::AlertStatusEnum::eFall;

        LOG_INF() << "Resource alert: id=" << mID << ", value=" << currentValue << ", status=" << status
                  << ", time=" << currentTime;

        mAlertCondition   = false;
        mMinThresholdTime = currentTime;
        mMaxThresholdTime = Time();

        if (auto err = SendAlert(currentValue, currentTime, status); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error AlertProcessor::SendAlert(uint64_t currentValue, const Time& currentTime, cloudprotocol::AlertStatus status)
{
    CreateAlertVisitor visitor(currentValue, currentTime, status);

    auto alert = mAlertTemplate.ApplyVisitor(visitor);

    if (auto err = mAlertSender->SendAlert(alert); !err.IsNone()) {
        LOG_ERR() << "Failed to send alert: err=" << err;

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::monitoring
