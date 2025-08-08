/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/fs.hpp>
#include <core/common/tools/log.hpp>
#include <core/common/tools/logger.hpp>
#include <core/common/tools/memory.hpp>

#include "fileidentifier.hpp"

namespace aos::iam::identhandler::fileidentifier {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error FileIdentifier::Init(const Config& config, aos::identhandler::SubjectsObserverItf& subjectsObserver)
{
    mConfig           = config;
    mSubjectsObserver = &subjectsObserver;
    mSubjects.Clear();

    auto err = ReadSystemId();
    if (!err.IsNone()) {
        return err;
    }

    err = ReadUnitModel();
    if (!err.IsNone()) {
        return err;
    }

    err = ReadSubjects();
    if (!err.IsNone()) {
        LOG_WRN() << "Can't read subjects, empty subjects will be used" << Log::Field(err);

        mSubjects.Clear();
    }

    return ErrorEnum::eNone;
}

RetWithError<StaticString<cSystemIDLen>> FileIdentifier::GetSystemID()
{
    return mSystemId;
}

RetWithError<StaticString<cUnitModelLen>> FileIdentifier::GetUnitModel()
{
    return mUnitModel;
}

Error FileIdentifier::GetSubjects(Array<StaticString<cSubjectIDLen>>& subjects)
{
    if (subjects.MaxSize() < mSubjects.Size()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    return subjects.Assign(mSubjects);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error FileIdentifier::ReadSystemId()
{
    const auto err = fs::ReadFileToString(mConfig.mSystemIDPath, mSystemId);

    return AOS_ERROR_WRAP(err);
}

Error FileIdentifier::ReadUnitModel()
{
    const auto err = fs::ReadFileToString(mConfig.mUnitModelPath, mUnitModel);

    return AOS_ERROR_WRAP(err);
}

Error FileIdentifier::ReadSubjects()
{
    StaticString<cMaxSubjectIDSize * cSubjectIDLen> buffer;

    auto err = fs::ReadFileToString(mConfig.mSubjectsPath, buffer);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = buffer.Split(mSubjects, '\n');
    if (!err.IsNone()) {
        mSubjects.Clear();

        return AOS_ERROR_WRAP(err);
    }

    mSubjectsObserver->SubjectsChanged(mSubjects);

    return ErrorEnum::eNone;
}

} // namespace aos::iam::identhandler::fileidentifier
