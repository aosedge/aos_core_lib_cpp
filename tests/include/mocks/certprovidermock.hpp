/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CERTPROVIDER_MOCK_HPP_
#define CERTPROVIDER_MOCK_HPP_

#include <gmock/gmock.h>

#include <aos/iam/certprovider.hpp>

namespace aos::iam::certprovider {

/**
 * Mocks certificate provider.
 */

class CertProviderMock : public CertProviderItf {
public:
    MOCK_METHOD(Error, GetCert,
        (const String& certType, const Array<uint8_t>& issuer, const Array<uint8_t>& serial,
            certhandler::CertInfo& resCert),
        (const override));
    MOCK_METHOD(
        Error, SubscribeCertChanged, (const String& certType, certhandler::CertReceiverItf& certReceiver), (override));
    MOCK_METHOD(Error, UnsubscribeCertChanged, (certhandler::CertReceiverItf & certReceiver), (override));
};

} // namespace aos::iam::certprovider

#endif
