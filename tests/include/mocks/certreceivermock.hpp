/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CERT_RECEIVER_MOCK_HPP_
#define CERT_RECEIVER_MOCK_HPP_

#include <gmock/gmock.h>

#include <aos/iam/certhandler.hpp>

/**
 * Certificate handler mock.
 */
class CertReceiverItfMock : public aos::iam::certhandler::CertReceiverItf {
public:
    MOCK_METHOD(void, OnCertChanged, (const aos::iam::certhandler::CertInfo& info), (override));
};

#endif
