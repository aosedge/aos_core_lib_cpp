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
 * Max num URLs.
 */
constexpr auto cMaxNumURLs = AOS_CONFIG_CLOUDPROTOCOL_MAX_NUM_URLS;

/**
 * Chain name len.
 */
constexpr auto cChainNameLen = AOS_CONFIG_CLOUDPROTOCOL_CHAIN_NAME_LEN;

/**
 * Bearer token len.
 */
constexpr auto cBearerTokenLen = AOS_CONFIG_CLOUDPROTOCOL_BEARER_TOKEN_LEN;

/**
 * Aos identifier.
 */
struct Identifier {
    Optional<StaticString<cIDLen>>          mID;
    Optional<UpdateItemType>                mType;
    Optional<StaticString<cCodeNameLen>>    mCodeName;
    Optional<StaticString<cTitleLen>>       mTitle;
    Optional<StaticString<cDescriptionLen>> mDescription;
    Optional<StaticString<cURNLen>>         mURN;

    /**
     * Compares identifiers.
     *
     * @param other identifier to compare.
     * @return bool.
     */
    bool operator==(const Identifier& other) const
    {
        if (mURN.HasValue() && other.mURN.HasValue()) {
            return mURN.GetValue() == other.mURN.GetValue();
        }

        return mType == other.mType && mCodeName == other.mCodeName;
    }

    /**
     * Compares identifiers.
     *
     * @param other identifier to compare.
     * @return bool.
     */
    bool operator!=(const Identifier& other) const { return !operator==(other); }
};

} // namespace aos::cloudprotocol

#endif
