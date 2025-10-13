/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IAMCLIENT_ITF_NODEHANDLER_HPP_
#define AOS_CORE_COMMON_IAMCLIENT_ITF_NODEHANDLER_HPP_

#include <core/common/types/common.hpp>

namespace aos::iamclient {

/**
 * Node handler interface.
 */
class NodeHandlerItf {
public:
    /**
     * Destroys node handler interface.
     */
    virtual ~NodeHandlerItf() = default;

    /**
     * Pauses node.
     *
     * @param nodeID node ID.
     * @returns Error.
     */
    virtual Error PauseNode(const String& nodeID) = 0;

    /**
     * Resumes node.
     *
     * @param nodeID node ID.
     * @returns Error.
     */
    virtual Error ResumeNode(const String& nodeID) = 0;
};

} // namespace aos::iamclient

#endif
