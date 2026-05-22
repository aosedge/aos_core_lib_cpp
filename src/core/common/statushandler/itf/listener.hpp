/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_STATUSHANDLER_ITF_LISTENER_HPP_
#define AOS_CORE_COMMON_STATUSHANDLER_ITF_LISTENER_HPP_

#include <core/common/types/instance.hpp>

namespace aos::statushandler {

/**
 * Interface for receiving notification about instance status changes.
 */
class ListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ListenerItf() = default;

    /**
     * Notifies about changed instance status.
     *
     * @param status instance status.
     */
    virtual void OnInstanceStatusChanged(const InstanceStatus& status) = 0;
};

} // namespace aos::statushandler

#endif
