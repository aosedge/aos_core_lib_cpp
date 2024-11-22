/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ALERTS_HPP_
#define AOS_ALERTS_HPP_

#include "aos/common/cloudprotocol/alerts.hpp"
#include "aos/common/types.hpp"

namespace aos {
namespace alerts {

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

} // namespace alerts
} // namespace aos

#endif
