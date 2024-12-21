/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_DOWNLOADER_MOCK_HPP_
#define AOS_DOWNLOADER_MOCK_HPP_

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
