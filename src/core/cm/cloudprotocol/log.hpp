/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_CLOUDPROTOCOL_LOG_HPP_
#define AOS_CORE_CM_CLOUDPROTOCOL_LOG_HPP_

#include <cstdint>

#include <core/common/tools/optional.hpp>
#include <core/common/types/types.hpp>

#include "common.hpp"

namespace aos::cloudprotocol {

/**
 * Log filter.
 */
struct LogFilter : public InstanceFilter {
    Optional<Time>                      mFrom;
    Optional<Time>                      mTill;
    StaticArray<Identity, cMaxNumNodes> mNodes;

    /**
     * Compares log filter.
     *
     * @param filter log filter to compare with.
     * @return bool.
     */
    bool operator==(const LogFilter& filter) const
    {
        return InstanceFilter::operator==(filter) && mFrom == filter.mFrom && mTill == filter.mTill
            && mNodes == filter.mNodes;
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
 * Log request.
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
 * Push log.
 */
struct PushLog {
    StaticString<cLogIDLen>      mLogID;
    Identity                     mNode;
    uint64_t                     mPartsCount;
    uint64_t                     mPart;
    StaticString<cLogContentLen> mContent;
    LogStatus                    mStatus;
    Error                        mError;

    /**
     * Compares push log.
     *
     * @param log push log to compare with.
     * @return bool.
     */
    bool operator==(const PushLog& log) const
    {
        return mNode == log.mNode && mLogID == log.mLogID && mPartsCount == log.mPartsCount && mPart == log.mPart
            && mContent == log.mContent && mStatus == log.mStatus && mError == log.mError;
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
