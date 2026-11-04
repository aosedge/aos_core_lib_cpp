/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_DOWNLOADER_ITF_DOWNLOADER_HPP_
#define AOS_CORE_COMMON_DOWNLOADER_ITF_DOWNLOADER_HPP_

#include <core/common/consts.hpp>
#include <core/common/tools/array.hpp>
#include <core/common/tools/cancelcontext.hpp>
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
     * Downloads file. Tries the URLs in order until the download succeeds. Each URL is retried with the downloader
     * retry policy, then the next URL is tried. A partially downloaded file is resumed if the server supports range
     * requests. The download checks the cancel context periodically and returns eCanceled if it is canceled.
     *
     * @param ctx cancel context.
     * @param digest file digest.
     * @param urls file URLs.
     * @param path path to file.
     * @return Error: eCanceled if the download is canceled by the cancel context, eInvalidArgument if urls is empty,
     * eAlreadyExist if a download of the same digest is in progress, otherwise the error of the last attempt.
     */
    virtual Error Download(
        CancelContext& ctx, const String& digest, const Array<StaticString<cURLLen>>& urls, const String& path)
        = 0;
};

} // namespace aos::downloader

#endif
