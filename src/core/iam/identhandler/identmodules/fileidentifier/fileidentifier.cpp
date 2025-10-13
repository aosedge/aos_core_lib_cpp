/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/fs.hpp>
#include <core/common/tools/logger.hpp>
#include <core/common/tools/memory.hpp>

#include "fileidentifier.hpp"

namespace aos::iam::identhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error FileIdentifier::Init(const Config& config)
{
    mConfig = config;
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
        LOG_WRN() << "Can't read subjects: err=" << err << ". Empty subjects will be used";
    }

    return ErrorEnum::eNone;
}

RetWithError<StaticString<cIDLen>> FileIdentifier::GetSystemID()
{
    return mSystemId;
}

RetWithError<StaticString<cUnitModelLen>> FileIdentifier::GetUnitModel()
{
    return mUnitModel;
}

Error FileIdentifier::GetSubjects(Array<StaticString<cIDLen>>& subjects)
{
    if (subjects.MaxSize() < mSubjects.Size()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    subjects = mSubjects;

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error FileIdentifier::ReadSystemId()
{
    const auto err = fs::ReadFileToString(mConfig.systemIDPath, mSystemId);

    return AOS_ERROR_WRAP(err);
}

Error FileIdentifier::ReadUnitModel()
{
    const auto err = fs::ReadFileToString(mConfig.unitModelPath, mUnitModel);

    return AOS_ERROR_WRAP(err);
}

Error FileIdentifier::ReadSubjects()
{
    StaticString<cMaxNumSubjects * cIDLen> buffer;

    auto err = fs::ReadFileToString(mConfig.subjectsPath, buffer);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = buffer.Split(mSubjects, '\n');
    if (!err.IsNone()) {
        mSubjects.Clear();

        return AOS_ERROR_WRAP(err);
    }

    if (mSubjectsListener) {
        mSubjectsListener->SubjectsChanged(mSubjects);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::iam::identhandler
