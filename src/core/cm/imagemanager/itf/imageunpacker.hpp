/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGEUNPACKER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGEUNPACKER_HPP_

#include <core/common/tools/string.hpp>

namespace aos::cm::imagemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Image unpacker interface.
 */
class ImageUnpackerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageUnpackerItf() = default;

    /**
     * Returns the size of the uncompressed file in the archive.
     *
     * @param path path to the archive file.
     * @param filePath path to the file in the archive.
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> GetUncompressedFileSize(const String& path, const String& filePath) = 0;

    /**
     * Extracts a file from an archive.
     *
     * @param archivePath path to the archive file.
     * @param filePath path to the file in the archive.
     * @param outputPath path to the output file.
     * @return Error.
     */
    virtual Error ExtractFileFromArchive(const String& archivePath, const String& filePath, const String& outputPath)
        = 0;
};

} // namespace aos::cm::imagemanager

#endif // AOS_CORE_CM_IMAGEMANAGER_ITF_IMAGEUNPACKER_HPP_
