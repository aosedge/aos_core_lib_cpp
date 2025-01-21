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
class HostFSMock : public HostFSItf {
public:
    MOCK_METHOD(RetWithError<StaticString<cFilePathLen>>, GetMountPoint, (const String& dir), (const, override));
    MOCK_METHOD(RetWithError<int64_t>, GetTotalSize, (const String& dir), (const, override));
    MOCK_METHOD(RetWithError<int64_t>, GetDirSize, (const String& dir), (const, override));
    MOCK_METHOD(RetWithError<int64_t>, GetAvailableSize, (const String& dir), (const, override));
};
} // namespace aos

#endif
