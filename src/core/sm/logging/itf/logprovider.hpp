/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LOGGING_ITF_LOGPROVIDER_HPP_
#define AOS_CORE_SM_LOGGING_ITF_LOGPROVIDER_HPP_

#include <core/common/types/log.hpp>

namespace aos::sm::logging {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Log provider interface.
 */
class LogProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~LogProviderItf() = default;

    /**
     * Gets instance log.
     *
     * @param request request log.
     * @return Error.
     */
    virtual Error GetInstanceLog(const RequestLog& request) = 0;

    /**
     * Gets instance crash log.
     *
     * @param request request log.
     * @return Error.
     */
    virtual Error GetInstanceCrashLog(const RequestLog& request) = 0;

    /**
     * Gets system log.
     *
     * @param request request log.
     * @return Error.
     */
    virtual Error GetSystemLog(const RequestLog& request) = 0;
};

/** @}*/

} // namespace aos::sm::logging

#endif
