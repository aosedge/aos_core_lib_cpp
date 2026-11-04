/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CURRENTNODE_NODEIDPROVIDER_NODEIDPROVIDER_HPP_
#define AOS_CORE_IAM_CURRENTNODE_NODEIDPROVIDER_NODEIDPROVIDER_HPP_

#include <core/common/crypto/itf/uuid.hpp>
#include <core/common/types/common.hpp>
#include <core/iam/currentnode/itf/hwinfoprovider.hpp>
#include <core/iam/currentnode/itf/nodeidprovider.hpp>

namespace aos::iam::currentnode {

/**
 * Node ID provider.
 *
 * Derives node ID from hardware ID as UUIDv5 in cNodeIDNamespaceUUID namespace.
 */
class NodeIDProvider : public NodeIDProviderItf {
public:
    /**
     * Initializes node ID provider.
     *
     * @param hwInfoProvider hardware info provider.
     * @param uuidProvider UUID provider.
     * @return Error.
     */
    Error Init(HWInfoProviderItf& hwInfoProvider, crypto::UUIDItf& uuidProvider);

    /**
     * Returns node ID.
     *
     * @param[out] nodeID node ID.
     * @return Error.
     */
    Error GetNodeID(String& nodeID) override;

private:
    StaticString<cIDLen> mNodeID;
};

} // namespace aos::iam::currentnode

#endif
