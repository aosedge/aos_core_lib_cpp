/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "updatemanager.hpp"

namespace aos::cm::updatemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error UpdateManager::Init(const Config& config, iamclient::IdentProviderItf& identProvider,
    iamclient::NodeHandlerItf& nodeHandler, unitconfig::UnitConfigItf& unitConfig,
    nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider, imagemanager::ImageManagerItf& imageManager,
    launcher::LauncherItf& launcher, cloudconnection::CloudConnectionItf& cloudConnection, SenderItf& sender,
    StorageItf& storage)
{
    LOG_DBG() << "Init update manager";

    if (auto err
        = mDesiredStatusHandler.Init(nodeHandler, unitConfig, imageManager, launcher, mUnitStatusHandler, storage);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mUnitStatusHandler.Init(
            config, identProvider, unitConfig, nodeInfoProvider, imageManager, launcher, cloudConnection, sender);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UpdateManager::Start()
{
    LOG_DBG() << "Start update manager";

    if (auto err = mDesiredStatusHandler.Start(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mUnitStatusHandler.Start(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UpdateManager::Stop()
{
    Error err;

    if (auto desiredStatusErr = mDesiredStatusHandler.Stop(); !desiredStatusErr.IsNone() && err.IsNone()) {
        err = AOS_ERROR_WRAP(desiredStatusErr);
    }

    if (auto unitStatusErr = mUnitStatusHandler.Stop(); !unitStatusErr.IsNone() && err.IsNone()) {
        err = AOS_ERROR_WRAP(unitStatusErr);
    }

    return err;
}

Error UpdateManager::ProcessDesiredStatus(const DesiredStatus& desiredStatus)
{
    if (auto err = mDesiredStatusHandler.ProcessDesiredStatus(desiredStatus); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::cm::updatemanager
