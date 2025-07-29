/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_STATE_HPP_
#define AOS_CLOUDPROTOCOL_STATE_HPP_

#include <core/common/crypto/crypto.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/types/types.hpp>

#include "cloudprotocol.hpp"

namespace aos::cloudprotocol {

/**
 * State length.
 */
static constexpr auto cStateLen = AOS_CONFIG_CLOUDPROTOCOL_STATE_LEN;

/**
 * State reason length.
 */
static constexpr auto cStateReason = cErrorMessageLen;

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

struct BaseState {
    /**
     * Constructor.
     *
     * @param instanceIdent instance identifier.
     * @param messageType state message type.
     */
    BaseState(const InstanceIdent& instanceIdent, MessageType messageType)
        : mInstanceIdent(instanceIdent)
        , mMessageType(messageType)
    {
    }

    InstanceIdent mInstanceIdent;
    MessageType   mMessageType;

    /**
     * Compares base state.
     *
     * @param state base state to compare with.
     * @return bool.
     */
    bool operator==(const BaseState& state) const
    {
        return mInstanceIdent == state.mInstanceIdent && mMessageType == state.mMessageType;
    }

    /**
     * Compares base state.
     *
     * @param state base state to compare with.
     * @return bool.
     */
    bool operator!=(const BaseState& state) const { return !operator==(state); }
};

/**
 * State acceptance.
 */
struct StateAcceptance : public BaseState {
    /**
     * Constructor.
     *
     * @param instanceIdent instance identifier.
     */
    explicit StateAcceptance(const InstanceIdent& instanceIdent = InstanceIdent())
        : BaseState(instanceIdent, MessageTypeEnum::eStateAcceptance)
    {
    }

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
        return BaseState::operator==(acceptance) && mChecksum == acceptance.mChecksum && mResult == acceptance.mResult
            && mReason == acceptance.mReason;
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
 * Update state.
 */
struct UpdateState : public BaseState {
    /**
     * Constructor.
     *
     * @param instanceIdent instance identifier.
     */
    explicit UpdateState(const InstanceIdent& instanceIdent = InstanceIdent())
        : BaseState(instanceIdent, MessageTypeEnum::eUpdateState)
    {
    }

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
        return BaseState::operator==(state) && mChecksum == state.mChecksum && mState == state.mState;
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
 * New state.
 */
struct NewState : public BaseState {
    /**
     * Constructor.
     *
     * @param instanceIdent instance identifier.
     */
    explicit NewState(const InstanceIdent& instanceIdent = InstanceIdent())
        : BaseState(instanceIdent, MessageTypeEnum::eNewState)
    {
    }

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
        return BaseState::operator==(state) && mChecksum == state.mChecksum && mState == state.mState;
    }
};

/**
 * State request.
 */
struct StateRequest : public BaseState {
    /**
     * Constructor.
     *
     * @param instanceIdent instance identifier.
     */
    explicit StateRequest(const InstanceIdent& instanceIdent = InstanceIdent())
        : BaseState(instanceIdent, MessageTypeEnum::eStateRequest)
    {
    }

    bool mDefault {};

    /**
     * Compares state request.
     *
     * @param request state request to compare with.
     * @return bool.
     */
    bool operator==(const StateRequest& request) const
    {
        return BaseState::operator==(request) && mDefault == request.mDefault;
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
