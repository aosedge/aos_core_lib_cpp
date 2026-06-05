/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UNITCONFIG_ITF_UNITCONFIG_HPP_
#define AOS_CORE_CM_UNITCONFIG_ITF_UNITCONFIG_HPP_

#include <core/common/types/unitconfig.hpp>

namespace aos::cm::unitconfig {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Interface for unit config handling.
 */
class UnitConfigItf {
public:
    /**
     * Destructor.
     */
    virtual ~UnitConfigItf() = default;

    /**
     * Checks unit config.
     *
     * @param config unit config.
     * @return Error.
     */
    virtual Error CheckUnitConfig(const UnitConfig& config) = 0;

    /**
     * Updates unit config.
     *
     * @param config unit config.
     * @return Error.
     */
    virtual Error UpdateUnitConfig(const UnitConfig& config) = 0;

    /**
     * Returns unit config status.
     *
     * @param[out] status unit config status.
     * @return Error.
     */
    virtual Error GetUnitConfigStatus(UnitConfigStatus& status) = 0;
};

} // namespace aos::cm::unitconfig

#endif
