/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_IDENTHANDLER_IDENTMODULES_FILEIDENTIFIER_FILEIDENTIFIER_HPP_
#define AOS_CORE_IAM_IDENTHANDLER_IDENTMODULES_FILEIDENTIFIER_FILEIDENTIFIER_HPP_

#include <core/common/tools/thread.hpp>
#include <core/iam/config.hpp>
#include <core/iam/identhandler/identmodule.hpp>

#include "config.hpp"

namespace aos::iam::identhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * File identifier.
 */
class FileIdentifier : public IdentModuleItf {
public:
    /**
     * Initializes file identifier.
     *
     * @param config module config.
     * @return Error.
     */
    Error Init(const FileIdentifierConfig& config);

    /**
     * Starts file identifier.
     *
     * @return Error.
     */
    Error Start() override { return ErrorEnum::eNone; };

    /**
     * Stops file identifier.
     *
     * @return Error.
     */
    Error Stop() override { return ErrorEnum::eNone; };

    /**
     * Returns System info.
     *
     * @param[out] info result system info.
     * @returns Error.
     */
    Error GetSystemInfo(SystemInfo& info) override;

    /**
     * Returns subjects.
     *
     * @param[out] subjects result subjects.
     * @returns Error.
     */
    Error GetSubjects(Array<StaticString<cIDLen>>& subjects) override;

private:
    static constexpr auto cModelVersionDelimiter = ';';
    static constexpr auto cMaxNumSubscribers     = 4;

    Error ReadSystemId();
    Error ReadUnitModel();
    Error ReadSubjects();

    Mutex                                              mMutex;
    FileIdentifierConfig                               mConfig;
    SystemInfo                                         mSystemInfo;
    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> mSubjects;
};

/** @}*/

} // namespace aos::iam::identhandler

#endif
