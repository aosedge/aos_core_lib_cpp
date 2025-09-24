/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_COMMON_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_COMMON_HPP_

#include <core/common/tools/enum.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/tools/uuid.hpp>
#include <core/common/types/types.hpp>

namespace aos::cloudprotocol {

/**
 * Identifier URN  len.
 */
constexpr auto cURNLen = AOS_CONFIG_CLOUDPROTOCOL_URN_LEN;

/**
 * Identifier codename len.
 */
constexpr auto cCodeNameLen = AOS_CONFIG_CLOUDPROTOCOL_CODENAME_LEN;

/**
 * Identifier title len.
 */

constexpr auto cTitleLen = AOS_CONFIG_CLOUDPROTOCOL_TITLE_LEN;

/**
 * Identifier description len.
 */

constexpr auto cDescriptionLen = AOS_CONFIG_CLOUDPROTOCOL_DESCRIPTION_LEN;

/**
 * Aos identity.
 */
struct Identity {
    Optional<StaticString<cIDLen>>          mID;
    Optional<UpdateItemType>                mType;
    Optional<StaticString<cCodeNameLen>>    mCodeName;
    Optional<StaticString<cTitleLen>>       mTitle;
    Optional<StaticString<cDescriptionLen>> mDescription;
    Optional<StaticString<cURNLen>>         mURN;

    /**
     * Compares identities.
     *
     * @param other identity to compare.
     * @return bool.
     */
    bool operator==(const Identity& other) const
    {
        if (mURN.HasValue() && other.mURN.HasValue()) {
            return mURN.GetValue() == other.mURN.GetValue();
        }

        return mType == other.mType && mCodeName == other.mCodeName;
    }

    /**
     * Compares identities.
     *
     * @param other identity to compare.
     * @return bool.
     */
    bool operator!=(const Identity& other) const { return !operator==(other); }
};

/**
 * Instance identity.
 */
struct InstanceIdent {
    Identity mIdentity;
    Identity mSubject;
    uint64_t mInstance = 0;

    /**
     * Compares instance identities.
     *
     * @param other instance identity to compare.
     * @return bool.
     */
    bool operator==(const InstanceIdent& other) const
    {
        return mIdentity == other.mIdentity && mSubject == other.mSubject && mInstance == other.mInstance;
    }

    /**
     * Compares instance identities.
     *
     * @param other instance identity to compare.
     * @return bool.
     */
    bool operator!=(const InstanceIdent& other) const { return !operator==(other); }
};

/**
 * Instance filter.
 */
struct InstanceFilter {
    Optional<Identity> mIdentity;
    Optional<Identity> mSubject;
    Optional<uint64_t> mInstance;

    /**
     * Compares instance filters.
     *
     * @param other instance filter to compare.
     * @return bool.
     */
    bool operator==(const InstanceFilter& other) const
    {
        return mIdentity == other.mIdentity && mSubject == other.mSubject && mInstance == other.mInstance;
    }

    /**
     * Compares instance filters.
     *
     * @param other instance filter to compare.
     * @return bool.
     */
    bool operator!=(const InstanceFilter& other) const { return !operator==(other); }
};

} // namespace aos::cloudprotocol

#endif
