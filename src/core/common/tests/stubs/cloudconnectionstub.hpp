/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_STUBS_CLOUDCONNECTIONSTUB_HPP_
#define AOS_CORE_COMMON_TESTS_STUBS_CLOUDCONNECTIONSTUB_HPP_

#include <mutex>
#include <set>

#include <core/common/cloudconnection/itf/cloudconnection.hpp>

namespace aos::cloudconnection {

/**
 * Cloud connection stub.
 */
class CloudConnectionStub : public CloudConnectionItf {
public:
    /**
     * Subscribes to cloud connection events.
     *
     * @param listener listener reference.
     */
    Error SubscribeListener(ConnectionListenerItf& listener) override
    {
        std::lock_guard<std::mutex> lock {mMutex};

        mListeners.insert(&listener);

        return ErrorEnum::eNone;
    }

    /**
     * Unsubscribes from cloud connection events.
     *
     * @param listener listener reference.
     */
    void UnsubscribeListener(ConnectionListenerItf& listener) override
    {
        std::lock_guard<std::mutex> lock {mMutex};

        mListeners.erase(&listener);

        return;
    }

    /**
     * Notifies all subscribers cloud connected.
     */
    void NotifyConnect()
    {
        std::lock_guard<std::mutex> lock {mMutex};

        for (const auto& mListener : mListeners) {
            mListener->OnConnect();
        }

        return;
    }

    /**
     * Notifies all subscribers cloud disconnected.
     */
    void NotifyDisconnect()
    {
        std::lock_guard<std::mutex> lock {mMutex};

        for (const auto& mListener : mListeners) {
            mListener->OnDisconnect();
        }

        return;
    }

private:
    std::mutex                       mMutex;
    std::set<ConnectionListenerItf*> mListeners;
};

} // namespace aos::cloudconnection

#endif
