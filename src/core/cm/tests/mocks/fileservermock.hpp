/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_FILESERVERMOCK_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_FILESERVERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/fileserver/itf/fileserver.hpp>

namespace aos::cm::fileserver {

/**
 * File server mock.
 */
class FileServerMock : public FileServerItf {
public:
    MOCK_METHOD(Error, TranslateFilePathURL, (const String&, String&), (override));
};

} // namespace aos::cm::fileserver

#endif
