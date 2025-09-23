/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_ENVVARS_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_ENVVARS_HPP_

#include <core/common/types/types.hpp>

#include "common.hpp"

namespace aos::cloudprotocol {

/**
 * Environment variables instance info.
 */
struct EnvVarsInstanceInfo : public InstanceFilter {
    EnvVarInfoArray mVariables;

    /**
     * Compares environment variable instance info.
     *
     * @param info environment variable instance info to compare with.
     * @return bool.
     */
    bool operator==(const EnvVarsInstanceInfo& info) const
    {
        return InstanceFilter::operator==(info) && mVariables == info.mVariables;
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
 * Environment variables instance status.
 */
struct EnvVarsInstanceStatus : public InstanceIdent {
    EnvVarStatusArray mStatuses;

    /**
     * Compares environment variable instance status.
     *
     * @param status environment variable instance status to compare with.
     * @return bool.
     */
    bool operator==(const EnvVarsInstanceStatus& status) const
    {
        return InstanceIdent::operator==(status) && mStatuses == status.mStatuses;
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
