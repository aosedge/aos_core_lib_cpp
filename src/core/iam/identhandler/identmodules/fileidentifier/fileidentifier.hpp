/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_IDENTHANDLER_IDENTMODULES_FILEIDENTIFIER_FILEIDENTIFIER_HPP_
#define AOS_CORE_IAM_IDENTHANDLER_IDENTMODULES_FILEIDENTIFIER_FILEIDENTIFIER_HPP_

#include <core/common/identhandler/identhandler.hpp>
#include <core/common/types/types.hpp>
#include <core/iam/config.hpp>

namespace aos::iam::identhandler::fileidentifier {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * FileIdentifier configuration.
 */
struct Config {
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

/**
 * File identifier.
 */
class FileIdentifier : public aos::identhandler::IdentHandlerItf {
public:
    /**
     * Initializes file identifier.
     *
     * @param config module config.
     * @param subjectsObserver subject observer.
     * @return Error.
     */
    Error Init(const Config& config, aos::identhandler::SubjectsObserverItf& subjectsObserver);

    /**
     * Returns System ID.
     *
     * @returns RetWithError<StaticString>.
     */
    RetWithError<StaticString<cSystemIDLen>> GetSystemID() override;

    /**
     * Returns unit model.
     *
     * @returns RetWithError<StaticString>.
     */
    RetWithError<StaticString<cUnitModelLen>> GetUnitModel() override;

    /**
     * Returns subjects.
     *
     * @param[out] subjects result subjects.
     * @returns Error.
     */
    Error GetSubjects(Array<StaticString<cSubjectIDLen>>& subjects) override;

    /**
     * Destroys ident handler interface.
     */
    ~FileIdentifier() = default;

private:
    Error ReadSystemId();
    Error ReadUnitModel();
    Error ReadSubjects();

    Config                                                      mConfig {};
    aos::identhandler::SubjectsObserverItf*                     mSubjectsObserver {};
    StaticString<cSystemIDLen>                                  mSystemId;
    StaticString<cUnitModelLen>                                 mUnitModel;
    StaticArray<StaticString<cSubjectIDLen>, cMaxSubjectIDSize> mSubjects;
};

/** @}*/

} // namespace aos::iam::identhandler::fileidentifier

#endif
