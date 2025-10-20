/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_SMCONTROLLER_ITF_LOGPROVIDER_HPP_
#define AOS_CORE_CM_SMCONTROLLER_ITF_LOGPROVIDER_HPP_

#include <core/common/types/log.hpp>

namespace aos::cm::smcontroller {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 *  Log provider interface.
 */
class LogProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~LogProviderItf() = default;

    /**
     * Requests log.
     *
     * @param log log request.
     * @return Error.
     */
    virtual Error RequestLog(const RequestLog& log) = 0;
};

} // namespace aos::cm::smcontroller

#endif
