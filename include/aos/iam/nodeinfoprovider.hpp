/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_NODEINFOPROVIDER_HPP_
#define AOS_NODEINFOPROVIDER_HPP_

#include "aos/common/tools/error.hpp"
#include "aos/common/types.hpp"

namespace aos {
namespace iam {

/**
 * Main node attribute.
 */
static constexpr auto cAttrMainNode = "MainNode";

/**
 * Aos components attribute.
 */
static constexpr auto cAttrAosComponents = "AosComponents";

/**
 * Node runners attribute.
 */
static constexpr auto cAttrNodeRunners = "NodeRunners";

/**
 * Aos component cm.
 */
static constexpr auto cAosComponentCM = "cm";

/**
 * Aos component iam.
 */
static constexpr auto cAosComponentIAM = "iam";

/**
 * Aos component sm.
 */
static constexpr auto cAosComponentSM = "sm";

/**
 * Aos component um.
 */
static constexpr auto cAosComponentUM = "um";

/**
 * Node info provider interface.
 */
struct NodeInfoProviderItf {
    /**
     * Gets the node info object.
     *
     * @param[out] nodeInfo node info
     * @return Error
     */
    virtual Error GetNodeInfo(NodeInfo& nodeInfo) const = 0;

    /**
     * Sets the node status.
     *
     * @param status node status
     * @return Error
     */
    virtual Error SetNodeStatus(const NodeStatus& status) = 0;

    /**
     * Destructor.
     */
    virtual ~NodeInfoProviderItf() = default;
};

} // namespace iam
} // namespace aos

#endif
