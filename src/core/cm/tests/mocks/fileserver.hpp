/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_FILESERVER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_FILESERVER_HPP_

#include <gmock/gmock.h>

#include <core/cm/fileserver/itf/fileserver.hpp>

namespace aos::cm::fileserver {

class MockFileServer : public FileServerItf {
public:
    MOCK_METHOD(Error, TranslateFilePathURL, (const String& filePath, String& outURL), (override));
};

} // namespace aos::cm::fileserver

#endif
