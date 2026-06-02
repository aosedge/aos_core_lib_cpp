/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_SM_LAUNCHER_ITF_INSTANCEIDPROVIDER_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_INSTANCEIDPROVIDER_HPP_

#include <core/common/types/common.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Instance ID provider.
 */
class InstanceIDProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~InstanceIDProviderItf() = default;

    /**
     * Returns instance ID.
     *
     * @param instance instance ident.
     * @param[out] instanceID instance ID.
     * @return Error.
     */
    virtual Error GetInstanceID(const InstanceIdent& instance, String& instanceID) const = 0;
};

} // namespace aos::sm::launcher

#endif
