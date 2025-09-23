/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LOGPROVIDER_LOGPROVIDER_HPP_
#define AOS_CORE_SM_LOGPROVIDER_LOGPROVIDER_HPP_

#include <core/common/types/types.hpp>

namespace aos::sm::logprovider {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Log observer interface.
 */
class LogObserverItf {
public:
    /**
     * Destructor.
     */
    virtual ~LogObserverItf() = default;

    /**
     * On log received event handler.
     *
     * @param log log.
     * @return Error.
     */
    virtual Error OnLogReceived(const PushLog& log) = 0;
};

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

    /**
     * Subscribes on logs.
     *
     * @param observer logs observer.
     * @return Error.
     */
    virtual Error Subscribe(LogObserverItf& observer) = 0;

    /**
     * Unsubscribes from logs.
     *
     * @param observer logs observer.
     * @return Error.
     */
    virtual Error Unsubscribe(LogObserverItf& observer) = 0;
};

/** @}*/

} // namespace aos::sm::logprovider

#endif
