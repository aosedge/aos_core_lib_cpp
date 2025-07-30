/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CERTHANDLER_MOCK_HPP_
#define AOS_CERTHANDLER_MOCK_HPP_

#include <gmock/gmock.h>

#include <aos/iam/certhandler.hpp>

namespace aos::iam::certhandler {
/**
 * Certificate handler mock.
 */
class CertHandlerMock : public CertHandlerItf {
public:
    MOCK_METHOD(Error, GetCertTypes, (Array<StaticString<cCertTypeLen>>&), (const override));
    MOCK_METHOD(Error, SetOwner, (const String&, const String&), (override));
    MOCK_METHOD(Error, Clear, (const String&), (override));
    MOCK_METHOD(Error, CreateKey, (const String&, const String&, const String&, String&), (override));
    MOCK_METHOD(Error, ApplyCertificate, (const String&, const String&, CertInfo&), (override));
    MOCK_METHOD(
        Error, GetCertificate, (const String&, const Array<uint8_t>&, const Array<uint8_t>&, CertInfo&), (override));
    MOCK_METHOD(Error, SubscribeCertChanged, (const String&, CertReceiverItf&), (override));
    MOCK_METHOD(Error, UnsubscribeCertChanged, (CertReceiverItf & certReceiver), (override));
    MOCK_METHOD(Error, CreateSelfSignedCert, (const String&, const String&), (override));
    MOCK_METHOD(RetWithError<ModuleConfig>, GetModuleConfig, (const String&), (const, override));
};

/**
 * Certificate receiver mock.
 */
class CertReceiverMock : public CertReceiverItf {
public:
    MOCK_METHOD(void, OnCertChanged, (const CertInfo& info), (override));
};

/**
 * Provides interface to mock HSM interface.
 */
class HSMMock : public HSMItf {
public:
    MOCK_METHOD(Error, SetOwner, (const String&), (override));
    MOCK_METHOD(Error, Clear, (), (override));
    MOCK_METHOD(
        RetWithError<SharedPtr<crypto::PrivateKeyItf>>, CreateKey, (const String&, crypto::KeyType), (override));
    MOCK_METHOD(Error, ApplyCert, (const Array<crypto::x509::Certificate>&, CertInfo&, String&), (override));
    MOCK_METHOD(Error, RemoveCert, (const String&, const String&), (override));
    MOCK_METHOD(Error, RemoveKey, (const String&, const String&), (override));
    MOCK_METHOD(Error, ValidateCertificates,
        (Array<StaticString<cURLLen>>&, Array<StaticString<cURLLen>>&, Array<CertInfo>&), (override));
};

} // namespace aos::iam::certhandler

#endif
