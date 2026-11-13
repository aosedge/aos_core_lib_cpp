/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_ITF_MONITORINGDATA_HPP_
#define AOS_CORE_COMMON_MONITORING_ITF_MONITORINGDATA_HPP_

#include <core/common/types/monitoring.hpp>

namespace aos::monitoring {

struct InstanceMonitoringData {
    InstanceIdent        mInstanceIdent;
    StaticString<cIDLen> mRuntimeID;
    MonitoringData       mMonitoringData;

    /**
     * Constructs a new instance monitoring data object.
     */
    InstanceMonitoringData() = default;

    /**
     * Constructs a new instance monitoring data object.
     *
     * @param instanceIdent instance ident.
     */
    explicit InstanceMonitoringData(const InstanceIdent& instanceIdent)
        : mInstanceIdent(instanceIdent)
    {
    }

    /**
     * Compares instance monitoring data.
     *
     * @param rhs instance monitoring data to compare.
     * @return bool.
     */
    friend bool operator==(const InstanceMonitoringData& lhs, const InstanceMonitoringData& rhs)
    {
        return lhs.mInstanceIdent == rhs.mInstanceIdent && lhs.mRuntimeID == rhs.mRuntimeID
            && lhs.mMonitoringData == rhs.mMonitoringData;
    };

    /**
     * Compares instance monitoring data.
     *
     * @param data instance monitoring data to compare.
     * @return bool.
     */
    friend bool operator!=(const InstanceMonitoringData& lhs, const InstanceMonitoringData& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * Node monitoring data.
 */
struct NodeMonitoringData {
    StaticString<cIDLen>                                  mNodeID;
    MonitoringData                                        mMonitoringData;
    StaticArray<InstanceMonitoringData, cMaxNumInstances> mInstances;

    /**
     * Compares node monitoring data.
     *
     * @param data node monitoring data to compare with.
     * @return bool.
     */
    friend bool operator==(const NodeMonitoringData& lhs, const NodeMonitoringData& data)
    {
        return lhs.mNodeID == data.mNodeID && lhs.mMonitoringData == data.mMonitoringData
            && lhs.mInstances == data.mInstances;
    };

    /**
     * Compares node monitoring data.
     *
     * @param data node monitoring data to compare with.
     * @return bool.
     */
    friend bool operator!=(const NodeMonitoringData& lhs, const NodeMonitoringData& data) { return !(lhs == data); };
};

} // namespace aos::monitoring

#endif
