/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_NAMESPACEMANAGER_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_NAMESPACEMANAGER_HPP_

#include <core/common/types/common.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Namespace manager interface.
 */
class NamespaceManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NamespaceManagerItf() = default;

    /**
     * Creates network namespace.
     *
     * @param ns network namespace name.
     * @return Error.
     */
    virtual Error CreateNetworkNamespace(const String& ns) = 0;

    /**
     * Returns network namespace path.
     *
     * @param ns network namespace name.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> GetNetworkNamespacePath(const String& ns) const = 0;

    /**
     * Deletes network namespace.
     *
     * @param ns network namespace name.
     * @return Error.
     */
    virtual Error DeleteNetworkNamespace(const String& ns) = 0;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
