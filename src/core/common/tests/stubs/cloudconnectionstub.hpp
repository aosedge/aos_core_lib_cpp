/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_STUBS_CLOUDCONNECTIONSTUB_HPP_
#define AOS_CORE_COMMON_TESTS_STUBS_CLOUDCONNECTIONSTUB_HPP_

#include <set>

#include <core/common/cloudconnection/itf/cloudconnection.hpp>

namespace aos {

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
    Error Subscribe(ConnectionListenerItf& listener) override
    {
        mListeners.insert(&listener);

        return ErrorEnum::eNone;
    }

    /**
     * Unsubscribes from cloud connection events.
     *
     * @param listener listener reference.
     */
    void Unsubscribe(ConnectionListenerItf& listener) override
    {
        mListeners.erase(&listener);

        return;
    }

    /**
     * Notifies all subscribers cloud connected.
     */
    void NotifyConnect() const
    {

        for (const auto& mListener : mListeners) {
            mListener->OnConnect();
        }

        return;
    }

    /**
     * Notifies all subscribers cloud disconnected.
     */
    void NotifyDisconnect() const
    {

        for (const auto& mListener : mListeners) {
            mListener->OnDisconnect();
        }

        return;
    }

private:
    std::set<ConnectionListenerItf*> mListeners;
};

} // namespace aos

#endif
