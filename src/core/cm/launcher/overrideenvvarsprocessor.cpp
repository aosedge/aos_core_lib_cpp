/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "overrideenvvarsprocessor.hpp"

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error OverrideEnvVarsProcessor::Init(
    const Config& config, StorageItf& storage, SenderItf& envVarStatusSender, OverrideEnvVarsListenerItf& listener)
{
    LOG_DBG() << "Init override env vars processor";

    mCheckPeriod        = config.mCheckOverrideEnvVarsPeriod;
    mStorage            = &storage;
    mEnvVarStatusSender = &envVarStatusSender;
    mListener           = &listener;

    return ErrorEnum::eNone;
}

RetWithError<bool> OverrideEnvVarsProcessor::Start()
{
    LOG_DBG() << "Start override env vars processor";

    bool  changed = false;
    Error err;

    {
        LockGuard lock {mMutex};

        if (auto loadErr = mStorage->LoadOverrideEnvVars(mOverrideEnvVars); !loadErr.IsNone()) {
            return {false, AOS_ERROR_WRAP(loadErr)};
        }

        // Drop variables that expired while offline.
        Tie(changed, err) = ProcessOverrideEnvVars(mOverrideEnvVars);
        if (!err.IsNone()) {
            return {changed, err};
        }
    }

    if (auto timerErr = mTimer.Start(
            mCheckPeriod, [this](void*) { OnTTLTimerTick(); }, false);
        !timerErr.IsNone()) {
        return {changed, AOS_ERROR_WRAP(timerErr)};
    }

    return {changed, ErrorEnum::eNone};
}

Error OverrideEnvVarsProcessor::Stop()
{
    LOG_DBG() << "Stop override env vars processor";

    if (auto err = mTimer.Stop(Timer::StopMode::WaitForCallbacks); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<bool> OverrideEnvVarsProcessor::OverrideEnvVars(const OverrideEnvVarsRequest& envVars)
{
    bool  changed = false;
    Error err;

    {
        LockGuard lock {mMutex};

        Tie(changed, err) = ProcessOverrideEnvVars(envVars);
    }

    // Notify outside mMutex to avoid lock order inversion.
    if (changed && err.IsNone()) {
        mListener->OnOverrideEnvVarsChanged();
    }

    return {changed, err};
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void OverrideEnvVarsProcessor::OnTTLTimerTick()
{
    if (auto [_, err] = OverrideEnvVars(mOverrideEnvVars); !err.IsNone()) {
        LOG_ERR() << "Update override env vars failed" << Log::Field(err);
    }
}

RetWithError<bool> OverrideEnvVarsProcessor::ProcessOverrideEnvVars(const OverrideEnvVarsRequest& envVars)
{
    auto now     = Time::Now();
    bool changed = mOverrideEnvVars.mItems != envVars.mItems || HasExpiredVariables(envVars, now);

    LOG_DBG() << "Process override env vars" << Log::Field("changed", changed)
              << Log::Field("count", envVars.mItems.Size());

    if (changed) {
        // envVars may alias mOverrideEnvVars on TTL recheck.
        if (&mOverrideEnvVars != &envVars) {
            mOverrideEnvVars = envVars;
        }

        RemoveExpiredVariables(mOverrideEnvVars, now);

        if (auto err = mStorage->SaveOverrideEnvVars(mOverrideEnvVars); !err.IsNone()) {
            return {changed, AOS_ERROR_WRAP(err)};
        }
    }

    return {changed, ErrorEnum::eNone};
}

bool OverrideEnvVarsProcessor::HasExpiredVariables(const OverrideEnvVarsRequest& envVars, const Time& now)
{
    for (const auto& item : envVars.mItems) {
        for (const auto& envVar : item.mVariables) {
            if (envVar.mTTL.HasValue() && envVar.mTTL.GetValue() < now) {
                return true;
            }
        }
    }

    return false;
}

void OverrideEnvVarsProcessor::RemoveExpiredVariables(OverrideEnvVarsRequest& envVars, const Time& now)
{
    for (auto& item : envVars.mItems) {
        item.mVariables.RemoveIf([&now](const EnvVarInfo& envVarInfo) {
            return envVarInfo.mTTL.HasValue() && envVarInfo.mTTL.GetValue() < now;
        });
    }

    envVars.mItems.RemoveIf([](const EnvVarsInstanceInfo& item) { return item.mVariables.IsEmpty(); });
}

} // namespace aos::cm::launcher
