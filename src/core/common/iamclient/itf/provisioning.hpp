/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_IAMCLIENT_ITF_PROVISIONING_HPP_
#define AOS_CORE_COMMON_IAMCLIENT_ITF_PROVISIONING_HPP_

#include <core/common/types/common.hpp>

namespace aos::iamclient {

/**
 * Provisioning interface.
 */
class ProvisioningItf {
public:
    /**
     * Destructor.
     */
    virtual ~ProvisioningItf() = default;

    /**
     * Returns IAM cert types.
     *
     * @param nodeID node ID.
     * @param[out] certTypes result certificate types.
     * @returns Error.
     */
    virtual Error GetCertTypes(const String& nodeID, Array<StaticString<cCertTypeLen>>& certTypes) const = 0;

    /**
     * Starts node provisioning.
     *
     * @param nodeID node ID.
     * @param password password.
     * @returns Error.
     */
    virtual Error StartProvisioning(const String& nodeID, const String& password) = 0;

    /**
     * Finishes node provisioning.
     *
     * @param nodeID node ID.
     * @param password password.
     * @returns Error.
     */
    virtual Error FinishProvisioning(const String& nodeID, const String& password) = 0;

    /**
     * Deprovisions node.
     *
     * @param nodeID node ID.
     * @param password password.
     * @returns Error.
     */
    virtual Error Deprovision(const String& nodeID, const String& password) = 0;
};

} // namespace aos::iamclient

#endif
