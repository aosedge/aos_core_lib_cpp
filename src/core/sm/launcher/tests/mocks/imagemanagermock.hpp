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
    MOCK_METHOD(Error, GetAllInstalledItems, (Array<UpdateItemStatus>&), (const, override));
    MOCK_METHOD(Error, InstallUpdateItem, (const UpdateItemInfo&), (override));
    MOCK_METHOD(Error, RemoveUpdateItem, (const String&, const String&), (override));
};

} // namespace aos::sm::imagemanager

#endif
