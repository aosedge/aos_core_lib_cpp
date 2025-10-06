/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_UNITCONFIG_HPP_
#define AOS_CORE_COMMON_TYPES_UNITCONFIG_HPP_

#include "common.hpp"

namespace aos {

/**
 * Unit config state type.
 */
class UnitConfigStateType {
public:
    enum class Enum {
        eAbsent,
        eInstalled,
        eFailed,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "absent",
            "installed",
            "failed",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using UnitConfigStateEnum = UnitConfigStateType::Enum;
using UnitConfigState     = EnumStringer<UnitConfigStateType>;

/**
 * Node config.
 */
struct NodeConfig {
    StaticString<cIDLen>                                        mNodeID;
    StaticString<cNodeTypeLen>                                  mNodeType;
    StaticString<cVersionLen>                                   mVersion;
    Optional<AlertRules>                                        mAlertRules;
    Optional<ResourceRatios>                                    mResourceRatios;
    StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels> mLabels;
    uint64_t                                                    mPriority {};

    /**
     * Compares node configs.
     *
     * @param rhs node config to compare.
     * @return bool.
     */
    bool operator==(const NodeConfig& rhs) const
    {
        return mNodeID == rhs.mNodeID && mNodeType == rhs.mNodeType && mVersion == rhs.mVersion
            && mAlertRules == rhs.mAlertRules && mResourceRatios == rhs.mResourceRatios && mLabels == rhs.mLabels
            && mPriority == rhs.mPriority;
    }

    /**
     * Compares node configs.
     *
     * @param rhs node config to compare.
     * @return bool.
     */
    bool operator!=(const NodeConfig& rhs) const { return !operator==(rhs); }
};

/**
 * Unit config.
 */
struct UnitConfig {
    StaticString<cVersionLen>             mVersion;
    StaticString<cVersionLen>             mFormatVersion;
    StaticArray<NodeConfig, cMaxNumNodes> mNodes;

    /**
     * Compares unit config.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const UnitConfig& rhs) const
    {
        return mVersion == rhs.mVersion && mFormatVersion == rhs.mFormatVersion && mNodes == rhs.mNodes;
    }

    /**
     * Compares unit config.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const UnitConfig& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
