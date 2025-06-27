/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_STATE_HPP_
#define AOS_CLOUDPROTOCOL_STATE_HPP_

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
 * Max checksum len.
 *
 */
constexpr auto cCheckSumLen = AOS_CONFIG_CRYPTO_SHA1_DIGEST_SIZE;

/**
 * State message type.
 */
class StateMessageTypeType {
public:
    enum class Enum {
        eStateAcceptance,
        eUpdateState,
        eNewState,
        eStateRequest,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "stateAcceptance",
            "updateState",
            "newState",
            "stateRequest",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using StateMessageTypeEnum = StateMessageTypeType::Enum;
using StateMessageType     = EnumStringer<StateMessageTypeType>;

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

struct BaseState : public InstanceIdent {
    /**
     * Constructor.
     *
     * @param instanceIdent instance identifier.
     * @param messageType state message type.
     */
    BaseState(const InstanceIdent& instanceIdent, StateMessageType messageType)
        : InstanceIdent(instanceIdent)
        , mMessageType(messageType)
    {
    }

    StateMessageType mMessageType;
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
        : BaseState(instanceIdent, StateMessageTypeEnum::eStateAcceptance)
    {
    }

    StaticString<cCheckSumLen> mChecksum;
    StateResult                mResult;
    StaticString<cStateReason> mReason;
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
        : BaseState(instanceIdent, StateMessageTypeEnum::eUpdateState)
    {
    }

    StaticString<cCheckSumLen> mChecksum;
    StaticString<cStateLen>    mState;
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
        : BaseState(instanceIdent, StateMessageTypeEnum::eNewState)
    {
    }

    StaticString<cCheckSumLen> mChecksum;
    StaticString<cStateLen>    mState;
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
        : BaseState(instanceIdent, StateMessageTypeEnum::eStateRequest)
    {
    }

    bool mDefault {};
};

} // namespace aos::cloudprotocol

#endif
