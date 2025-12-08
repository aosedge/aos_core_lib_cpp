/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/fs.hpp>
#include <core/common/tools/logger.hpp>
#include <core/common/tools/semver.hpp>

#include "unitconfig.hpp"

namespace aos::cm::unitconfig {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error UnitConfig::Init(const Config& config, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    NodeConfigHandlerItf& nodeConfigHandler, JSONProviderItf& jsonProvider)
{
    LOG_DBG() << "Init unit config";

    mUnitConfigFile    = config.mUnitConfigFile;
    mNodeInfoProvider  = &nodeInfoProvider;
    mNodeConfigHandler = &nodeConfigHandler;
    mJSONProvider      = &jsonProvider;

    if (auto err = LoadConfig(); !err.IsNone()) {
        LOG_ERR() << "Failed to load config" << Log::Field(err);
    }

    return ErrorEnum::eNone;
}

Error UnitConfig::GetUnitConfigStatus(UnitConfigStatus& status)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get unit config status";

    status.mVersion = mUnitConfig.mVersion;
    status.mState   = mUnitConfigState;

    if (mUnitConfigState == UnitConfigStateEnum::eFailed) {
        status.mError = mUnitConfigError;
    }

    return ErrorEnum::eNone;
}

Error UnitConfig::CheckUnitConfig(const aos::UnitConfig& config)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Check unit config" << Log::Field("version", config.mVersion);

    if (mUnitConfigState != UnitConfigStateEnum::eInstalled) {
        LOG_WRN() << "Skip unit config version check due to state" << Log::Field("state", mUnitConfigState)
                  << Log::Field(mUnitConfigError);
    } else if (auto err = CheckVersion(config.mVersion); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIds;

    if (auto err = mNodeInfoProvider->GetAllNodeIDs(nodeIds); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& id : nodeIds) {
        NodeConfigStatus nodeConfigStatus;

        if (auto err = mNodeConfigHandler->GetNodeConfigStatus(id, nodeConfigStatus); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (nodeConfigStatus.mVersion != config.mVersion || !nodeConfigStatus.mError.IsNone()) {
            auto nodeConfig = MakeUnique<NodeConfig>(&mAllocator);

            if (auto err = FindNodeConfig(id, "", config, *nodeConfig); !err.IsNone()) {
                return err;
            }

            if (auto err = mNodeConfigHandler->CheckNodeConfig(id, *nodeConfig); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error UnitConfig::GetNodeConfig(const String& nodeID, const String& nodeType, NodeConfig& config)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get node config" << Log::Field("nodeID", nodeID) << Log::Field("nodeType", nodeType);

    return FindNodeConfig(nodeID, nodeType, mUnitConfig, config);
}

Error UnitConfig::UpdateUnitConfig(const aos::UnitConfig& unitConfig)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Update unit config" << Log::Field("version", unitConfig.mVersion);

    if (mUnitConfigState != UnitConfigStateEnum::eInstalled && mUnitConfigState != UnitConfigStateEnum::eAbsent) {
        LOG_WRN() << "Skip unit config version check due to state" << Log::Field("state", mUnitConfigState)
                  << Log::Field(mUnitConfigError);
    } else if (mUnitConfigState == UnitConfigStateEnum::eInstalled) {
        if (auto err = CheckVersion(unitConfig.mVersion); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    mUnitConfig = unitConfig;

    auto unitConfigJSON = MakeUnique<StaticString<cUnitConfigJSONLen>>(&mAllocator);

    if (auto err = mJSONProvider->UnitConfigToJSON(unitConfig, *unitConfigJSON); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::WriteStringToFile(mUnitConfigFile, *unitConfigJSON, 0600); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mUnitConfigState = UnitConfigStateEnum::eInstalled;
    mUnitConfigError = ErrorEnum::eNone;

    StaticArray<StaticString<cIDLen>, cMaxNumNodes> nodeIds;

    if (auto err = mNodeInfoProvider->GetAllNodeIDs(nodeIds); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& id : nodeIds) {
        auto nodeConfig = MakeUnique<NodeConfig>(&mAllocator);

        if (auto err = FindNodeConfig(id, "", unitConfig, *nodeConfig); !err.IsNone()) {
            return err;
        }

        if (auto err = mNodeConfigHandler->UpdateNodeConfig(id, *nodeConfig); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

void UnitConfig::OnNodeInfoChanged(const UnitNodeInfo& info)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Node info received" << Log::Field("nodeID", info.mNodeID) << Log::Field("nodeType", info.mNodeType)
              << Log::Field("state", info.mState) << Log::Field("isConnected", info.mIsConnected)
              << Log::Field(info.mError);

    if (mUnitConfigState != UnitConfigStateEnum::eInstalled) {
        LOG_WRN() << "Can't update node config due to state" << Log::Field("nodeID", info.mNodeID)
                  << Log::Field("state", mUnitConfigState) << Log::Field(mUnitConfigError);

        return;
    }

    NodeConfigStatus nodeConfigStatus;

    if (auto err = mNodeConfigHandler->GetNodeConfigStatus(info.mNodeID, nodeConfigStatus); !err.IsNone()) {
        LOG_ERR() << "Can't get node config status" << Log::Field("nodeID", info.mNodeID) << Log::Field(err);

        return;
    }

    if (nodeConfigStatus.mVersion == mUnitConfig.mVersion && nodeConfigStatus.mError.IsNone()) {
        return;
    }

    auto nodeConfig = MakeUnique<NodeConfig>(&mAllocator);

    if (auto err = FindNodeConfig(info.mNodeID, info.mNodeType, mUnitConfig, *nodeConfig); !err.IsNone()) {
        LOG_ERR() << "Error finding node config" << Log::Field(err);

        return;
    }

    if (auto err = mNodeConfigHandler->UpdateNodeConfig(info.mNodeID, *nodeConfig); !err.IsNone()) {
        LOG_ERR() << "Error updating node config" << Log::Field(err);

        return;
    }
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error UnitConfig::LoadConfig()
{
    LOG_DBG() << "Load config";

    auto unitConfig = MakeUnique<StaticString<cUnitConfigJSONLen>>(&mAllocator);

    auto err = fs::ReadFileToString(mUnitConfigFile, *unitConfig);
    if (!err.IsNone()) {
        if (err == ENOENT) {
            mUnitConfigState = UnitConfigStateEnum::eAbsent;

            return ErrorEnum::eNone;
        }

        mUnitConfigError = err;
        mUnitConfigState = UnitConfigStateEnum::eFailed;

        return AOS_ERROR_WRAP(err);
    }

    err = mJSONProvider->UnitConfigFromJSON(*unitConfig, mUnitConfig);
    if (!err.IsNone()) {
        mUnitConfigError = err;
        mUnitConfigState = UnitConfigStateEnum::eFailed;

        return AOS_ERROR_WRAP(err);
    }

    mUnitConfigState = UnitConfigStateEnum::eInstalled;

    return ErrorEnum::eNone;
}

Error UnitConfig::CheckVersion(const String& version)
{
    LOG_DBG() << "Check version" << Log::Field("version", mUnitConfig.mVersion) << Log::Field("newVersion", version);

    auto [result, err] = semver::CompareSemver(version, mUnitConfig.mVersion);
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

Error UnitConfig::FindNodeConfig(
    const String& nodeID, const String& nodeType, const aos::UnitConfig& config, NodeConfig& nodeConfig)
{
    if (mUnitConfigState == UnitConfigStateEnum::eFailed) {
        return AOS_ERROR_WRAP(mUnitConfigError);
    }

    auto node = config.mNodes.FindIf([&](const NodeConfig& node) { return node.mNodeID == nodeID; });
    if (node != config.mNodes.end()) {
        nodeConfig = *node;
    } else {
        node = config.mNodes.FindIf([&](const NodeConfig& node) { return node.mNodeType == nodeType; });
        if (node != config.mNodes.end()) {
            nodeConfig = *node;
        } else {
            nodeConfig = NodeConfig {};
        }
    }

    nodeConfig.mVersion = config.mVersion;
    nodeConfig.mNodeID  = nodeID;

    return ErrorEnum::eNone;
}

} // namespace aos::cm::unitconfig
