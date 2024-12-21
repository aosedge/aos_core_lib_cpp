/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CONNECTIONSUBSC_STUB_HPP_
#define AOS_CONNECTIONSUBSC_STUB_HPP_

#include <mutex>
#include <unordered_set>

#include "aos/common/connectionsubsc.hpp"

namespace aos {

/**
 * Connection publisher stub.
 */
class ConnectionPublisherStub : public ConnectionPublisherItf {
public:
    /**
     * Subscribes to cloud connection events.
     *
     * @param subscriber subscriber reference.
     */
    Error Subscribe(ConnectionSubscriberItf& subscriber) override
    {
        std::lock_guard lock {mMutex};

        if (mSubscribers.find(&subscriber) != mSubscribers.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        mSubscribers.insert(&subscriber);

        return ErrorEnum::eNone;
    }

    /**
     * Unsubscribes from cloud connection events.
     *
     * @param subscriber subscriber reference.
     */
    void Unsubscribe(ConnectionSubscriberItf& subscriber) override
    {
        std::lock_guard lock {mMutex};

        mSubscribers.erase(&subscriber);
    }

    /**
     * Notifies publishers that cloud is connected.
     */
    void Connect()
    {
        std::lock_guard lock {mMutex};

        for (auto subscriber : mSubscribers) {
            subscriber->OnConnect();
        }
    }

    /**
     * Notifies publishers that cloud is disconnected.
     */
    void Disconnect()
    {
        std::lock_guard lock {mMutex};

        for (auto subscriber : mSubscribers) {
            subscriber->OnDisconnect();
        }
    }

private:
    std::mutex                                   mMutex;
    std::unordered_set<ConnectionSubscriberItf*> mSubscribers;
};

} // namespace aos

#endif
