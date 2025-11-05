/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_NDOECONFIG_JSONPROVIDER_HPP_
#define AOS_CORE_COMMON_NDOECONFIG_JSONPROVIDER_HPP_

#include <core/common/types/unitconfig.hpp>

namespace aos::nodeconfig {

/**
 * Node config JSON length.
 */
static constexpr auto cNodeConfigJSONLen = AOS_CONFIG_NODECONFIG_JSON_LEN;

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
     * Dumps config object into string.
     *
     * @param nodeConfig node config object.
     * @param[out] json node config JSON string.
     * @return Error.
     */
    virtual Error NodeConfigToJSON(const NodeConfig& nodeConfig, String& json) const = 0;

    /**
     * Creates node config object from a JSON string.
     *
     * @param json node config JSON string.
     * @param[out] nodeConfig node config object.
     * @return Error.
     */
    virtual Error NodeConfigFromJSON(const String& json, NodeConfig& nodeConfig) const = 0;
};

} // namespace aos::nodeconfig

#endif
