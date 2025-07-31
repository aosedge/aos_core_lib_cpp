/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_IMAGEPROVIDER_HPP_
#define AOS_CM_IMAGEPROVIDER_HPP_

#include <aos/common/ocispec/serviceconfig.hpp>
#include <aos/common/types.hpp>

namespace aos::cm::imagemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Structure providing extended service information.
 */
struct ServiceInfo : public aos::ServiceInfo {
    /**
     * Remote URL from which the service can be retrieved.
     */
    StaticString<cURLLen> mRemoteURL;

    /**
     * Filesystem path where the service is stored.
     */
    StaticString<cFilePathLen> mPath;

    /**
     * Service installation time.
     */
    Time mTimestamp;

    /**
     * Service state.
     */
    ServiceState mState;

    /**
     * Configuration parameters specific to the service.
     */
    oci::ServiceConfig mConfig;

    /**
     * List of layer digests used by the service.
     */
    StaticArray<StaticString<cLayerDigestLen>, cMaxNumLayers> mLayerDigests;

    /**
     * List of ports exposed by this service.
     */
    StaticArray<StaticString<cExposedPortLen>, cMaxNumExposedPorts> mExposedPorts;
};

/**
 * Information about a service layer.
 */
struct LayerInfo : public aos::LayerInfo {
    /**
     * Remote URL to download the layer from.
     */
    StaticString<cURLLen> mRemoteURL;

    /**
     * Local file system path to the layer.
     */
    StaticString<cFilePathLen> mPath;

    /**
     * Timestamp of the layer's last update.
     */
    Time mTimestamp;

    /**
     * Layer state.
     */
    LayerState mState;
};

/**
 * Listener receiving notifications when services are removed.
 */
class ServiceListenerItf {
public:
    /**
     * Destructor.
     */
    virtual ~ServiceListenerItf() = default;

    /**
     * Callback triggered when a service is removed.
     *
     * @param serviceID identifier of the removed service.
     */
    virtual void OnServiceRemoved(const String& serviceID) = 0;
};

/**
 * Interface that retrieves service information from its image.
 */
class ImageProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~ImageProviderItf() = default;

    /**
     * Retrieves information about specified service.
     *
     * @param serviceID identifier of the service.
     * @param[out] serviceInfo service information.
     * @return Error.
     */
    virtual Error GetServiceInfo(const String& serviceID, ServiceInfo& serviceInfo) = 0;

    /**
     * Retrieves metadata about an image layer.
     *
     * @param digest layer digest.
     * @param[out] layerInfo layer info.
     * @return Error.
     */
    virtual Error GetLayerInfo(const String& digest, LayerInfo& layerInfo) = 0;

    /**
     * Subscribes listener on service information updates.
     *
     * @param listener service listener.
     * @return Error.
     */
    virtual Error SubscribeListener(ServiceListenerItf& listener) = 0;

    /**
     * Unsubscribes service information listener.
     *
     * @param listener service listener.
     * @return Error.
     */
    virtual Error UnsubscribeListener(ServiceListenerItf& listener) = 0;
};

/** @}*/

} // namespace aos::cm::imagemanager

#endif
