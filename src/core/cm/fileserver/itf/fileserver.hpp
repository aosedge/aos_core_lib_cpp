/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_FILESERVER_HPP_
#define AOS_CORE_CM_FILESERVER_HPP_

#include <core/common/tools/string.hpp>

namespace aos::cm::fileserver {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * File server interface.
 */
class FileServerItf {
public:
    /**
     * Destructor.
     */
    virtual ~FileServerItf() = default;

    /**
     * Translates file path URL.
     *
     * @param filePath input file path.
     * @param[out] outURL translated URL.
     * @return Error.
     */
    virtual Error TranslateFilePathURL(const String& filePath, String& outURL) = 0;
};

} // namespace aos::cm::fileserver

#endif // AOS_CORE_CM_FILESERVER_HPP_
