/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_PERMISSIONS_HPP_
#define AOS_CORE_COMMON_TYPES_PERMISSIONS_HPP_

#include "common.hpp"

namespace aos {

/**
 * Permissions length.
 */
static constexpr auto cPermissionsLen = AOS_CONFIG_TYPES_PERMISSIONS_LEN;

/**
 * Function name length.
 */
static constexpr auto cFunctionLen = AOS_CONFIG_TYPES_FUNCTION_LEN;

/**
 * Max number of functions for functional service.
 */
static constexpr auto cFunctionsMaxCount = AOS_CONFIG_TYPES_FUNCTIONS_MAX_COUNT;

/**
 * Functional service name length.
 */
static constexpr auto cFuncServiceLen = AOS_CONFIG_TYPES_FUNC_SERVICE_LEN;

/**
 * Maximum number of functional services.
 */
static constexpr auto cFuncServiceMaxCount = AOS_CONFIG_TYPES_FUNC_SERVICE_MAX_COUNT;

/**
 * Function permissions.
 */
struct FunctionPermissions {
    StaticString<cFunctionLen>    mFunction;
    StaticString<cPermissionsLen> mPermissions;

    /**
     * Compares function permissions.
     *
     * @param rhs permissions to compare.
     * @return bool.
     */
    bool operator==(const FunctionPermissions& rhs)
    {
        return (mFunction == rhs.mFunction) && (mPermissions == rhs.mPermissions);
    }

    /**
     * Compares function permissions.
     *
     * @param rhs permissions to compare.
     * @return bool.
     */
    bool operator!=(const FunctionPermissions& rhs) { return !operator==(rhs); }
};

/**
 * Function service permissions.
 */
struct FunctionServicePermissions {
    StaticString<cFuncServiceLen>                        mName;
    StaticArray<FunctionPermissions, cFunctionsMaxCount> mPermissions;
};

} // namespace aos

#endif // AOS_CORE_COMMON_TYPES_PERMISSIONS_HPP_
