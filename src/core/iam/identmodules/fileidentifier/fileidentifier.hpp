/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_IDENTMODULES_FILEIDENTIFIER_FILEIDENTIFIER_HPP_
#define AOS_CORE_IAM_IDENTMODULES_FILEIDENTIFIER_FILEIDENTIFIER_HPP_

#include <core/common/identprovider/itf/identprovider.hpp>
#include <core/common/types/types.hpp>
#include <core/iam/config.hpp>

namespace aos::iam::identmodules::fileidentifier {

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
class FileIdentifier : public identprovider::IdentProviderItf {
public:
    /**
     * Initializes file identifier.
     *
     * @param config module config.
     * @return Error.
     */
    Error Init(const Config& config);

    /**
     * Returns System ID.
     *
     * @returns RetWithError<StaticString>.
     */
    RetWithError<StaticString<cIDLen>> GetSystemID() override;

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
    Error GetSubjects(Array<StaticString<cIDLen>>& subjects) override;

    /**
     * Subscribes to subjects changed events.
     *
     * @param observer subjects observer.
     * @returns Error.
     */
    Error SubscribeSubjectsChanged(identprovider::SubjectsObserverItf& observer) override;

    /**
     * Unsubscribes from subjects changed events.
     *
     * @param observer subjects observer.
     */
    void UnsubscribeSubjectsChanged(identprovider::SubjectsObserverItf& observer) override;

private:
    Error ReadSystemId();
    Error ReadUnitModel();
    Error ReadSubjects();

    Config                                             mConfig {};
    StaticString<cIDLen>                               mSystemId;
    StaticString<cIDLen>                               mUnitModel;
    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> mSubjects;
};

/** @}*/

} // namespace aos::iam::identmodules::fileidentifier

#endif
