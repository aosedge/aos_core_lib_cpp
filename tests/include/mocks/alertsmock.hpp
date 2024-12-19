/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ALERTS_MOCK_HPP_
#define AOS_ALERTS_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/common/alerts/alerts.hpp"

namespace aos::alerts {

class AlertSenderMock : public SenderItf {
public:
    MOCK_METHOD(Error, SendAlert, (const cloudprotocol::AlertVariant&), (override));
};

} // namespace aos::alerts

#endif
