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
    friend bool operator==(const NodeConfig& lhs, const NodeConfig& rhs)
    {
        return lhs.mNodeID == rhs.mNodeID && lhs.mNodeType == rhs.mNodeType && lhs.mVersion == rhs.mVersion
            && lhs.mAlertRules == rhs.mAlertRules && lhs.mResourceRatios == rhs.mResourceRatios
            && lhs.mLabels == rhs.mLabels && lhs.mPriority == rhs.mPriority;
    };

    /**
     * Compares node configs.
     *
     * @param rhs node config to compare.
     * @return bool.
     */
    friend bool operator!=(const NodeConfig& lhs, const NodeConfig& rhs) { return !(lhs == rhs); };
};

/**
 * Unit config.
 */
struct UnitConfig {
    StaticString<cVersionLen>             mFormatVersion;
    StaticString<cVersionLen>             mVersion;
    StaticArray<NodeConfig, cMaxNumNodes> mNodes;

    /**
     * Compares unit config.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator==(const UnitConfig& lhs, const UnitConfig& rhs)
    {
        return lhs.mFormatVersion == rhs.mFormatVersion && lhs.mVersion == rhs.mVersion && lhs.mNodes == rhs.mNodes;
    };

    /**
     * Compares unit config.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const UnitConfig& lhs, const UnitConfig& rhs) { return !(lhs == rhs); };
};

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
using NodeConfigState     = UnitConfigState;

/**
 * Unit config status.
 */
struct UnitConfigStatus {
    StaticString<cVersionLen> mVersion;
    UnitConfigState           mState;
    Error                     mError;

    /**
     * Compares unit config status.
     *
     * @param rhs unit config status to compare with.
     * @return bool.
     */
    friend bool operator==(const UnitConfigStatus& lhs, const UnitConfigStatus& rhs)
    {
        return lhs.mVersion == rhs.mVersion && lhs.mState == rhs.mState && lhs.mError == rhs.mError;
    };

    friend bool operator!=(const UnitConfigStatus& lhs, const UnitConfigStatus& rhs) { return !(lhs == rhs); };
};

using NodeConfigStatus = UnitConfigStatus;

} // namespace aos

#endif
