/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_TYPES_HPP_
#define AOS_TYPES_HPP_

#include <cstdint>

#include "aos/common/config/types.hpp"
#include "aos/common/error.hpp"
#include "aos/common/stringer.hpp"

namespace aos {

/**
 * Instance identification.
 */
struct InstanceIdent {
    char     mServiceID[AOS_CONFIG_SERVICE_ID_LEN + 1];
    char     mSubjectID[AOS_CONFIG_SUBJECT_ID_LEN + 1];
    uint64_t mInstance;
};

/**
 * Instance info.
 */
struct InstanceInfo {
    InstanceIdent mInstanceIdent;
    uint32_t      mUID;
    uint64_t      mPriority;
    char          mStoragePath[AOS_CONFIG_FILE_PATH_LEN + 1];
    char          mStatePath[AOS_CONFIG_FILE_PATH_LEN + 1];
};

/**
 * Instance run state.
 */
class InstanceRunStateType {
public:
    enum class Enum { eActive, eFailed, eNumStates };

    static Pair<const char* const*, size_t> GetStrings()
    {
        static const char* const cInstanceRunStateStrings[static_cast<size_t>(Enum::eNumStates)] = {"active", "failed"};

        return Pair<const char* const*, size_t>(cInstanceRunStateStrings, static_cast<size_t>(Enum::eNumStates));
    };
};

using InstanceRunStateEnum = InstanceRunStateType::Enum;
using InstanceRunState = EnumStringer<InstanceRunStateType>;

/**
 * Instance status.
 */
struct InstanceStatus {
    InstanceIdent    mInstanceIdent;
    uint64_t         mAosVersion;
    uint8_t          mStateCheckSum[AOS_CONFIG_STATE_CHECKSUM_LEN];
    InstanceRunState mRunState;
    Error            mError;
};

} // namespace aos

#endif
