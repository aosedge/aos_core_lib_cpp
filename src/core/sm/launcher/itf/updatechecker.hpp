/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_ITF_UPDATECHECKER_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_UPDATECHECKER_HPP_

#include <core/common/tools/error.hpp>

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Update checker interface.
 */
class UpdateCheckerItf {
public:
    /**
     * Destructor.
     */
    virtual ~UpdateCheckerItf() = default;

    /**
     * Checks if update succeeded.
     *
     * @return Error.
     */
    virtual Error Check() = 0;
};

/** @} */

} // namespace aos::sm::launcher

#endif
