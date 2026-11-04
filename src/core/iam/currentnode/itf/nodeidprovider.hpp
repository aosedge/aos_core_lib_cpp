/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CURRENTNODE_ITF_NODEIDPROVIDER_HPP_
#define AOS_CORE_IAM_CURRENTNODE_ITF_NODEIDPROVIDER_HPP_

#include <core/common/tools/error.hpp>
#include <core/common/tools/string.hpp>

namespace aos::iam::currentnode {

/**
 * Node ID provider interface.
 */
class NodeIDProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeIDProviderItf() = default;

    /**
     * Returns node ID.
     *
     * @param[out] nodeID node ID.
     * @return Error.
     */
    virtual Error GetNodeID(String& nodeID) = 0;
};

} // namespace aos::iam::currentnode

#endif
