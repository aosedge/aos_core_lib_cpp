/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DOWNLOADER_MOCK_HPP_
#define DOWNLOADER_MOCK_HPP_

#include <gmock/gmock.h>

#include <aos/common/downloader/downloader.hpp>

namespace aos::downloader {

/**
 * Downloader mock.
 */
class DownloaderMock : public DownloaderItf {
public:
    MOCK_METHOD(Error, Download, (const String&, const String&, DownloadContent), (override));
};

} // namespace aos::downloader

#endif
