/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_ENVVARS_HPP_
#define AOS_CORE_COMMON_TYPES_ENVVARS_HPP_

#include "common.hpp"

namespace aos {

/**
 * Environment variable name len.
 */
constexpr auto cEnvVarNameLen = AOS_CONFIG_TYPES_ENV_VAR_NAME_LEN;

/**
 * Environment variable value len.
 */
constexpr auto cEnvVarValueLen = AOS_CONFIG_TYPES_ENV_VAR_VALUE_LEN;

/**
 * Environment variable len.
 *
 * Consists of name and value plus equal sign.
 */
constexpr auto cEnvVarLen = cEnvVarNameLen + cEnvVarValueLen + 1;

/**
 * Max number of environment variables.
 */
constexpr auto cMaxNumEnvVariables = AOS_CONFIG_TYPES_MAX_NUM_ENV_VARIABLES;

/**
 * Env vars array.
 */
using EnvVarArray = StaticArray<StaticString<cEnvVarLen>, cMaxNumEnvVariables>;

/**
 * Environment variable info.
 */
struct EnvVarInfo {
    StaticString<cEnvVarNameLen>  mName;
    StaticString<cEnvVarValueLen> mValue;
    Optional<Time>                mTTL;

    /**
     * Compares environment variable info.
     *
     * @param rhs environment variable info to compare with.
     * @return bool.
     */
    bool operator==(const EnvVarInfo& rhs) const
    {
        return mName == rhs.mName && mValue == rhs.mValue && mTTL == rhs.mTTL;
    }

    /**
     * Compares environment variable info.
     *
     * @param rhs environment variable info to compare with.
     * @return bool.
     */
    bool operator!=(const EnvVarInfo& rhs) const { return !operator==(rhs); }
};

/**
 * Env vars info array.
 */
using EnvVarInfoArray = StaticArray<EnvVarInfo, cMaxNumEnvVariables>;

/**
 * Environment variables instance info.
 */
struct EnvVarsInstanceInfo : public InstanceFilter {
    EnvVarInfoArray mVariables;

    /**
     * Default constructor.
     */
    EnvVarsInstanceInfo() = default;

    /**
     * Creates environment variable instance info.
     *
     * @param filter instance filter.
     * @param variables environment variables.
     */
    EnvVarsInstanceInfo(const InstanceFilter& filter, const Array<EnvVarInfo>& variables)
        : InstanceFilter(filter)
        , mVariables(variables)
    {
    }

    /**
     * Compares environment variable instance info.
     *
     * @param rhs environment variable instance info to compare with.
     * @return bool.
     */
    bool operator==(const EnvVarsInstanceInfo& rhs) const
    {
        return InstanceFilter::operator==(rhs) && mVariables == rhs.mVariables;
    }

    /**
     * Compares environment variable instance info.
     *
     * @param rhs environment variable instance info to compare with.
     * @return bool.
     */
    bool operator!=(const EnvVarsInstanceInfo& rhs) const { return !operator==(rhs); }
};

using EnvVarsInstanceInfoArray = StaticArray<EnvVarsInstanceInfo, cMaxNumInstances>;

/**
 * Environment variable status.
 */
struct EnvVarStatus {
    StaticString<cEnvVarNameLen> mName;
    Error                        mError;

    /**
     * Compares environment variable status.
     *
     * @param rhs environment variable instance to compare with.
     * @return bool.
     */
    bool operator==(const EnvVarStatus& rhs) const { return mName == rhs.mName && mError == rhs.mError; }

    /**
     * Compares environment variable status.
     *
     * @param rhs environment variable instance to compare with.
     * @return bool.
     */
    bool operator!=(const EnvVarStatus& rhs) const { return !operator==(rhs); }
};

using EnvVarStatusArray = StaticArray<EnvVarStatus, cMaxNumEnvVariables>;

/**
 * Environment variables instance status.
 */
struct EnvVarsInstanceStatus : public InstanceIdent {
    EnvVarStatusArray mStatuses;

    /**
     * Compares environment variable instance status.
     *
     * @param rhs environment variable instance status to compare with.
     * @return bool.
     */
    bool operator==(const EnvVarsInstanceStatus& rhs) const
    {
        return InstanceIdent::operator==(rhs) && mStatuses == rhs.mStatuses;
    }

    /**
     * Compares environment variable instance status.
     *
     * @param rhs environment variable instance status to compare with.
     * @return bool.
     */
    bool operator!=(const EnvVarsInstanceStatus& rhs) const { return !operator==(rhs); }
};

using EnvVarsInstanceStatusArray = StaticArray<EnvVarsInstanceStatus, cMaxNumInstances>;

/**
 * Environment variable override request.
 */
struct OverrideEnvVarsRequest {
    EnvVarsInstanceInfoArray mItems;

    /**
     * Compares environment variable override request.
     *
     * @param rhs environment variable override request to compare with.
     * @return bool.
     */
    bool operator==(const OverrideEnvVarsRequest& rhs) const { return mItems == rhs.mItems; }

    /**
     * Compares environment variable override request.
     *
     * @param rhs environment variable override request to compare with.
     * @return bool.
     */
    bool operator!=(const OverrideEnvVarsRequest& rhs) const { return !operator==(rhs); }
};

/**
 * Environment variable override statuses.
 */
struct OverrideEnvVarsStatuses {
    EnvVarsInstanceStatusArray mStatuses;

    /**
     * Compares environment variable override statuses.
     *
     * @param rhs environment variable override statuses to compare with.
     * @return bool.
     */
    bool operator==(const OverrideEnvVarsStatuses& rhs) const { return mStatuses == rhs.mStatuses; }

    /**
     * Compares environment variable override statuses.
     *
     * @param statuses environment variable override statuses to compare with.
     * @return bool.
     */
    bool operator!=(const OverrideEnvVarsStatuses& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
