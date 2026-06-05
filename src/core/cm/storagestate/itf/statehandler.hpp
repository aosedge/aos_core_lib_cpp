/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_STORAGESTATE_ITF_STATEHANDLER_HPP_
#define AOS_CORE_CM_STORAGESTATE_ITF_STATEHANDLER_HPP_

#include <core/common/types/state.hpp>

namespace aos::cm::storagestate {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Interface to manage state.
 */
class StateHandlerItf {
public:
    /**
     * Destructor.
     */
    virtual ~StateHandlerItf() = default;

    /**
     * Updates storage state with new state.
     *
     * @param state update state.
     * @return Error.
     */
    virtual Error UpdateState(const UpdateState& state) = 0;

    /**
     * Accepts state.
     *
     * @param state state acceptance.
     * @return Error.
     */
    virtual Error AcceptState(const StateAcceptance& state) = 0;
};

/** @}*/

} // namespace aos::cm::storagestate

#endif
