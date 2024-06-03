/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROVISIONING_CALLBACK_MOCK_HPP_
#define PROVISIONING_CALLBACK_MOCK_HPP_

#include <gmock/gmock.h>

#include <aos/iam/provisionmanager.hpp>

/**
 * Provision manager callback mock.
 */
class ProvisionManagerCallbackMock : public aos::iam::provisionmanager::ProvisionManagerCallback {
public:
    MOCK_METHOD(aos::Error, OnStartProvisioning, (const aos::String&), (override));
    MOCK_METHOD(aos::Error, OnFinishProvisioning, (const aos::String&), (override));
    MOCK_METHOD(aos::Error, OnDeprovision, (const aos::String&), (override));
    MOCK_METHOD(aos::Error, OnEncryptDisk, (const aos::String&), (override));
};

#endif
