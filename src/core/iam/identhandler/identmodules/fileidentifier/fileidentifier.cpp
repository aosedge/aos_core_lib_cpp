/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/fs.hpp>
#include <core/common/tools/logger.hpp>

#include "fileidentifier.hpp"

namespace aos::iam::identhandler {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cWhiteSpaces = "\n\t ";

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error FileIdentifier::Init(const FileIdentifierConfig& config)
{
    LOG_DBG() << "Init file identifier";

    mConfig = config;
    mSubjects.Clear();

    auto err = ReadUnitModel();
    if (!err.IsNone()) {
        return err;
    }

    err = ReadSubjects();
    if (!err.IsNone()) {
        LOG_WRN() << "Can't read subjects: err=" << err << ". Empty subjects will be used";

        mSubjects.Clear();
    }

    return ErrorEnum::eNone;
}

Error FileIdentifier::GetUnitModelInfo(UnitModelInfo& info)
{
    LOG_DBG() << "Get unit model info" << Log::Field("unitModel", mUnitModelInfo.mUnitModel)
              << Log::Field("version", mUnitModelInfo.mVersion);

    info = mUnitModelInfo;

    return ErrorEnum::eNone;
}

Error FileIdentifier::GetSubjects(Array<StaticString<cIDLen>>& subjects)
{
    LOG_DBG() << "Get subjects";

    if (subjects.MaxSize() < mSubjects.Size()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    }

    if (auto err = subjects.Assign(mSubjects); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error FileIdentifier::ReadUnitModel()
{
    StaticString<cUnitModelLen + cVersionLen + 1>                 buffer;
    StaticArray<StaticString<Max(cUnitModelLen, cVersionLen)>, 2> parts;

    if (auto err = fs::ReadFileToString(mConfig.mUnitModelPath, buffer); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = buffer.Split(parts, cModelVersionDelimiter); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (parts.Size() != 2) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    if (auto err = mUnitModelInfo.mUnitModel.Assign(parts[0]); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mUnitModelInfo.mVersion.Assign(parts[1]); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    (void)mUnitModelInfo.mUnitModel.Trim(cWhiteSpaces);
    (void)mUnitModelInfo.mVersion.Trim(cWhiteSpaces);

    return ErrorEnum::eNone;
}

Error FileIdentifier::ReadSubjects()
{
    StaticString<cMaxNumSubjects * cIDLen> buffer;

    auto err = fs::ReadFileToString(mConfig.mSubjectsPath, buffer);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = buffer.Split(mSubjects, '\n');
    if (!err.IsNone()) {
        mSubjects.Clear();

        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::iam::identhandler
