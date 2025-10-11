/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_TESTS_MOCKS_FILEINFOPROVIDER_HPP_
#define AOS_CORE_CM_TESTS_MOCKS_FILEINFOPROVIDER_HPP_

#include <gmock/gmock.h>

#include <core/common/tools/fs.hpp>

namespace aos::fs {

class MockFileInfoProvider : public FileInfoProviderItf {
public:
    MOCK_METHOD(Error, CreateFileInfo, (const String& path, FileInfo& info), (override));
    MOCK_METHOD(Error, CreateSHA256, (const String& path, Array<uint8_t>& sha256), (override));
};

} // namespace aos::fs

#endif
