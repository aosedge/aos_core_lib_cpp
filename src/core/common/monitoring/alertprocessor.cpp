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

class CreateAlertVisitor : public StaticVisitor<void> {
public:
    CreateAlertVisitor(
        const ResourceIdentifier& id, uint64_t currentValue, const Time& currentTime, const QuotaAlertState& state)
        : mID(id)
        , mCurrentVal(currentValue)
        , mCurrentTime(currentTime)
        , mState(state)
    {
    }

    Res Visit(SystemQuotaAlert& val) const
    {
        val.mNodeID    = mID.mNodeID;
        val.mParameter = GetParameterName(mID);
        val.mTimestamp = mCurrentTime;
        val.mValue     = mCurrentVal;
        val.mState     = mState;
    }

    Res Visit(InstanceQuotaAlert& val) const
    {
        val.mParameter                   = GetParameterName(mID);
        static_cast<InstanceIdent&>(val) = mID.mInstanceIdent.GetValue();
        val.mTimestamp                   = mCurrentTime;
        val.mValue                       = mCurrentVal;
        val.mState                       = mState;
    }

    template <typename T>
    Res Visit(const T&) const
    {
        assert(false);
    }

private:
    String GetParameterName(const ResourceIdentifier& id) const
    {
        if (id.mPartitionName.HasValue()) {
            return id.mPartitionName.GetValue(); // NOSONAR cpp:S5912 - String is used as a string view.
        }

        return id.mType.ToString(); // NOSONAR cpp:S5912 - String is used as a string view.
    }

    const ResourceIdentifier& mID;
    uint64_t                  mCurrentVal {};
    Time                      mCurrentTime;
    QuotaAlertState           mState;
};

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error AlertProcessor::Init(const ResourceIdentifier& id, const AlertRulePoints& rule, alerts::SenderItf& sender)
{
    mID           = id;
    mMinTimeout   = rule.mMinTimeout;
    mMinThreshold = rule.mMinThreshold;
    mMaxThreshold = rule.mMaxThreshold;

    LOG_DBG() << "Create alert processor" << Log::Field("id", mID) << Log::Field("minThreshold", mMinThreshold)
              << Log::Field("maxThreshold", mMaxThreshold) << Log::Field("minTimeout", mMinTimeout);

    mAlertSender = &sender;

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

Error AlertProcessor::SendAlert(uint64_t currentValue, const Time& currentTime, const QuotaAlertState& state)
{
    AlertVariant alert;

    if (mID.mLevel == ResourceLevelEnum::eSystem) {
        alert.SetValue<SystemQuotaAlert>();
    } else if (mID.mLevel == ResourceLevelEnum::eInstance) {
        alert.SetValue<InstanceQuotaAlert>();
    } else {
        return Error(ErrorEnum::eInvalidArgument);
    }

    const CreateAlertVisitor visitor(mID, currentValue, currentTime, state);

    alert.ApplyVisitor(visitor);

    if (auto err = mAlertSender->SendAlert(alert); !err.IsNone()) {
        LOG_ERR() << "Failed to send alert" << Log::Field(err);

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::monitoring
