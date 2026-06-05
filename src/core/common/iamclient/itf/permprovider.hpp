/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IAMCLIENT_ITF_PERMPROVIDER_HPP_
#define AOS_CORE_COMMON_IAMCLIENT_ITF_PERMPROVIDER_HPP_

#include <core/common/types/permissions.hpp>

namespace aos::iamclient {

/**
 * Permission provider interface.
 */
class PermProviderItf {
public:
    /**
     * Destroys permission provider interface.
     */
    virtual ~PermProviderItf() = default;

    /**
     * Returns instance ident and permissions by secret and functional server ID.
     *
     * @param secret secret.
     * @param funcServerID functional server ID.
     * @param[out] instanceIdent result instance ident.
     * @param[out] servicePermissions result service permission.
     * @returns Error.
     */
    virtual Error GetPermissions(const String& secret, const String& funcServerID, InstanceIdent& instanceIdent,
        Array<FunctionPermissions>& servicePermissions)
        = 0;
};

} // namespace aos::iamclient

#endif
