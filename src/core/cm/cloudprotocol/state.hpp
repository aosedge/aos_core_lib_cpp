/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_CLOUDPROTOCOL_STATE_HPP_
#define AOS_CORE_CM_CLOUDPROTOCOL_STATE_HPP_

#include <core/common/crypto/crypto.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/types/types.hpp>

namespace aos::cloudprotocol {

/**
 * State reason length.
 */
static constexpr auto cStateReason = cErrorMessageLen;

/**
 * New state.
 */
struct NewState : public InstanceIdent {
    StaticString<crypto::cSHA2DigestSize> mChecksum;
    StaticString<cStateLen>               mState;

    /**
     * Compares new state.
     *
     * @param state new state to compare with.
     * @return bool.
     */
    bool operator==(const NewState& state) const
    {
        return InstanceIdent::operator==(state) && mChecksum == state.mChecksum && mState == state.mState;
    }
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
     * @param state update state to compare with.
     * @return bool.
     */
    bool operator==(const UpdateState& state) const
    {
        return InstanceIdent::operator==(state) && mChecksum == state.mChecksum && mState == state.mState;
    }

    /**
     * Compares update state.
     *
     * @param state update state to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateState& state) const { return !operator==(state); }
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
     * @param acceptance state acceptance to compare with.
     * @return bool.
     */
    bool operator==(const StateAcceptance& acceptance) const
    {
        return InstanceIdent::operator==(acceptance) && mChecksum == acceptance.mChecksum
            && mResult == acceptance.mResult && mReason == acceptance.mReason;
    }

    /**
     * Compares state acceptance.
     *
     * @param acceptance state acceptance to compare with.
     * @return bool.
     */
    bool operator!=(const StateAcceptance& acceptance) const { return !operator==(acceptance); }
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
     * @param request state request to compare with.
     * @return bool.
     */
    bool operator==(const StateRequest& request) const
    {
        return mInstanceIdent == request.mInstanceIdent && mDefault == request.mDefault;
    }

    /**
     * Compares state request.
     *
     * @param request state request to compare with.
     * @return bool.
     */
    bool operator!=(const StateRequest& request) const { return !operator==(request); }
};

} // namespace aos::cloudprotocol

#endif
