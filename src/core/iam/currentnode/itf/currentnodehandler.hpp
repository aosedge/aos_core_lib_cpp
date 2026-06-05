/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CURRENTNODE_ITF_CURRENTNODEHANDLER_HPP_
#define AOS_CORE_IAM_CURRENTNODE_ITF_CURRENTNODEHANDLER_HPP_

#include <core/common/iamclient/itf/currentnodeinfoprovider.hpp>

namespace aos::iam::currentnode {

/**
 * Node handler interface.
 */
class CurrentNodeHandlerItf : public iamclient::CurrentNodeInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~CurrentNodeHandlerItf() = default;

    /**
     * Sets current node state.
     *
     * @param state new node state.
     * @return Error.
     */
    virtual Error SetState(NodeState state) = 0;

    /**
     * Sets current node connected state.
     *
     * @param isConnected connected state.
     * @return Error.
     */
    virtual Error SetConnected(bool isConnected) = 0;
};

} // namespace aos::iam::currentnode

#endif
