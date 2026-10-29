/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/fs.hpp>

#include "filehwinfoprovider.hpp"

namespace aos::iam::currentnode {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error FileHwInfoProvider::Init(const String& hwIDPath)
{
    if (auto err = fs::ReadFileToString(hwIDPath, mHwID); err != ErrorEnum::eNone) {
        return err;
    }

    mHwID.RightTrim(" \t\r\n");

    if (mHwID.IsEmpty()) {
        return Error(ErrorEnum::eInvalidArgument, "Hardware ID is empty");
    }

    return ErrorEnum::eNone;
}

Error FileHwInfoProvider::GetHardwareID(String& hwID)
{
    return hwID.Assign(mHwID);
}

} // namespace aos::iam::currentnode
