/**
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_STATE_HPP_
#define AOS_CORE_COMMON_TYPES_STATE_HPP_

#include <core/common/crypto/itf/hash.hpp>

#include "common.hpp"

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
struct NewState : public Protocol, public InstanceIdent {
    StaticArray<uint8_t, crypto::cSHA256Size> mChecksum;
    StaticString<cStateLen>                   mState;

    /**
     * Compares new state.
     *
     * @param rhs new state to compare with.
     * @return bool.
     */
    friend bool operator==(const NewState& lhs, const NewState& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && (static_cast<const InstanceIdent&>(lhs) == rhs)
            && lhs.mChecksum == rhs.mChecksum && lhs.mState == rhs.mState;
    };

    /**
     * Compares new state.
     *
     * @param rhs new state to compare with.
     * @return bool.
     */
    friend bool operator!=(const NewState& lhs, const NewState& rhs) { return !(lhs == rhs); };
};

/**
 * Update state.
 */
struct UpdateState : public Protocol, public InstanceIdent {
    StaticArray<uint8_t, crypto::cSHA256Size> mChecksum;
    StaticString<cStateLen>                   mState;

    /**
     * Compares update state.
     *
     * @param rhs update state to compare with.
     * @return bool.
     */
    friend bool operator==(const UpdateState& lhs, const UpdateState& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && (static_cast<const InstanceIdent&>(lhs) == rhs)
            && lhs.mChecksum == rhs.mChecksum && lhs.mState == rhs.mState;
    };

    /**
     * Compares update state.
     *
     * @param rhs update state to compare with.
     * @return bool.
     */
    friend bool operator!=(const UpdateState& lhs, const UpdateState& rhs) { return !(lhs == rhs); };
};

/**
 * State acceptance.
 */
struct StateAcceptance : public Protocol, public InstanceIdent {
    StaticArray<uint8_t, crypto::cSHA256Size> mChecksum;
    StateResult                               mResult;
    StaticString<cStateReason>                mReason;

    /**
     * Compares state acceptance.
     *
     * @param rhs state acceptance to compare with.
     * @return bool.
     */
    friend bool operator==(const StateAcceptance& lhs, const StateAcceptance& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && (static_cast<const InstanceIdent&>(lhs) == rhs)
            && lhs.mChecksum == rhs.mChecksum && lhs.mResult == rhs.mResult && lhs.mReason == rhs.mReason;
    };

    /**
     * Compares state acceptance.
     *
     * @param rhs state acceptance to compare with.
     * @return bool.
     */
    friend bool operator!=(const StateAcceptance& lhs, const StateAcceptance& rhs) { return !(lhs == rhs); };
};

/**
 * State request.
 */
struct StateRequest : public Protocol, public InstanceIdent {
    bool mDefault {};

    /**
     * Compares state request.
     *
     * @param rhs state request to compare with.
     * @return bool.
     */
    friend bool operator==(const StateRequest& lhs, const StateRequest& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && (static_cast<const InstanceIdent&>(lhs) == rhs)
            && lhs.mDefault == rhs.mDefault;
    };

    /**
     * Compares state request.
     *
     * @param rhs state request to compare with.
     * @return bool.
     */
    friend bool operator!=(const StateRequest& lhs, const StateRequest& rhs) { return !(lhs == rhs); };
};

} // namespace aos

#endif
