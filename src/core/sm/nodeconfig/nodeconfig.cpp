/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/fs.hpp>
#include <core/common/tools/logger.hpp>
#include <core/common/tools/semver.hpp>

#include "nodeconfig.hpp"

namespace aos::sm::nodeconfig {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error NodeConfig::Init(const Config& config, aos::nodeconfig::JSONProviderItf& jsonProvider)
{
    LOG_DBG() << "Init node config";

    mNodeConfigFile = config.mNodeConfigFile;
    mJSONProvider   = &jsonProvider;

    if (auto err = LoadConfig(); !err.IsNone()) {
        LOG_ERR() << "Failed to load config" << Log::Field(err);
    }

    return ErrorEnum::eNone;
}

Error NodeConfig::CheckNodeConfig(const aos::NodeConfig& config)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Check node config" << Log::Field("version", config.mVersion);

    if (mNodeConfigState == UnitConfigStateEnum::eFailed) {
        return AOS_ERROR_WRAP(mNodeConfigError);
    }

    if (mNodeConfigState == UnitConfigStateEnum::eInstalled) {
        if (auto err = CheckVersion(config.mVersion); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NodeConfig::UpdateNodeConfig(const aos::NodeConfig& config)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Update node config" << Log::Field("version", config.mVersion);

    if (mNodeConfigState != UnitConfigStateEnum::eInstalled && mNodeConfigState != UnitConfigStateEnum::eAbsent) {
        LOG_WRN() << "Skip node config version check due to state" << Log::Field("state", mNodeConfigState)
                  << Log::Field(mNodeConfigError);
    } else if (mNodeConfigState == UnitConfigStateEnum::eInstalled) {
        if (auto err = CheckVersion(config.mVersion); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    mNodeConfig = config;

    auto nodeConfigJSON = MakeUnique<StaticString<aos::nodeconfig::cNodeConfigJSONLen>>(&mAllocator);

    if (auto err = mJSONProvider->NodeConfigToJSON(config, *nodeConfigJSON); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::WriteStringToFile(mNodeConfigFile, *nodeConfigJSON, 0600); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mNodeConfigState = UnitConfigStateEnum::eInstalled;
    mNodeConfigError = ErrorEnum::eNone;

    for (auto* listener : mListeners) {
        if (auto err = listener->OnNodeConfigChanged(mNodeConfig); !err.IsNone()) {
            LOG_ERR() << "Failed to notify listener" << Log::Field(err);
        }
    }

    return ErrorEnum::eNone;
}

Error NodeConfig::GetNodeConfigStatus(NodeConfigStatus& status)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get node config status";

    status.mVersion = mNodeConfig.mVersion;
    status.mState   = mNodeConfigState;

    if (mNodeConfigState == UnitConfigStateEnum::eFailed) {
        status.mError = mNodeConfigError;
    }

    return ErrorEnum::eNone;
}

Error NodeConfig::GetNodeConfig(aos::NodeConfig& nodeConfig)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get node config";

    nodeConfig = mNodeConfig;

    return ErrorEnum::eNone;
}

Error NodeConfig::SubscribeListener(aos::nodeconfig::NodeConfigListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe listener";

    if (auto err = mListeners.PushBack(&listener); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error NodeConfig::UnsubscribeListener(aos::nodeconfig::NodeConfigListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unsubscribe listener";

    return mListeners.Remove(&listener) != 0 ? ErrorEnum::eNone : ErrorEnum::eNotFound;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error NodeConfig::LoadConfig()
{
    LOG_DBG() << "Load config";

    auto nodeConfigJSON = MakeUnique<StaticString<aos::nodeconfig::cNodeConfigJSONLen>>(&mAllocator);

    auto err = fs::ReadFileToString(mNodeConfigFile, *nodeConfigJSON);
    if (!err.IsNone()) {
        if (err == ENOENT) {
            mNodeConfigState = UnitConfigStateEnum::eAbsent;

            return ErrorEnum::eNone;
        }

        mNodeConfigError = err;
        mNodeConfigState = UnitConfigStateEnum::eFailed;

        return AOS_ERROR_WRAP(err);
    }

    err = mJSONProvider->NodeConfigFromJSON(*nodeConfigJSON, mNodeConfig);
    if (!err.IsNone()) {
        mNodeConfigError = err;
        mNodeConfigState = UnitConfigStateEnum::eFailed;

        return AOS_ERROR_WRAP(err);
    }

    mNodeConfigState = UnitConfigStateEnum::eInstalled;

    return ErrorEnum::eNone;
}

Error NodeConfig::CheckVersion(const String& version)
{
    LOG_DBG() << "Check version" << Log::Field("version", mNodeConfig.mVersion) << Log::Field("newVersion", version);

    auto [result, err] = semver::CompareSemver(version, mNodeConfig.mVersion);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (result == 0) {
        return ErrorEnum::eAlreadyExist;
    }

    if (result < 0) {
        return Error(ErrorEnum::eWrongState, "wrong version");
    }

    return ErrorEnum::eNone;
}

} // namespace aos::sm::nodeconfig
