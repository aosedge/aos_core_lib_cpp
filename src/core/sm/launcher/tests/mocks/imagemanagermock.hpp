/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_TESTS_MOCKS_IMAGEMANAGERMOCK_HPP_
#define AOS_CORE_SM_LAUNCHER_TESTS_MOCKS_IMAGEMANAGERMOCK_HPP_

#include <core/sm/imagemanager/itf/imagemanager.hpp>

namespace aos::sm::imagemanager {

/**
 * Image manager mock.
 */
class ImageManagerMock : public ImageManagerItf {
public:
    MOCK_METHOD(Error, InstallItem, (const UpdateItemInfo&), (override));
    MOCK_METHOD(Error, RemoveItem, (const String&, const String&), (override));
    MOCK_METHOD(Error, Cancel, (), (override));
};

} // namespace aos::sm::imagemanager

#endif
