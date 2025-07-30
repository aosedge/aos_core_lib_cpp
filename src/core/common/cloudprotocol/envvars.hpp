/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_ENVVARS_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_ENVVARS_HPP_

#include <core/common/types/types.hpp>

#include "cloudprotocol.hpp"

namespace aos::cloudprotocol {

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

/**
 * Environment variable override request.
 */
struct OverrideEnvVarsRequest {
    EnvVarsInstanceInfoArray mItems;

    /**
     * Compares environment variable override request.
     *
     * @param request environment variable override request to compare with.
     * @return bool.
     */
    bool operator==(const OverrideEnvVarsRequest& request) const { return mItems == request.mItems; }

    /**
     * Compares environment variable override request.
     *
     * @param request environment variable override request to compare with.
     * @return bool.
     */
    bool operator!=(const OverrideEnvVarsRequest& request) const { return !operator==(request); }
};

/**
 * Environment variable override statuses.
 */
struct OverrideEnvVarsStatuses {
    EnvVarsInstanceStatusArray mStatuses;

    /**
     * Compares environment variable override statuses.
     *
     * @param statuses environment variable override statuses to compare with.
     * @return bool.
     */
    bool operator==(const OverrideEnvVarsStatuses& statuses) const { return mStatuses == statuses.mStatuses; }

    /**
     * Compares environment variable override statuses.
     *
     * @param statuses environment variable override statuses to compare with.
     * @return bool.
     */
    bool operator!=(const OverrideEnvVarsStatuses& statuses) const { return !operator==(statuses); }
};

} // namespace aos::cloudprotocol

#endif
