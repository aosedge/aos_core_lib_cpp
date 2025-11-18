/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_DOWNLOADER_ITF_DOWNLOADER_HPP_
#define AOS_CORE_COMMON_DOWNLOADER_ITF_DOWNLOADER_HPP_

#include <core/common/tools/string.hpp>

namespace aos::downloader {

/**
 * Downloader interface.
 */
class DownloaderItf {
public:
    /**
     * Destroys the Downloader Itf object.
     */
    virtual ~DownloaderItf() = default;

    /**
     * Downloads file.
     *
     * @param digest image digest.
     * @param url URL.
     * @param path path to file.
     * @return Error.
     */
    virtual Error Download(const String& digest, const String& url, const String& path) = 0;

    /**
     * Cancels ongoing download.
     *
     * @param digest image digest.
     *
     * @return Error.
     */
    virtual Error Cancel(const String& digest) = 0;
};

} // namespace aos::downloader

#endif
