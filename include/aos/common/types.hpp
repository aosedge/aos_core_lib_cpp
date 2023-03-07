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

/*
 * Service ID len.
 */
constexpr auto cServiceIDLen = AOS_CONFIG_TYPES_SERVICE_ID_LEN;

/*
 * Subject ID len.
 */
constexpr auto cSubjectIDLen = AOS_CONFIG_TYPES_SUBJECT_ID_LEN;

/*
 * Instance ID len.
 */
constexpr auto cInstanceIDLen = AOS_CONFIG_TYPES_INSTANCE_ID_LEN;

/*
 * File path len.
 */
constexpr auto cFilePathLen = AOS_CONFIG_TYPES_FILE_PATH_LEN;

/*
 * Instance state check sum len.
 */
constexpr auto cStateCheckSumLen = AOS_CONFIG_TYPES_STATE_CHECKSUM_LEN;

/**
 * Instance identification.
 */
struct InstanceIdent {
    char     mServiceID[cServiceIDLen + 1];
    char     mSubjectID[cSubjectIDLen + 1];
    uint64_t mInstance;
};

/**
 * Instance info.
 */
struct InstanceInfo {
    InstanceIdent mInstanceIdent;
    uint32_t      mUID;
    uint64_t      mPriority;
    char          mStoragePath[cFilePathLen + 1];
    char          mStatePath[cFilePathLen + 1];
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
    uint8_t          mStateCheckSum[cStateCheckSumLen];
    InstanceRunState mRunState;
    Error            mError;
};

} // namespace aos

#endif
