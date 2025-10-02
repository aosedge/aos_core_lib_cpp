/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_STORAGESTATE_ITF_STATE_HPP_
#define AOS_CORE_CM_STORAGESTATE_ITF_STATE_HPP_

#include <core/common/types/obsolete.hpp>

namespace aos::cm::storagestate {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Interface to manage state.
 */
class StateItf {
public:
    /**
     * Destructor.
     */
    virtual ~StateItf() = default;

    /**
     * Updates storage state with new state.
     *
     * @param instanceIdent instance ident.
     * @param state state content.
     * @param checksum state checksum.
     * @return Error.
     */
    virtual Error UpdateState(const InstanceIdent& instanceIdent, const String& state, const String& checksum) = 0;

    /**
     * Accepts state from storage.
     *
     * @param instanceIdent instance ident.
     * @param checksum state checksum.
     * @param result state result.
     * @param reason reason of the result.
     * @return Error.
     */
    virtual Error AcceptState(
        const InstanceIdent& instanceIdent, const String& checksum, StateResult result, const String& reason)
        = 0;
};

/** @}*/

} // namespace aos::cm::storagestate

#endif
