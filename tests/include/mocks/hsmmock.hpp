/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_HSM_MOCK_HPP_
#define AOS_HSM_MOCK_HPP_

#include <aos/iam/certmodules/hsm.hpp>
#include <gmock/gmock.h>

/**
 * Provides interface to mock HSM interface.
 */
class HSMItfMock : public aos::iam::certhandler::HSMItf {
public:
    MOCK_METHOD(aos::Error, SetOwner, (const aos::String&), (override));
    MOCK_METHOD(aos::Error, Clear, (), (override));
    MOCK_METHOD(aos::RetWithError<aos::SharedPtr<aos::crypto::PrivateKeyItf>>, CreateKey,
        (const aos::String&, aos::crypto::KeyType), (override));
    MOCK_METHOD(aos::Error, ApplyCert,
        (const aos::Array<aos::crypto::x509::Certificate>&, aos::iam::certhandler::CertInfo&, aos::String&),
        (override));
    MOCK_METHOD(aos::Error, RemoveCert, (const aos::String&, const aos::String&), (override));
    MOCK_METHOD(aos::Error, RemoveKey, (const aos::String&, const aos::String&), (override));
    MOCK_METHOD(aos::Error, ValidateCertificates,
        (aos::Array<aos::StaticString<aos::cURLLen>>&, aos::Array<aos::StaticString<aos::cURLLen>>&,
            aos::Array<aos::iam::certhandler::CertInfo>&),
        (override));
};

#endif
