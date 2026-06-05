/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_IDENTHANDLER_IDENTMODULES_FILEIDENTIFIER_CONFIG_HPP_
#define AOS_CORE_IAM_IDENTHANDLER_IDENTMODULES_FILEIDENTIFIER_CONFIG_HPP_

#include <core/common/consts.hpp>
#include <core/common/tools/string.hpp>

namespace aos::iam::identhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * FileIdentifier configuration.
 */
struct FileIdentifierConfig {
    /**
     * System ID path.
     */
    StaticString<cFilePathLen> mSystemIDPath;

    /**
     * Unit model path.
     */
    StaticString<cFilePathLen> mUnitModelPath;

    /**
     * Subjects path.
     */
    StaticString<cFilePathLen> mSubjectsPath;
};

/** @}*/

} // namespace aos::iam::identhandler

#endif
