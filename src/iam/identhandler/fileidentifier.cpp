/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/iam/identhandler/fileidentifier.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/memory.hpp"
#include "log.hpp"

namespace aos {
namespace iam {
namespace identhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error FileIdenentifier::Init(const Config& config, SubjectsObserverItf& subjectsObserver)
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
        LOG_WRN() << "Can't read subjects: " << err.Message() << ". Empty subjects will be used";
    }

    return ErrorEnum::eNone;
}

RetWithError<StaticString<cSystemIDLen>> FileIdenentifier::GetSystemID()
{
    return mSystemId;
}

RetWithError<StaticString<cUnitModelLen>> FileIdenentifier::GetUnitModel()
{
    return mUnitModel;
}

Error FileIdenentifier::GetSubjects(Array<StaticString<cSubjectIDLen>>& subjects)
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

Error FileIdenentifier::ReadSystemId()
{
    const auto err = FS::ReadFileToString(mConfig.systemIDPath, mSystemId);

    return AOS_ERROR_WRAP(err);
}

Error FileIdenentifier::ReadUnitModel()
{
    const auto err = FS::ReadFileToString(mConfig.unitModelPath, mUnitModel);

    return AOS_ERROR_WRAP(err);
}

Error FileIdenentifier::ReadSubjects()
{
    StaticString<cMaxSubjectIDSize * cSubjectIDLen> buffer;

    auto err = FS::ReadFileToString(mConfig.subjectsPath, buffer);
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

} // namespace identhandler
} // namespace iam
} // namespace aos
