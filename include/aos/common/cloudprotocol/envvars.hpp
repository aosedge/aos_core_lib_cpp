/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_ENVVARS_HPP_
#define AOS_CLOUDPROTOCOL_ENVVARS_HPP_

#include "aos/common/cloudprotocol/cloudprotocol.hpp"
#include "aos/common/types.hpp"

namespace aos::cloudprotocol {

/**
 * Environment variable name value.
 */
constexpr auto cEnvVarValueLen = AOS_CONFIG_CLOUDPROTOCOL_ENV_VAR_VALUE_LEN;

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
     * @param info environment variable info to compare with.
     * @return bool.
     */
    bool operator==(const EnvVarInfo& info) const
    {
        return mName == info.mName && mValue == info.mValue && mTTL == info.mTTL;
    }

    /**
     * Compares environment variable info.
     *
     * @param info environment variable info to compare with.
     * @return bool.
     */
    bool operator!=(const EnvVarInfo& info) const { return !operator==(info); }
};

/**
 * Env vars info static array.
 */
using EnvVarInfoArray = StaticArray<EnvVarInfo, cMaxNumEnvVariables>;

/**
 * Environment variables instance info.
 */
struct EnvVarsInstanceInfo {
    InstanceFilter  mFilter;
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
        : mFilter(filter)
        , mVariables(variables)
    {
    }

    /**
     * Compares environment variable instance info.
     *
     * @param info environment variable instance info to compare with.
     * @return bool.
     */
    bool operator==(const EnvVarsInstanceInfo& info) const
    {
        return mFilter == info.mFilter && mVariables == info.mVariables;
    }

    /**
     * Compares environment variable instance info.
     *
     * @param info environment variable instance info to compare with.
     * @return bool.
     */
    bool operator!=(const EnvVarsInstanceInfo& info) const { return !operator==(info); }
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
     * @param status environment variable instance to compare with.
     * @return bool.
     */
    bool operator==(const EnvVarStatus& status) const { return mName == status.mName && mError == status.mError; }

    /**
     * Compares environment variable status.
     *
     * @param status environment variable instance to compare with.
     * @return bool.
     */
    bool operator!=(const EnvVarStatus& status) const { return !operator==(status); }
};

using EnvVarStatusArray = StaticArray<EnvVarStatus, cMaxNumEnvVariables>;

/**
 * Environment variables instance status.
 */
struct EnvVarsInstanceStatus {
    InstanceFilter    mFilter;
    EnvVarStatusArray mStatuses;

    /**
     * Default constructor.
     */
    EnvVarsInstanceStatus() = default;

    /**
     * Creates environment variable instance status.
     *
     * @param filter instance filter.
     * @param statuses environment variable statuses.
     */
    EnvVarsInstanceStatus(const InstanceFilter& filter, const Array<EnvVarStatus>& statuses)
        : mFilter(filter)
        , mStatuses(statuses)
    {
    }

    /**
     * Compares environment variable instance status.
     *
     * @param status environment variable instance status to compare with.
     * @return bool.
     */
    bool operator==(const EnvVarsInstanceStatus& status) const
    {
        return mFilter == status.mFilter && mStatuses == status.mStatuses;
    }

    /**
     * Compares environment variable instance status.
     *
     * @param status environment variable instance status to compare with.
     * @return bool.
     */
    bool operator!=(const EnvVarsInstanceStatus& status) const { return !operator==(status); }
};

using EnvVarsInstanceStatusArray = StaticArray<EnvVarsInstanceStatus, cMaxNumInstances>;

} // namespace aos::cloudprotocol

#endif
