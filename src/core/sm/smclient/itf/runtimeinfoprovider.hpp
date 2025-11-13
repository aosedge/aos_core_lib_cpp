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
     * Returns runtimes infos.
     *
     * @param[out] runtimes runtime infos.
     * @return Error.
     */
    virtual Error GetRuntimesInfos(Array<RuntimeInfo>& runtimes) const = 0;
};

} // namespace aos::sm::smclient

#endif
