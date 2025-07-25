/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_DESIREDSTATUS_HPP_
#define AOS_CLOUDPROTOCOL_DESIREDSTATUS_HPP_

#include "aos/common/cloudprotocol/cloudprotocol.hpp"
#include "aos/common/types.hpp"

namespace aos::cloudprotocol {

/**
 * Node config.
 */
struct NodeConfig {
    StaticString<cNodeTypeLen>                                  mNodeType;
    StaticString<cNodeIDLen>                                    mNodeID;
    Optional<AlertRules>                                        mAlertRules;
    Optional<ResourceRatios>                                    mResourceRatios;
    StaticArray<DeviceInfo, cMaxNumNodeDevices>                 mDevices;
    StaticArray<ResourceInfo, cMaxNumNodeResources>             mResources;
    StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels> mLabels;
    size_t                                                      mPriority {0};

    /**
     * Compares node configs.
     *
     * @param nodeConfig node config to compare.
     * @return bool.
     */
    bool operator==(const NodeConfig& nodeConfig) const
    {
        return mNodeType == nodeConfig.mNodeType && mNodeID == nodeConfig.mNodeID
            && mAlertRules == nodeConfig.mAlertRules && mResourceRatios == nodeConfig.mResourceRatios
            && mDevices == nodeConfig.mDevices && mResources == nodeConfig.mResources && mLabels == nodeConfig.mLabels
            && mPriority == nodeConfig.mPriority;
    }

    /**
     * Compares node configs.
     *
     * @param nodeConfig node config to compare.
     * @return bool.
     */
    bool operator!=(const NodeConfig& nodeConfig) const { return !operator==(nodeConfig); }
};

} // namespace aos::cloudprotocol

#endif
