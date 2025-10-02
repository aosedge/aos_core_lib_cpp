/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_DOWNLOADER_DOWNLOADER_HPP_
#define AOS_CORE_COMMON_DOWNLOADER_DOWNLOADER_HPP_

#include <core/common/types/common.hpp>

namespace aos::downloader {

/**
 * Downloader interface.
 */
class DownloaderItf {
public:
    /**
     * Downloads file.
     *
     * @param url URL.
     * @param path path to file.
     * @param imageID image ID.
     * @return Error.
     */
    virtual Error Download(const String& url, const String& path, const String& imageID = "") = 0;

    /**
     * Destroys the Downloader Itf object.
     */
    virtual ~DownloaderItf() = default;
};

} // namespace aos::downloader

#endif
