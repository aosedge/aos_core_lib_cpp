/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_IMAGEUNPACKER_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_TESTS_MOCKS_IMAGEUNPACKER_HPP_

#include <gmock/gmock.h>

#include <core/cm/imagemanager/itf/imageunpacker.hpp>

namespace aos::cm::imagemanager {

class MockImageUnpacker : public ImageUnpackerItf {
public:
    MOCK_METHOD(
        RetWithError<size_t>, GetUncompressedFileSize, (const String& path, const String& filePath), (override));
    MOCK_METHOD(Error, ExtractFileFromArchive,
        (const String& archivePath, const String& filePath, const String& outputPath), (override));
};

} // namespace aos::cm::imagemanager

#endif
