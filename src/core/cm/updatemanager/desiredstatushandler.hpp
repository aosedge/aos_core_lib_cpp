/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_DESIREDSTATUSHANDLER_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_DESIREDSTATUSHANDLER_HPP_

#include <core/cm/imagemanager/itf/imagemanager.hpp>
#include <core/cm/launcher/itf/launcher.hpp>
#include <core/common/iamclient/itf/nodehandler.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/types/desiredstatus.hpp>

#include "itf/storage.hpp"
#include "unitstatushandler.hpp"

namespace aos::cm::updatemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Desired status handler.
 */
class DesiredStatusHandler {
public:
    /**
     * Initializes desired status handler.
     *
     * @param nodeHandler node handler.
     * @param unitConfig unit config interface.
     * @param imageManager image manager.
     * @param launcher launcher interface.
     * @param unitStatusHandler unit status handler.
     * @param storage storage interface.
     * @return Error.
     */
    Error Init(iamclient::NodeHandlerItf& nodeHandler, unitconfig::UnitConfigItf& unitConfig,
        imagemanager::ImageManagerItf& imageManager, launcher::LauncherItf& launcher,
        UnitStatusHandler& unitStatusHandler, StorageItf& storage);

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
    static constexpr auto cAllocatorSize = Max(sizeof(StaticArray<UpdateItemStatus, cMaxNumUpdateItems>),
        sizeof(StaticArray<launcher::RunInstanceRequest, cMaxNumInstances>)
            + sizeof(StaticArray<InstanceStatus, cMaxNumInstances>));

    void  Run();
    void  LogDesiredStatus(const DesiredStatus& desiredStatus);
    void  SetState(UpdateState state);
    Error DownloadUpdateItems();
    Error InstallDesiredStatus();
    Error LaunchInstances();
    Error FinalizeUpdate();
    void  StartUpdate(UpdateState state = UpdateStateEnum::eDownloading);
    void  CancelUpdate();

    iamclient::NodeHandlerItf*     mNodeHandler {};
    unitconfig::UnitConfigItf*     mUnitConfig {};
    imagemanager::ImageManagerItf* mImageManager {};
    launcher::LauncherItf*         mLauncher {};
    UnitStatusHandler*             mUnitStatusHandler {};
    StorageItf*                    mStorage {};

    Mutex               mMutex;
    ConditionalVariable mCondVar;
    Thread<>            mThread;
    DesiredStatus       mCurrentDesiredStatus;
    DesiredStatus       mPendingDesiredStatus;

    bool        mIsRunning {};
    bool        mHasPendingDesiredStatus {};
    bool        mCancelCurrentUpdate {};
    UpdateState mUpdateState {};

    StaticAllocator<cAllocatorSize> mAllocator {};
};

/** @}*/

} // namespace aos::cm::updatemanager

#endif
