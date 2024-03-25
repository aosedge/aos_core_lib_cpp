/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_FILEIDENTIFIER_HPP_
#define AOS_FILEIDENTIFIER_HPP_

#include "aos/common/types.hpp"
#include "aos/iam/config.hpp"
#include "aos/iam/identhandler.hpp"

namespace aos {
namespace iam {
namespace identhandler {

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
    StaticString<cFilePathLen> systemIDPath;

    /**
     * Unit model path.
     */
    StaticString<cFilePathLen> unitModelPath;

    /**
     * Subjects path.
     */
    StaticString<cFilePathLen> subjectsPath;
};

/**
 * File identifier.
 */
class FileIdentifier : public IdentHandlerItf {
public:
    /**
     * Initializes file identifier.
     *
     * @param config module config.
     * @param subjectsObserver subject observer.
     * @return Error.
     */
    Error Init(const Config& config, SubjectsObserverItf& subjectsObserver);

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
    SubjectsObserverItf*                                        mSubjectsObserver {};
    StaticString<cSystemIDLen>                                  mSystemId;
    StaticString<cUnitModelLen>                                 mUnitModel;
    StaticArray<StaticString<cSubjectIDLen>, cMaxSubjectIDSize> mSubjects;
};

/** @}*/

} // namespace identhandler
} // namespace iam
} // namespace aos

#endif
