/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_SMCONTROLLER_ITF_SENDER_HPP_
#define AOS_CORE_CM_SMCONTROLLER_ITF_SENDER_HPP_

#include <core/common/types/log.hpp>

namespace aos::cm::smcontroller {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Sender interface.
 */
class SenderItf {
public:
    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;

    /**
     * Sends log.
     *
     * @param log log message.
     * @return Error.
     */
    virtual Error SendLog(const PushLog& log) = 0;
};

} // namespace aos::cm::smcontroller

#endif
