/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_HPP_
#define AOS_CLOUDPROTOCOL_HPP_

#include "aos/common/tools/optional.hpp"
#include "aos/common/types.hpp"

namespace aos::cloudprotocol {

/**
 * Instance filter.
 */
struct InstanceFilter {
    Optional<StaticString<cServiceIDLen>> mServiceID;
    Optional<StaticString<cSubjectIDLen>> mSubjectID;
    Optional<uint64_t>                    mInstance;

    /**
     * Returns true if instance ident matches filter.
     *
     * @param instanceIdent instance ident to match.
     * @return bool.
     */
    bool Match(const InstanceIdent& instanceIdent) const
    {
        return (!mServiceID.HasValue() || *mServiceID == instanceIdent.mServiceID)
            && (!mSubjectID.HasValue() || *mSubjectID == instanceIdent.mSubjectID)
            && (!mInstance.HasValue() || *mInstance == instanceIdent.mInstance);
    }

    /**
     * Compares instance filter.
     *
     * @param filter instance filter to compare with.
     * @return bool.
     */
    bool operator==(const InstanceFilter& filter) const
    {
        return mServiceID == filter.mServiceID && mSubjectID == filter.mSubjectID && mInstance == filter.mInstance;
    }

    /**
     * Compares instance filter.
     *
     * @param filter instance filter to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceFilter& filter) const { return !operator==(filter); }

    /**
     * Outputs instance filter to log.
     *
     * @param log log to output.
     * @param instanceFilter instance filter.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const InstanceFilter& instanceFilter)
    {
        StaticString<32> instanceStr = "*";

        if (instanceFilter.mInstance.HasValue()) {
            instanceStr.Convert(*instanceFilter.mInstance);
        }

        return log << "{" << (instanceFilter.mServiceID.HasValue() ? *instanceFilter.mServiceID : "*") << ":"
                   << (instanceFilter.mSubjectID.HasValue() ? *instanceFilter.mSubjectID : "*") << ":" << instanceStr
                   << "}";
    }
};

} // namespace aos::cloudprotocol

#endif
