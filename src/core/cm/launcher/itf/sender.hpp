/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_ITF_SENDER_HPP_
#define AOS_CORE_CM_LAUNCHER_ITF_SENDER_HPP_

#include <core/common/types/envvars.hpp>

namespace aos::cm::communication {

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
     * Sends override env vars statuses.
     *
     * @param statuses override env vars statuses.
     * @return Error.
     */
    virtual Error SendOverrideEnvsStatuses(const OverrideEnvVarsStatuses& statuses) = 0;
};

} // namespace aos::cm::communication

#endif
