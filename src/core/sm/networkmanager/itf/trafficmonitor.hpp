/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_TRAFFICMONITOR_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_TRAFFICMONITOR_HPP_

#include <core/common/types/common.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Traffic period type.
 */
class TrafficPeriodType {
public:
    enum class Enum { eMinutePeriod, eHourPeriod, eDayPeriod, eMonthPeriod, eYearPeriod };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sTrafficPeriodStrings[] = {"minute", "hour", "day", "month", "year"};

        return Array<const char* const>(sTrafficPeriodStrings, ArraySize(sTrafficPeriodStrings));
    };
};

using TrafficPeriodEnum = TrafficPeriodType::Enum;
using TrafficPeriod     = EnumStringer<TrafficPeriodType>;

/**
 * Traffic monitor interface.
 */
class TrafficMonitorItf {
public:
    /**
     * Destructor.
     */
    virtual ~TrafficMonitorItf() = default;

    /**
     * Starts traffic monitoring.
     *
     * @return Error.
     */
    virtual Error Start() = 0;

    /**
     * Stops traffic monitoring.
     *
     * @return Error.
     */
    virtual Error Stop() = 0;

    /**
     * Sets monitoring period.
     *
     * @param period monitoring period in seconds.
     */
    virtual void SetPeriod(TrafficPeriod period) = 0;

    /**
     * Starts monitoring instance.
     *
     * @param instanceID instance ID.
     * @param IPAddress instance IP address.
     * @param downloadLimit download limit.
     * @param uploadLimit upload limit.
     * @return Error.
     */
    virtual Error StartInstanceMonitoring(
        const String& instanceID, const String& IPAddress, uint64_t downloadLimit, uint64_t uploadLimit)
        = 0;

    /**
     * Stops monitoring instance.
     *
     * @param instanceID instance ID.
     * @return Error.
     */
    virtual Error StopInstanceMonitoring(const String& instanceID) = 0;

    /**
     * Returns system traffic data.
     *
     * @param inputTraffic input traffic.
     * @param outputTraffic output traffic.
     * @return Error.
     */
    virtual Error GetSystemTraffic(uint64_t& inputTraffic, uint64_t& outputTraffic) const = 0;

    /**
     * Returns instance traffic data.
     *
     * @param instanceID instance ID.
     * @param inputTraffic input traffic.
     * @param outputTraffic output traffic.
     * @return Error.
     */
    virtual Error GetInstanceTraffic(const String& instanceID, uint64_t& inputTraffic, uint64_t& outputTraffic) const
        = 0;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
