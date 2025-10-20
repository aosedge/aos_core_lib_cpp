/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_ALERTS_ITF_RECEIVER_HPP_
#define AOS_CORE_CM_ALERTS_ITF_RECEIVER_HPP_

#include <core/common/types/alerts.hpp>

namespace aos::cm::smcontroller {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Alert receiver interface.
 */
class ReceiverItf {
public:
    /**
     * Destructor.
     */
    virtual ~ReceiverItf() = default;

    /**
     * Receives alert.
     *
     * @param alert alert.
     * @return Error.
     */
    virtual Error OnAlertReceived(const AlertVariant& alert) = 0;
};

} // namespace aos::cm::smcontroller

#endif
