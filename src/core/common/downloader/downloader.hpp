/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_DOWNLOADER_DOWNLOADER_HPP_
#define AOS_CORE_COMMON_DOWNLOADER_DOWNLOADER_HPP_

#include <core/common/cloudprotocol/alerts.hpp>
#include <core/common/tools/enum.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/string.hpp>

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
     * @param targetType target type.
     * @param targetID target ID.
     * @param version version.
     * @return Error.
     */
    virtual Error Download(const String& url, const String& path, UpdateItemType targetType,
        const String& targetID = "", const String& version = "")
        = 0;

    /**
     * Destroys the Downloader Itf object.
     */
    virtual ~DownloaderItf() = default;
};

} // namespace aos::downloader

#endif
