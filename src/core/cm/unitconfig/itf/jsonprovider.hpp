/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UNITCONFIG_ITF_JSONPROVIDER_HPP_
#define AOS_CORE_CM_UNITCONFIG_ITF_JSONPROVIDER_HPP_

#include <core/common/types/unitconfig.hpp>

namespace aos::cm::unitconfig {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * JSON provider interface.
 */
class JSONProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~JSONProviderItf() = default;

    /**
     * Creates unit config object from a JSON string.
     *
     * @param json JSON string.
     * @param[out] unitConfig unit config object.
     * @return Error.
     */
    virtual Error UnitConfigFromJSON(const String& json, aos::UnitConfig& unitConfig) const = 0;

    /**
     * Creates unit config JSON string from a unit config object.
     *
     * @param unitConfig unit config object.
     * @param[out] json JSON string.
     * @return Error.
     */
    virtual Error UnitConfigToJSON(const aos::UnitConfig& unitConfig, String& json) const = 0;
};

/** @}*/

} // namespace aos::cm::unitconfig

#endif
