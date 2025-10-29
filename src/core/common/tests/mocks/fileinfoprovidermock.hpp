/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_TESTS_MOCKS_FILEINFOPROVIDERMOCK_HPP_
#define AOS_CORE_CM_TESTS_MOCKS_FILEINFOPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/tools/fs.hpp>

namespace aos::fs {

/**
 * File info provider mock.
 */
class FileInfoProviderMock : public FileInfoProviderItf {
public:
    MOCK_METHOD(Error, GetFileInfo, (const String& path, FileInfo& info), (override));
};

} // namespace aos::fs

#endif
