/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_STATE_HPP_
#define AOS_CLOUDPROTOCOL_STATE_HPP_

#include "aos/common/cloudprotocol/cloudprotocol.hpp"
#include "aos/common/crypto/crypto.hpp"
#include "aos/common/tools/optional.hpp"
#include "aos/common/types.hpp"

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
};

} // namespace aos::cloudprotocol

#endif
