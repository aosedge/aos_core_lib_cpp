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

struct EnvVar {
    StaticString<cEnvVarNameLen>  mName;
    StaticString<cEnvVarValueLen> mValue;

    /**
     * Compares environment variable.
     *
     * @param rhs environment variable to compare with.
     * @return bool.
     */
    friend bool operator==(const EnvVar& lhs, const EnvVar& rhs)
    {
        return lhs.mName == rhs.mName && lhs.mValue == rhs.mValue;
    };

    /**
     * Compares environment variable.
     *
     * @param rhs environment variable to compare with.
     * @return bool.
     */
    friend bool operator!=(const EnvVar& lhs, const EnvVar& rhs) { return !(lhs == rhs); };
};

/**
 * Env vars array.
 */
using EnvVarArray = StaticArray<EnvVar, cMaxNumEnvVariables>;

/**
 * Environment variable info.
 */
struct EnvVarInfo : public EnvVar {
    Optional<Time> mTTL;

    /**
     * Compares environment variable info.
     *
     * @param rhs environment variable info to compare with.
     * @return bool.
     */
    friend bool operator==(const EnvVarInfo& lhs, const EnvVarInfo& rhs)
    {
        return (static_cast<const EnvVar&>(lhs) == rhs) && lhs.mTTL == rhs.mTTL;
    };

    /**
     * Compares environment variable info.
     *
     * @param rhs environment variable info to compare with.
     * @return bool.
     */
    friend bool operator!=(const EnvVarInfo& lhs, const EnvVarInfo& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const EnvVarsInstanceInfo& lhs, const EnvVarsInstanceInfo& rhs)
    {
        return (static_cast<const InstanceFilter&>(lhs) == rhs) && lhs.mVariables == rhs.mVariables;
    };

    /**
     * Compares environment variable instance info.
     *
     * @param rhs environment variable instance info to compare with.
     * @return bool.
     */
    friend bool operator!=(const EnvVarsInstanceInfo& lhs, const EnvVarsInstanceInfo& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const EnvVarStatus& lhs, const EnvVarStatus& rhs)
    {
        return lhs.mName == rhs.mName && lhs.mError == rhs.mError;
    };

    /**
     * Compares environment variable status.
     *
     * @param rhs environment variable instance to compare with.
     * @return bool.
     */
    friend bool operator!=(const EnvVarStatus& lhs, const EnvVarStatus& rhs) { return !(lhs == rhs); };
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
    friend bool operator==(const EnvVarsInstanceStatus& lhs, const EnvVarsInstanceStatus& rhs)
    {
        return (static_cast<const InstanceIdent&>(lhs) == rhs) && lhs.mStatuses == rhs.mStatuses;
    };

    /**
     * Compares environment variable instance status.
     *
     * @param rhs environment variable instance status to compare with.
     * @return bool.
     */
    friend bool operator!=(const EnvVarsInstanceStatus& lhs, const EnvVarsInstanceStatus& rhs)
    {
        return !(lhs == rhs);
    };
};

using EnvVarsInstanceStatusArray = StaticArray<EnvVarsInstanceStatus, cMaxNumInstances>;

/**
 * Environment variable override request.
 */
struct OverrideEnvVarsRequest : public Protocol {
    EnvVarsInstanceInfoArray mItems;

    /**
     * Compares environment variable override request.
     *
     * @param rhs environment variable override request to compare with.
     * @return bool.
     */
    friend bool operator==(const OverrideEnvVarsRequest& lhs, const OverrideEnvVarsRequest& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mItems == rhs.mItems;
    };

    /**
     * Compares environment variable override request.
     *
     * @param rhs environment variable override request to compare with.
     * @return bool.
     */
    friend bool operator!=(const OverrideEnvVarsRequest& lhs, const OverrideEnvVarsRequest& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * Environment variable override statuses.
 */
struct OverrideEnvVarsStatuses : public Protocol {
    EnvVarsInstanceStatusArray mStatuses;

    /**
     * Compares environment variable override statuses.
     *
     * @param rhs environment variable override statuses to compare with.
     * @return bool.
     */
    friend bool operator==(const OverrideEnvVarsStatuses& lhs, const OverrideEnvVarsStatuses& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mStatuses == rhs.mStatuses;
    };

    /**
     * Compares environment variable override statuses.
     *
     * @param statuses environment variable override statuses to compare with.
     * @return bool.
     */
    friend bool operator!=(const OverrideEnvVarsStatuses& lhs, const OverrideEnvVarsStatuses& rhs)
    {
        return !(lhs == rhs);
    };
};

} // namespace aos

#endif
