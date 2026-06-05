/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_LOGGING_ITF_SENDER_HPP_
#define AOS_CORE_COMMON_LOGGING_ITF_SENDER_HPP_

#include <core/common/types/log.hpp>

namespace aos::logging {

/**
 * Logging sender interface.
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
     * @param log log to send.
     * @return Error.
     */
    virtual Error SendLog(const PushLog& log) = 0;
};

} // namespace aos::logging

#endif
