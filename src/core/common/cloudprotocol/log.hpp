/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_LOG_HPP_
#define AOS_CLOUDPROTOCOL_LOG_HPP_

#include <cstdint>

#include "aos/common/cloudprotocol/cloudprotocol.hpp"
#include "aos/common/tools/optional.hpp"
#include "aos/common/types.hpp"

namespace aos::cloudprotocol {
/**
 * Log id len.
 */
constexpr auto cLogIDLen = AOS_CONFIG_CLOUDPROTOCOL_LOG_ID_LEN;

/**
 * Log content len.
 */
constexpr auto cLogContentLen = AOS_CONFIG_CLOUDPROTOCOL_LOG_CONTENT_LEN;

/**
 * Log type type.
 */
class LogTypeType {
public:
    enum class Enum {
        eSystemLog,
        eServiceLog,
        eCrashLog,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sLogTypeStrings[] = {
            "systemLog",
            "serviceLog",
            "crashLog",
        };

        return Array<const char* const>(sLogTypeStrings, ArraySize(sLogTypeStrings));
    };
};

using LogTypeEnum = LogTypeType::Enum;
using LogType     = EnumStringer<LogTypeType>;

/**
 * Log upload type type.
 */
class LogUploadTypeType {
public:
    enum class Enum {
        eHTTPS,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "https",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using LogUploadTypeEnum = LogUploadTypeType::Enum;
using LogUploadType     = EnumStringer<LogUploadTypeType>;

/**
 * Log upload options.
 */
struct LogUploadOptions {
    LogUploadType                 mType;
    StaticString<cURLLen>         mURL;
    StaticString<cBearerTokenLen> mBearerToken;
    Optional<Time>                mBearerTokenTTL;

    /**
     * Compares log upload options.
     *
     * @param options log upload options to compare with.
     * @return bool.
     */
    bool operator==(const LogUploadOptions& options) const
    {
        return mType == options.mType && mURL == options.mURL && mBearerToken == options.mBearerToken
            && mBearerTokenTTL == options.mBearerTokenTTL;
    }

    /**
     * Compares log upload options.
     *
     * @param options log upload options to compare with.
     * @return bool.
     */
    bool operator!=(const LogUploadOptions& options) const { return !operator==(options); }
};

/**
 * Log filter.
 */
struct LogFilter {
    Optional<Time>                                      mFrom;
    Optional<Time>                                      mTill;
    StaticArray<StaticString<cNodeIDLen>, cMaxNumNodes> mNodeIDs;
    InstanceFilter                                      mInstanceFilter;

    /**
     * Compares log filter.
     *
     * @param filter log filter to compare with.
     * @return bool.
     */
    bool operator==(const LogFilter& filter) const
    {
        return mFrom == filter.mFrom && mTill == filter.mTill && mNodeIDs == filter.mNodeIDs
            && mInstanceFilter == filter.mInstanceFilter;
    }

    /**
     * Compares log filter.
     *
     * @param filter log filter to compare with.
     * @return bool.
     */
    bool operator!=(const LogFilter& filter) const { return !operator==(filter); }
};

/**
 * Log status.
 */
struct RequestLog {
    StaticString<cLogIDLen>    mLogID;
    LogType                    mLogType;
    LogFilter                  mFilter;
    Optional<LogUploadOptions> mUploadOptions;

    /**
     * Compares log request.
     *
     * @param request log request to compare with.
     * @return bool.
     */
    bool operator==(const RequestLog& request) const
    {
        return mLogID == request.mLogID && mLogType == request.mLogType && mFilter == request.mFilter
            && mUploadOptions == request.mUploadOptions;
    }

    /**
     * Compares log request.
     *
     * @param request log request to compare with.
     * @return bool.
     */
    bool operator!=(const RequestLog& request) const { return !operator==(request); }
};

/**
 * Log status type.
 */
class LogStatusType {
public:
    enum class Enum {
        eOk,
        eError,
        eEmpty,
        eAbsent,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sLogStatusStrings[] = {
            "ok",
            "error",
            "empty",
            "absent",
        };

        return Array<const char* const>(sLogStatusStrings, ArraySize(sLogStatusStrings));
    };
};

using LogStatusEnum = LogStatusType::Enum;
using LogStatus     = EnumStringer<LogStatusType>;

/**
 * Push log.
 */
struct PushLog {
    StaticString<cNodeIDLen>     mNodeID;
    StaticString<cLogIDLen>      mLogID;
    uint64_t                     mPartsCount;
    uint64_t                     mPart;
    StaticString<cLogContentLen> mContent;
    LogStatus                    mStatus;
    Error                        mErrorInfo;

    /**
     * Compares push log.
     *
     * @param log push log to compare with.
     * @return bool.
     */
    bool operator==(const PushLog& log) const
    {
        return mNodeID == log.mNodeID && mLogID == log.mLogID && mPartsCount == log.mPartsCount && mPart == log.mPart
            && mContent == log.mContent && mStatus == log.mStatus && mErrorInfo == log.mErrorInfo;
    }

    /**
     * Compares push log.
     *
     * @param log push log to compare with.
     * @return bool.
     */
    bool operator!=(const PushLog& log) const { return !operator==(log); }
};

} // namespace aos::cloudprotocol

#endif
