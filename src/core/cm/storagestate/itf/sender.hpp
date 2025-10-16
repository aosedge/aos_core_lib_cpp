/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_STORAGESTATE_ITF_SENDER_HPP_
#define AOS_CORE_CM_STORAGESTATE_ITF_SENDER_HPP_

#include <core/common/types/common.hpp>

namespace aos::cm::storagestate {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Sender interface.
 */
class SenderItf {
public:
    /**
     * Sends state request for the instance.
     *
     * @param instanceIdent instance ident.
     * @param isDefault true if default state is requested.
     * @return Error.
     */
    virtual Error SendStateRequest(const InstanceIdent& instanceIdent, bool isDefault) = 0;

    /**
     * Sends instance's new state.
     *
     * @param instanceIdent instance ident.
     * @param state new state.
     * @param checksum state checksum.
     * @return Error.
     */
    virtual Error SendNewState(const InstanceIdent& instanceIdent, const String& state, const Array<uint8_t>& checksum)
        = 0;

    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;
};

/** @}*/

} // namespace aos::cm::storagestate

#endif
