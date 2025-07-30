/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROVISIONMANAGER_MOCK_HPP_
#define PROVISIONMANAGER_MOCK_HPP_

#include <gmock/gmock.h>

#include <aos/iam/provisionmanager.hpp>

namespace aos::iam::provisionmanager {

/**
 * Provision manager callback mock.
 */
class ProvisionManagerCallbackMock : public ProvisionManagerCallbackItf {
public:
    MOCK_METHOD(Error, OnStartProvisioning, (const String&), (override));
    MOCK_METHOD(Error, OnFinishProvisioning, (const String&), (override));
    MOCK_METHOD(Error, OnDeprovision, (const String&), (override));
    MOCK_METHOD(Error, OnEncryptDisk, (const String&), (override));
};

/**
 * ProvisionManager interface mock
 */
class ProvisionManagerMock : public ProvisionManagerItf {
public:
    MOCK_METHOD(Error, StartProvisioning, (const String& password), (override));
    MOCK_METHOD(RetWithError<CertTypes>, GetCertTypes, (), (const override));
    MOCK_METHOD(Error, CreateKey, (const String& certType, const String& subject, const String& password, String& csr),
        (override));
    MOCK_METHOD(
        Error, ApplyCert, (const String& certType, const String& pemCert, certhandler::CertInfo& certInfo), (override));
    MOCK_METHOD(Error, FinishProvisioning, (const String& password), (override));
    MOCK_METHOD(Error, Deprovision, (const String& password), (override));
};

} // namespace aos::iam::provisionmanager

#endif
