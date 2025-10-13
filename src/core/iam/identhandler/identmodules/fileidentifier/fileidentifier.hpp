/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_IDENTHANDLER_IDENTMODULES_FILEIDENTIFIER_FILEIDENTIFIER_HPP_
#define AOS_CORE_IAM_IDENTHANDLER_IDENTMODULES_FILEIDENTIFIER_FILEIDENTIFIER_HPP_

#include <core/iam/config.hpp>
#include <core/iam/identhandler/identmodule.hpp>

namespace aos::iam::identhandler {

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
class FileIdentifier : public IdentModuleItf {
public:
    /**
     * Initializes file identifier.
     *
     * @param config module config.
     * @return Error.
     */
    Error Init(const Config& config);

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
     * Subscribes subjects listener.
     *
     * @param subjectsListener subjects listener.
     * @returns Error.
     */
    Error SubscribeListener(iamclient::SubjectsListenerItf& subjectsListener) override
    {
        mSubjectsListener = &subjectsListener;

        return ErrorEnum::eNone;
    }

    /**
     * Unsubscribes subjects listener.
     *
     * @param subjectsListener subjects listener.
     * @returns Error.
     */
    Error UnsubscribeListener(iamclient::SubjectsListenerItf& subjectsListener) override
    {
        (void)subjectsListener;

        mSubjectsListener = nullptr;

        return ErrorEnum::eNone;
    }

private:
    Error ReadSystemId();
    Error ReadUnitModel();
    Error ReadSubjects();

    Config                                             mConfig {};
    iamclient::SubjectsListenerItf*                    mSubjectsListener {};
    StaticString<cIDLen>                               mSystemId;
    StaticString<cIDLen>                               mUnitModel;
    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> mSubjects;
};

/** @}*/

} // namespace aos::iam::identhandler

#endif
