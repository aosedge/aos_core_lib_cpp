/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_DOWNLOADER_HPP_
#define AOS_DOWNLOADER_HPP_

#include "aos/common/enum.hpp"
#include "aos/common/error.hpp"
#include "aos/common/string.hpp"

namespace aos {

/**
 * Download content type.
 */
class DownloadContentType {
public:
    enum class Enum { eService };

    static const Array<const char* const> GetStrings()
    {
        static const char* const cContentTypeStrings[] = {"service"};

        return Array<const char* const>(cContentTypeStrings, ArraySize(cContentTypeStrings));
    };
};

using DownloadContentEnum = DownloadContentType::Enum;
using DownloadContent = EnumStringer<DownloadContentType>;

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
     * @param contentType content type.
     * @return Error.
     */
    virtual Error Download(const String& url, const String& path, DownloadContent contentType) = 0;

    /**
     * Destroys the Downloader Itf object.
     */
    virtual ~DownloaderItf() = default;
};

} // namespace aos

#endif
