/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_TESTS_MOCKS_IMAGEMANAGERMOCK_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_TESTS_MOCKS_IMAGEMANAGERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/imagemanager/itf/imagemanager.hpp>

namespace aos::cm::imagemanager {

/**
 * Image manager mock.
 */
class ImageManagerMock : public ImageManagerItf {
public:
    MOCK_METHOD(Error, SetCerts, (const Array<crypto::CertificateInfo>&, const Array<crypto::CertificateChainInfo>&),
        (override));
    MOCK_METHOD(Error, InstallItem, (const UpdateItemInfo&), (override));
    MOCK_METHOD(Error, RemoveItem, (const String&, const String&), (override));
    MOCK_METHOD(Error, Cancel, (), (override));
};

} // namespace aos::cm::imagemanager

#endif
