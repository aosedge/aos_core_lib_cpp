/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IAMCLIENT_ITF_PERMHANDLER_HPP_
#define AOS_CORE_COMMON_IAMCLIENT_ITF_PERMHANDLER_HPP_

#include <core/common/types/permissions.hpp>

namespace aos::iamclient {

/**
 * Permission handler interface.
 */
class PermHandlerItf {
public:
    /**
     * Destroys permission handler interface.
     */
    virtual ~PermHandlerItf() = default;

    /**
     * Adds new service instance and its permissions into cache.
     *
     * @param instanceIdent instance identification.
     * @param instancePermissions instance permissions.
     * @returns RetWithError<StaticString<cSecretLen>>.
     */
    virtual RetWithError<StaticString<cSecretLen>> RegisterInstance(
        const InstanceIdent& instanceIdent, const Array<FunctionServicePermissions>& instancePermissions)
        = 0;

    /**
     * Unregisters instance deletes service instance with permissions from cache.
     *
     * @param instanceIdent instance identification.
     * @returns Error.
     */
    virtual Error UnregisterInstance(const InstanceIdent& instanceIdent) = 0;
};

} // namespace aos::iamclient

#endif
