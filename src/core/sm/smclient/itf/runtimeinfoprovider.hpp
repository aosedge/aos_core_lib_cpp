/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_SM_SMCLIENT_ITF_RUNTIMEINFOPROVIDER_HPP_
#define AOS_CORE_SM_SMCLIENT_ITF_RUNTIMEINFOPROVIDER_HPP_

#include <core/common/types/common.hpp>

namespace aos::sm::smclient {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Runtime info provider.
 */
class RuntimeInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~RuntimeInfoProviderItf() = default;

    /**
     * Returns all runtime IDs.
     *
     * @param runtimeIDs array to store runtime IDs.
     * @return Error.
     */
    virtual Error GetAllRuntimeIDs(Array<StaticString<cIDLen>>& runtimeIDs) const = 0;

    /**
     * Returns runtime info by ID.
     *
     * @param runtimeID runtime ID.
     * @param[out] runtimeInfo runtime info.
     * @return Error.
     */
    virtual Error GetRuntimeInfo(const String& runtimeID, RuntimeInfo& runtimeInfo) const = 0;
};

} // namespace aos::sm::smclient

#endif
