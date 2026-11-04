/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>
#include <core/common/tools/uuid.hpp>

#include "nodeidprovider.hpp"

namespace aos::iam::currentnode {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error NodeIDProvider::Init(HWInfoProviderItf& hwInfoProvider, crypto::UUIDItf& uuidProvider)
{
    LOG_DBG() << "Init node ID provider";

    mNodeID.Clear();

    StaticString<cIDLen> hwID;

    if (auto err = hwInfoProvider.GetHWID(hwID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto [space, err] = uuid::StringToUUID(cNodeIDNamespaceUUID);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    uuid::UUID nodeUUID;

    Tie(nodeUUID, err) = uuidProvider.CreateUUIDv5(space, hwID.AsByteArray());
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (err = mNodeID.Assign(uuid::UUIDToString(nodeUUID)); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_INF() << "Node ID initialized" << Log::Field("hwID", hwID) << Log::Field("nodeID", mNodeID);

    return ErrorEnum::eNone;
}

Error NodeIDProvider::GetNodeID(String& nodeID)
{
    if (mNodeID.IsEmpty()) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eWrongState, "node ID is not initialized"));
    }

    if (auto err = nodeID.Assign(mNodeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::iam::currentnode
