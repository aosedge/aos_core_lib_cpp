/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_IMAGEPROVIDER_HPP_
#define AOS_CM_LAUNCHER_STUBS_IMAGEPROVIDER_HPP_

#include <aos/cm/imageprovider.hpp>

#include <algorithm>
#include <map>
#include <vector>

namespace aos::cm::imageprovider {

/**
 * Stub implementation of ImageProviderItf interface
 */
class ImageProviderStub : public ImageProviderItf {
public:
    /**
     * Initializes stub object.
     */
    void Init()
    {
        mServices.clear();
        mLayers.clear();
        mListeners.clear();
        mRemovedServiceQueue.clear();
    }

    /**
     * Retrieves information about specified service.
     *
     * @param serviceID identifier of the service.
     * @param[out] serviceInfo service information.
     * @return Error.
     */
    Error GetServiceInfo(const String& serviceID, ServiceInfo& serviceInfo) override
    {
        auto it = mServices.find(serviceID);
        if (it != mServices.end()) {
            serviceInfo = it->second;

            return ErrorEnum::eNone;
        }

        return ErrorEnum::eNotFound;
    }

    /**
     * Retrieves metadata about an image layer.
     *
     * @param digest layer digest.
     * @param[out] layerInfo layer info.
     * @return Error.
     */
    Error GetLayerInfo(const String& digest, LayerInfo& layerInfo) override
    {
        auto it = mLayers.find(digest);
        if (it != mLayers.end()) {
            layerInfo = it->second;

            return ErrorEnum::eNone;
        }

        return ErrorEnum::eNotFound;
    }

    /**
     * Subscribes listener on service information updates.
     *
     * @param listener service listener.
     * @return Error.
     */
    Error SubscribeListener(ServiceListenerItf& listener) override
    {
        mListeners.push_back(&listener);

        return ErrorEnum::eNone;
    }

    /**
     * Unsubscribes service information listener.
     *
     * @param listener service listener.
     * @return Error.
     */
    Error UnsubscribeListener(ServiceListenerItf& listener) override
    {
        mListeners.erase(std::remove(mListeners.begin(), mListeners.end(), &listener), mListeners.end());

        return ErrorEnum::eNotFound;
    }

    /**
     * Adds a service to the test provider.
     *
     * @param serviceID service identifier.
     * @param serviceInfo service information.
     */
    void AddService(const String& serviceID, const ServiceInfo& serviceInfo) { mServices[serviceID] = serviceInfo; }

    /**
     * Adds a layer to the test provider.
     *
     * @param digest layer digest.
     * @param layerInfo layer information.
     */
    void AddLayer(const String& digest, const LayerInfo& layerInfo) { mLayers[digest] = layerInfo; }

    /**
     * Removes a service from the test provider and notifies listeners.
     *
     * @param serviceID service identifier.
     * @return Error.
     */
    Error RemoveService(const String& serviceID)
    {
        auto it = mServices.find(serviceID);
        if (it == mServices.end()) {
            return ErrorEnum::eNotFound;
        }

        mServices.erase(it);

        for (auto* listener : mListeners) {
            listener->OnServiceRemoved(serviceID);
        }

        mRemovedServiceQueue.push_back(serviceID);

        return ErrorEnum::eNone;
    }

    /**
     * Removes a layer from the test provider.
     *
     * @param digest layer digest.
     * @return Error.
     */
    Error RemoveLayer(const String& digest)
    {
        auto it = mLayers.find(digest);
        if (it == mLayers.end()) {
            return ErrorEnum::eNotFound;
        }

        mLayers.erase(it);

        return ErrorEnum::eNone;
    }

private:
    std::map<StaticString<cServiceIDLen>, ServiceInfo> mServices;
    std::map<StaticString<cLayerDigestLen>, LayerInfo> mLayers;
    std::vector<ServiceListenerItf*>                   mListeners;
    std::vector<StaticString<cServiceIDLen>>           mRemovedServiceQueue;
};

} // namespace aos::cm::imageprovider

#endif
