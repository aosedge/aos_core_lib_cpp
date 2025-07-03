/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_NODEMANAGER_HPP_
#define AOS_CM_LAUNCHER_NODEMANAGER_HPP_

#include <aos/common/monitoring/monitoring.hpp>
#include <aos/common/types.hpp>

namespace aos::cm::nodemanager {

/** @addtogroup CM NodeManager
 *  @{
 */

/**
 * Represents the status of service instance, extended with its state checksum.
 */
struct InstanceStatus : public aos::InstanceStatus {
    /**
     * Node identifier.
     */
    StaticString<cNodeIDLen> mNodeID;

    /**
     * Service state checksum.
     */
    StaticString<cSHA3_224Size> mStateChecksum;

    /**
     * Compares instance status.
     *
     * @param instance status to compare.
     * @return bool.
     */
    bool operator==(const InstanceStatus& instance) const
    {
        return static_cast<const aos::InstanceStatus&>(*this) == static_cast<const aos::InstanceStatus&>(instance)
            && mNodeID == instance.mNodeID && mStateChecksum == instance.mStateChecksum;
    }

    /**
     * Compares instance status.
     *
     * @param instance status to compare.
     * @return bool.
     */
    bool operator!=(const InstanceStatus& instance) const { return !operator==(instance); }
};

/**
 * Represents run status of service instances running on a particular node.
 */
struct NodeRunInstanceStatus {
    /**
     * Node identifier.
     */
    StaticString<cNodeIDLen> mNodeID;

    /**
     * Node type.
     */
    StaticString<cNodeTypeLen> mNodeType;

    /**
     * List of instance statuses running on this node.
     */
    StaticArray<InstanceStatus, cMaxNumInstances> mInstances;

    /**
     * Compares instance status.
     *
     * @param instance status to compare.
     * @return bool.
     */
    bool operator==(const NodeRunInstanceStatus& instance) const
    {
        return mNodeID == instance.mNodeID && mNodeType == instance.mNodeType && mInstances == instance.mInstances;
    }

    /**
     * Compares instance status.
     *
     * @param instance status to compare.
     * @return bool.
     */
    bool operator!=(const NodeRunInstanceStatus& instance) const { return !operator==(instance); }
};

/**
 * Interface for receiving status updates about running service instances.
 */
class ServiceStatusListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ServiceStatusListenerItf() = default;

    /**
     * Notifies changes of service instance statuses.
     *
     * @param status status of service instances.
     */
    virtual void OnStatusChanged(const NodeRunInstanceStatus& status) = 0;
};

/**
 * Interface to manage service instances.
 */
class NodeManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeManagerItf() = default;

    /**
     * Runs service instances on the specified node.
     *
     * @param nodeID node identifier.
     * @param services service list.
     * @param layers service layer list.
     * @param instances service instance list.
     * @param forceRestart force service restart.
     * @return Error.
     */
    virtual Error StartInstances(const String& nodeID, const Array<ServiceInfo>& services,
        const Array<LayerInfo>& layers, const Array<InstanceInfo>& instances, bool forceRestart)
        = 0;

    /**
     * Stops service instances on the specified node.
     *
     * @param nodeID node identifier.
     * @param instances service instance list.
     * @return Error.
     */
    virtual Error StopInstances(const String& nodeID, const Array<InstanceIdent>& instances) = 0;

    /**
     * Retrieves average monitoring data for a node.
     *
     * @param nodeID node identifier.
     * @param monitoring average monitoring data.
     * @return Error.
     */
    virtual Error GetAverageMonitoring(const String& nodeID, monitoring::NodeMonitoringData& monitoring) const = 0;

    /**
     * Subscribes listener of service statuses.
     *
     * @param listener service status listener.
     * @return Error.
     */
    virtual Error SubscribeListener(ServiceStatusListenerItf& listener) = 0;

    /**
     * Unsubscribes listener of service statuses.
     *
     * @param listener service status listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(ServiceStatusListenerItf& listener) = 0;
};

/** @}*/

} // namespace aos::cm::nodemanager

#endif
