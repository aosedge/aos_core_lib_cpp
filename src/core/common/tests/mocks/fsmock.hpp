/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_FSMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_FSMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/tools/fs.hpp>

namespace aos {
class FSPlatformMock : public fs::FSPlatformItf {
public:
    MOCK_METHOD(RetWithError<StaticString<cFilePathLen>>, GetMountPoint, (const String& dir), (const, override));
    MOCK_METHOD(RetWithError<size_t>, GetTotalSize, (const String& dir), (const, override));
    MOCK_METHOD(RetWithError<size_t>, GetDirSize, (const String& dir), (const, override));
    MOCK_METHOD(RetWithError<size_t>, GetAvailableSize, (const String& dir), (const, override));
    MOCK_METHOD(Error, SetUserQuota, (const String& path, size_t quota, size_t uid), (const, override));
};
} // namespace aos

#endif
