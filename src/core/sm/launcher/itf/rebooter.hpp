/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_ITF_REBOOTER_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_REBOOTER_HPP_

#include <core/common/tools/error.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Rebooter interface.
 */
class RebooterItf {
public:
    /**
     * Destructor.
     */
    virtual ~RebooterItf() = default;

    /**
     * Reboots the system.
     *
     * @return Error.
     */
    virtual Error Reboot() = 0;
};

/** @} */

} // namespace aos::sm::launcher

#endif
