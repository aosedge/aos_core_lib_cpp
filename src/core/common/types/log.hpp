/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_LOG_HPP_
#define AOS_CORE_COMMON_TYPES_LOG_HPP_

#include "common.hpp"

namespace aos {

/**
 * Log content len.
 */
constexpr auto cLogContentLen = AOS_CONFIG_TYPES_LOG_CONTENT_LEN;

/**
 * Log type type.
 */
class LogTypeType {
public:
    enum class Enum {
        eSystemLog,
        eInstanceLog,
        eCrashLog,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sLogTypeStrings[] = {
            "systemLog",
            "instanceLog",
            "crashLog",
        };

        return Array<const char* const>(sLogTypeStrings, ArraySize(sLogTypeStrings));
    };
};

using LogTypeEnum = LogTypeType::Enum;
using LogType     = EnumStringer<LogTypeType>;

/**
 * Log status type.
 */
class LogStatusType {
public:
    enum class Enum {
        eOK,
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
 * Log filter.
 */
struct LogFilter : public InstanceFilter {
    Optional<Time>                                  mFrom;
    Optional<Time>                                  mTill;
    StaticArray<StaticString<cIDLen>, cMaxNumNodes> mNodes;

    /**
     * Default constructor.
     */
    LogFilter() = default;

    /**
     * Creates log filter.
     *
     * @param instanceFilter instance filter.
     * @param from optional from time.
     * @param till optional till time.
     */
    LogFilter(const InstanceFilter& instanceFilter, const Optional<Time>& from, const Optional<Time>& till)
        : InstanceFilter(instanceFilter)
        , mFrom(from)
        , mTill(till)
    {
    }

    /**
     * Compares log filter.
     *
     * @param rhs log filter to compare with.
     * @return bool.
     */
    friend bool operator==(const LogFilter& lhs, const LogFilter& rhs)
    {
        return (static_cast<const InstanceFilter&>(lhs) == rhs) && lhs.mFrom == rhs.mFrom && lhs.mTill == rhs.mTill
            && lhs.mNodes == rhs.mNodes;
    };

    /**
     * Compares log filter.
     *
     * @param rhs log filter to compare with.
     * @return bool.
     */
    friend bool operator!=(const LogFilter& lhs, const LogFilter& rhs) { return !(lhs == rhs); };
};

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
     * @param rhs log upload options to compare with.
     * @return bool.
     */
    friend bool operator==(const LogUploadOptions& lhs, const LogUploadOptions& rhs)
    {
        return lhs.mType == rhs.mType && lhs.mURL == rhs.mURL && lhs.mBearerToken == rhs.mBearerToken
            && lhs.mBearerTokenTTL == rhs.mBearerTokenTTL;
    };

    /**
     * Compares log upload options.
     *
     * @param rhs log upload options to compare with.
     * @return bool.
     */
    friend bool operator!=(const LogUploadOptions& lhs, const LogUploadOptions& rhs) { return !(lhs == rhs); };
};

/**
 * Log request.
 */
struct RequestLog : public Protocol {
    LogType                    mLogType;
    LogFilter                  mFilter;
    Optional<LogUploadOptions> mUploadOptions;

    /**
     * Compares log request.
     *
     * @param rhs log request to compare with.
     * @return bool.
     */
    friend bool operator==(const RequestLog& lhs, const RequestLog& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mLogType == rhs.mLogType && lhs.mFilter == rhs.mFilter
            && lhs.mUploadOptions == rhs.mUploadOptions;
    };

    /**
     * Compares log request.
     *
     * @param rhs log request to compare with.
     * @return bool.
     */
    friend bool operator!=(const RequestLog& lhs, const RequestLog& rhs) { return !(lhs == rhs); };
};

/**
 * Push log.
 */
struct PushLog : public Protocol {
    StaticString<cIDLen>         mNodeID;
    uint64_t                     mPartsCount;
    uint64_t                     mPart;
    StaticString<cLogContentLen> mContent;
    LogStatus                    mStatus;
    Error                        mError;

    /**
     * Compares push log.
     *
     * @param rhs push log to compare with.
     * @return bool.
     */
    friend bool operator==(const PushLog& lhs, const PushLog& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mNodeID == rhs.mNodeID
            && lhs.mPartsCount == rhs.mPartsCount && lhs.mPart == rhs.mPart && lhs.mContent == rhs.mContent
            && lhs.mStatus == rhs.mStatus && lhs.mError == rhs.mError;
    };

    /**
     * Compares push log.
     *
     * @param rhs push log to compare with.
     * @return bool.
     */
    friend bool operator!=(const PushLog& lhs, const PushLog& rhs) { return !(lhs == rhs); };
};

} // namespace aos

#endif
