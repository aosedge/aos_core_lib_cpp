/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_ALERTS_ITF_ALERTS_HPP_
#define AOS_CORE_COMMON_ALERTS_ITF_ALERTS_HPP_

#include <core/common/types/alerts.hpp>

namespace aos::alerts {

/**
 * Sender interface.
 */
class SenderItf {
public:
    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;

    /**
     * Sends alert data.
     *
     * @param alert alert variant.
     * @return Error.
     */
    virtual Error SendAlert(const AlertVariant& alert) = 0;
};

} // namespace aos::alerts

#endif
