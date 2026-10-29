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
#include <core/iam/identhandler/itf/identmodule.hpp>

#include "config.hpp"

namespace aos::iam::identhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * File identifier.
 *
 * Reads unit model, version and subjects from files.
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
     * Returns unit model info.
     *
     * @param[out] info result unit model info.
     * @returns Error.
     */
    Error GetUnitModelInfo(UnitModelInfo& info) override;

    /**
     * Returns subjects.
     *
     * @param[out] subjects result subjects.
     * @returns Error.
     */
    Error GetSubjects(Array<StaticString<cIDLen>>& subjects) override;

private:
    static constexpr auto cModelVersionDelimiter = ';';

    Error ReadUnitModel();
    Error ReadSubjects();

    Mutex                                              mMutex;
    FileIdentifierConfig                               mConfig;
    UnitModelInfo                                      mUnitModelInfo;
    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> mSubjects;
};

/** @}*/

} // namespace aos::iam::identhandler

#endif
