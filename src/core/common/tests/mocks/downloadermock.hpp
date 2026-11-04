/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_MOCKS_DOWNLOADERMOCK_HPP_
#define AOS_CORE_COMMON_TESTS_MOCKS_DOWNLOADERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/downloader/itf/downloader.hpp>

namespace aos::downloader {

/**
 * Downloader mock.
 */
class DownloaderMock : public DownloaderItf {
public:
    MOCK_METHOD(Error, Download, (CancelContext&, const String&, (const Array<StaticString<cURLLen>>&), const String&),
        (override));
};

} // namespace aos::downloader

#endif
