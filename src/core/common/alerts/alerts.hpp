/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_ALERTS_ALERTS_HPP_
#define AOS_CORE_COMMON_ALERTS_ALERTS_HPP_

#include <core/common/cloudprotocol/alerts.hpp>
#include <core/common/types/obsolete.hpp>

namespace aos::alerts {

/**
 * Sender interface.
 */
class SenderItf {
public:
    /**
     * Sends alert data.
     *
     * @param alert alert variant.
     * @return Error.
     */
    virtual Error SendAlert(const cloudprotocol::AlertVariant& alert) = 0;

    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;
};

} // namespace aos::alerts

#endif
