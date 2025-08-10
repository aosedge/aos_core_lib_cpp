/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>
#include <core/common/tools/semver.hpp>

#include "unitconfig.hpp"

namespace aos::cm::unitconfig {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error UnitConfig::Init(const String& cfgFile, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    unitconfig::NodeConfigControllerItf& nodeConfigController, JSONProviderItf& jsonProvider)
{
    LOG_INF() << "Init unit config";

    mCfgFile              = cfgFile;
    mNodeInfoProvider     = &nodeInfoProvider;
    mNodeConfigController = &nodeConfigController;
    mJSONProvider         = &jsonProvider;

    auto nodeInfo = MakeUnique<NodeInfo>(&mAllocator);

    mCurNodeID = nodeInfoProvider.GetCurrentNodeID();

    if (auto err = nodeInfoProvider.GetNodeInfo(mCurNodeID, *nodeInfo); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mCurNodeType = nodeInfo->mNodeType;

    return LoadConfig();
}

Error UnitConfig::GetStatus(cloudprotocol::UnitConfigStatus& status)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get status";

    status.mVersion = mUnitConfig.mVersion;

    if (!mUnitConfigError.IsNone()) {
        status.mStatus = ItemStatusEnum::eError;
        status.mError  = mUnitConfigError;

        return ErrorEnum::eNone;
    }

    status.mStatus = ItemStatusEnum::eInstalled;

    return ErrorEnum::eNone;
}

Error UnitConfig::CheckUnitConfig(const cloudprotocol::UnitConfig& config)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Check unit config: " << Log::Field("Version", config.mVersion);

