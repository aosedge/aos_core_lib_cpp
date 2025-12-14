/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_DESIREDSTATUSHANDLER_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_DESIREDSTATUSHANDLER_HPP_

#include <core/cm/launcher/itf/launcher.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/desiredstatus.hpp>

#include "unitstatushandler.hpp"

namespace aos::cm::updatemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Update state.
 */
class UpdateStateType {
public:
    enum class Enum {
        eNone,
        eDownloading,
        ePending,
        eInstalling,
        eLaunching,
        eFinalizing,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "none",
            "downloading",
            "pending",
            "installing",
            "launching",
            "finalizing",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using UpdateStateEnum = UpdateStateType::Enum;
using UpdateState     = EnumStringer<UpdateStateType>;

/**
 * Desired status handler.
 */
class DesiredStatusHandler {
public:
    /**
     * Initializes desired status handler.
     *
     * @param unitStatusHandler unit status handler.
     * @return Error.
     */
    Error Init(UnitStatusHandler& unitStatusHandler);

    /**
     * Starts desired status handler.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops desired status handler.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Processes desired status.
     *
     * @param desiredStatus desired status.
     * @return Error.
     */
    Error ProcessDesiredStatus(const DesiredStatus& desiredStatus);

private:
    void Run();
    void LogDesiredStatus(const DesiredStatus& desiredStatus);
    void SetState(UpdateState state);

    UnitStatusHandler* mUnitStatusHandler {};

    Mutex               mMutex;
    ConditionalVariable mCondVar;
    Thread<>            mThread;
    DesiredStatus       mCurrentDesiredStatus;
    DesiredStatus       mPendingDesiredStatus;

    bool        mIsRunning {};
    bool        mHasPendingDesiredStatus {};
    UpdateState mUpdateState {};
};

/** @}*/

} // namespace aos::cm::updatemanager

#endif
