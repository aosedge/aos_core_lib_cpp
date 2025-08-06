/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_FILESYSTEM_MOCK_HPP_
#define AOS_FILESYSTEM_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/common/tools/fs.hpp"

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
