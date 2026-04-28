/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_BANDWIDTH_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_BANDWIDTH_HPP_

#include <core/common/tools/string.hpp>
#include <core/common/types/common.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Bandwidth parameters (rates in bits/sec, bursts in bytes).
 *
 * Zero rate disables shaping for that direction. Apply is a no-op when both
 * mIngressRate and mEgressRate are zero.
 */
struct BandwidthParams {
    uint64_t mIngressRate {};
    uint64_t mIngressBurst {};
    uint64_t mEgressRate {};
    uint64_t mEgressBurst {};
};

/**
 * Bandwidth interface.
 *
 * Applies/clears tc shaping (TBF, plus IFB+mirred for ingress) on a given
 * interface.
 */
class BandwidthItf {
public:
    /**
     * Destructor.
     */
    virtual ~BandwidthItf() = default;

    /**
     * Applies bandwidth shaping to the given interface.
     *
     * No-op when both mIngressRate and mEgressRate are zero.
     *
     * @param ifName interface name.
     * @param params bandwidth parameters.
     * @return Error.
     */
    virtual Error Apply(const String& ifName, const BandwidthParams& params) = 0;

    /**
     * Clears bandwidth shaping (qdisc + IFB if used) from the given interface.
     *
     * @param ifName interface name.
     * @return Error.
     */
    virtual Error Clear(const String& ifName) = 0;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