    if (!mUnitConfigError.IsNone() && !mUnitConfig.mVersion.IsEmpty()) {
        LOG_WRN() << "Skip unit config version check due to error: " << mUnitConfigError;
    } else if (auto err = CheckVersion(config.mVersion); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto nodeConfigStatuses = MakeUnique<StaticArray<NodeConfigStatus, cMaxNumNodes>>(&mAllocator);

    if (auto err = mNodeConfigController->GetNodeConfigStatuses(*nodeConfigStatuses); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& nodeConfigStatus : *nodeConfigStatuses) {
        if (nodeConfigStatus.mVersion != config.mVersion || !nodeConfigStatus.mError.IsNone()) {
            auto nodeConfig = MakeUnique<cloudprotocol::NodeConfig>(&mAllocator);

            if (auto err = FindNodeConfig(nodeConfigStatus.mNodeID, nodeConfigStatus.mNodeType, config, *nodeConfig);
                !err.IsNone()) {
                LOG_WRN() << "Error finding node config: " << err;
            }

            nodeConfig->mNodeID = nodeConfigStatus.mNodeID;

            if (auto err
                = mNodeConfigController->CheckNodeConfig(nodeConfigStatus.mNodeID, config.mVersion, *nodeConfig);
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error UnitConfig::GetNodeConfig(const String& nodeID, const String& nodeType, cloudprotocol::NodeConfig& config)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get node config: " << Log::Field("NodeID", nodeID) << Log::Field("NodeType", nodeType);

    return FindNodeConfig(nodeID, nodeType, mUnitConfig, config);
}

Error UnitConfig::GetCurrentNodeConfig(cloudprotocol::NodeConfig& config)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get current node config";

    return GetNodeConfig(mCurNodeID, mCurNodeType, config);
}

Error UnitConfig::UpdateUnitConfig(const cloudprotocol::UnitConfig& unitConfig)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Update unit config: " << Log::Field("Version", unitConfig.mVersion);

    if (mUnitConfigError.IsNone() && mUnitConfig.mVersion.IsEmpty()) {
        LOG_WRN() << "Skip unit config version check due to error: " << mUnitConfigError;
    } else if (auto err = CheckVersion(unitConfig.mVersion); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mUnitConfig = unitConfig;

    auto nodeConfigStatuses = MakeUnique<StaticArray<NodeConfigStatus, cMaxNumNodes>>(&mAllocator);

    if (auto err = mNodeConfigController->GetNodeConfigStatuses(*nodeConfigStatuses); !err.IsNone()) {
        LOG_ERR() << "Error getting node config statuses: " << err;
    }

    for (const auto& nodeConfigStatus : *nodeConfigStatuses) {
        if (nodeConfigStatus.mVersion != unitConfig.mVersion || !nodeConfigStatus.mError.IsNone()) {
            auto nodeConfig = MakeUnique<cloudprotocol::NodeConfig>(&mAllocator);

            if (auto err
                = FindNodeConfig(nodeConfigStatus.mNodeID, nodeConfigStatus.mNodeType, unitConfig, *nodeConfig);
                !err.IsNone()) {
                LOG_WRN() << "Error finding node config: " << err;
            }

            if (auto err
                = mNodeConfigController->SetNodeConfig(nodeConfigStatus.mNodeID, unitConfig.mVersion, *nodeConfig);
                !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            if (nodeConfigStatus.mNodeID == mCurNodeID) {
                UpdateNodeConfigListeners(*nodeConfig);
            }
        }
    }

    auto unitConfigJSON = MakeUnique<StaticString<cUnitConfigJSONLen>>(&mAllocator);

    if (auto err = mJSONProvider->UnitConfigToJSON(unitConfig, *unitConfigJSON); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::WriteStringToFile(mCfgFile, *unitConfigJSON, 0600); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UnitConfig::SubscribeCurrentNodeConfigChange(NodeConfigChangeListenerItf* listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe current node config change";

    return mNodeConfigListeners.PushBack(listener);
}

void UnitConfig::UnsubscribeCurrentNodeConfigChange(NodeConfigChangeListenerItf* listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unsubscribe current node config change";

    mNodeConfigListeners.Remove(listener);
}

void UnitConfig::OnNodeConfigStatus(const NodeConfigStatus& status)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Node config status: " << Log::Field("NodeID", status.mNodeID)
              << Log::Field("NodeType", status.mNodeType) << Log::Field("Version", status.mVersion)
              << Log::Field("Error", status.mError);

    if (!mUnitConfigError.IsNone()) {
        LOG_WRN() << "Can't update node config" << Log::Field("NodeID", status.mNodeID) << Log::Field(mUnitConfigError);
    }

    if (status.mVersion == mUnitConfig.mVersion && status.mError.IsNone()) {
        return;
    }

    auto nodeConfig = MakeUnique<cloudprotocol::NodeConfig>(&mAllocator);

    if (auto err = FindNodeConfig(status.mNodeID, status.mNodeType, mUnitConfig, *nodeConfig); !err.IsNone()) {
        LOG_WRN() << "Error finding node config: " << err;
    }

    if (auto err = mNodeConfigController->SetNodeConfig(status.mNodeID, mUnitConfig.mVersion, *nodeConfig);
        !err.IsNone()) {
        LOG_WRN() << "Error setting node config: " << err;
    }

    if (status.mNodeID == mCurNodeID) {
        UpdateNodeConfigListeners(*nodeConfig);
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error UnitConfig::LoadConfig()
{
    LOG_DBG() << "Load config";

    auto unitConfig = MakeUnique<StaticString<cUnitConfigJSONLen>>(&mAllocator);

    auto err = fs::ReadFileToString(mCfgFile, *unitConfig);
    if (!err.IsNone()) {
        if (err == ENOENT) {
            mUnitConfig.mVersion = "0.0.0";

            return ErrorEnum::eNone;
        }

        mUnitConfigError = err;

        return AOS_ERROR_WRAP(err);
    }

    err = mJSONProvider->UnitConfigFromJSON(*unitConfig, mUnitConfig);
    if (!err.IsNone()) {
        mUnitConfig.mVersion = "0.0.0";
        mUnitConfigError     = err;

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error UnitConfig::CheckVersion(const String& version)
{
    LOG_DBG() << "Check version: " << Log::Field("mUnitConfig.mVersion", mUnitConfig.mVersion)
              << Log::Field("version", version);

    auto [result, err] = semver::CompareSemver(version, mUnitConfig.mVersion);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (result == 0) {
        return ErrorEnum::eAlreadyExist;
    }

    if (result < 0) {
        return Error(ErrorEnum::eWrongState, "Wrong version");
    }

    return ErrorEnum::eNone;
}

Error UnitConfig::FindNodeConfig(const String& nodeID, const String& nodeType, const cloudprotocol::UnitConfig& config,
    cloudprotocol::NodeConfig& nodeConfig)
{
    auto node = config.mNodes.FindIf([&](const cloudprotocol::NodeConfig& node) { return node.mNodeID == nodeID; });
    if (node != config.mNodes.end()) {
        nodeConfig = *node;

        return ErrorEnum::eNone;
    }

    node = config.mNodes.FindIf([&](const cloudprotocol::NodeConfig& node) { return node.mNodeType == nodeType; });
    if (node != config.mNodes.end()) {
        nodeConfig = *node;

        return ErrorEnum::eNone;
    }

    return ErrorEnum::eNotFound;
}

void UnitConfig::UpdateNodeConfigListeners(const cloudprotocol::NodeConfig& nodeConfig)
{
    for (auto listener : mNodeConfigListeners) {
        listener->OnNodeConfigChange(nodeConfig);
    }
}

} // namespace aos::cm::unitconfig
