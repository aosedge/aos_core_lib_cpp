/**
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_STATE_HPP_
#define AOS_CORE_COMMON_TYPES_STATE_HPP_

#include <core/common/crypto/itf/hash.hpp>

namespace aos {

/**
 * State length.
 */
constexpr auto cStateLen = AOS_CONFIG_TYPES_STATE_LEN;

/**
 * State reason length.
 */
constexpr auto cStateReason = cErrorMessageLen;

/**
 * State result type.
 */
class StateResultType {
public:
    enum class Enum {
        eAccepted,
        eRejected,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "accepted",
            "rejected",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using StateResultEnum = StateResultType::Enum;
using StateResult     = EnumStringer<StateResultType>;

/**
 * New state.
 */
struct NewState : public InstanceIdent {
    StaticString<crypto::cSHA2DigestSize> mChecksum;
    StaticString<cStateLen>               mState;

    /**
     * Compares new state.
     *
     * @param rhs new state to compare with.
     * @return bool.
     */
    bool operator==(const NewState& rhs) const
    {
        return InstanceIdent::operator==(rhs) && mChecksum == rhs.mChecksum && mState == rhs.mState;
    }

    /**
     * Compares new state.
     *
     * @param rhs new state to compare with.
     * @return bool.
     */
    bool operator!=(const NewState& rhs) const { return !operator==(rhs); }
};

/**
 * Update state.
 */
struct UpdateState : public InstanceIdent {
    StaticString<crypto::cSHA2DigestSize> mChecksum;
    StaticString<cStateLen>               mState;

    /**
     * Compares update state.
     *
     * @param rhs update state to compare with.
     * @return bool.
     */
    bool operator==(const UpdateState& rhs) const
    {
        return InstanceIdent::operator==(rhs) && mChecksum == rhs.mChecksum && mState == rhs.mState;
    }

    /**
     * Compares update state.
     *
     * @param rhs update state to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateState& rhs) const { return !operator==(rhs); }
};

/**
 * State acceptance.
 */
struct StateAcceptance : public InstanceIdent {
    StaticString<crypto::cSHA2DigestSize> mChecksum;
    StateResult                           mResult;
    StaticString<cStateReason>            mReason;

    /**
     * Compares state acceptance.
     *
     * @param rhs state acceptance to compare with.
     * @return bool.
     */
    bool operator==(const StateAcceptance& rhs) const
    {
        return InstanceIdent::operator==(rhs) && mChecksum == rhs.mChecksum && mResult == rhs.mResult
            && mReason == rhs.mReason;
    }

    /**
     * Compares state acceptance.
     *
     * @param rhs state acceptance to compare with.
     * @return bool.
     */
    bool operator!=(const StateAcceptance& rhs) const { return !operator==(rhs); }
};

/**
 * State request.
 */
struct StateRequest {
    InstanceIdent mInstanceIdent;
    bool          mDefault {};

    /**
     * Compares state request.
     *
     * @param rhs state request to compare with.
     * @return bool.
     */
    bool operator==(const StateRequest& rhs) const
    {
        return mInstanceIdent == rhs.mInstanceIdent && mDefault == rhs.mDefault;
    }

    /**
     * Compares state request.
     *
     * @param rhs state request to compare with.
     * @return bool.
     */
    bool operator!=(const StateRequest& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
