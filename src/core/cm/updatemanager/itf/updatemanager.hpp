/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_ITF_UPDATEMANAGER_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_ITF_UPDATEMANAGER_HPP_

#include <core/common/types/desiredstatus.hpp>

namespace aos::cm::updatemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Sender interface.
 */
class UpdateManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~UpdateManagerItf() = default;

    /**
     * Processes desired status.
     *
     * @param desiredStatus desired status.
     * @return Error.
     */
    virtual Error ProcessDesiredStatus(const DesiredStatus& desiredStatus) = 0;
};

/** @}*/

} // namespace aos::cm::updatemanager

#endif
