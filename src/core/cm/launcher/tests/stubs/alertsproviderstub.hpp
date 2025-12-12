/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_ALERTS_TESTS_STUBS_ALERTSPROVIDERSTUB_HPP_
#define AOS_CM_ALERTS_TESTS_STUBS_ALERTSPROVIDERSTUB_HPP_

#include <vector>

#include <core/cm/alerts/itf/provider.hpp>

namespace aos::cm::alerts {

class AlertsProviderStub : public alerts::AlertsProviderItf {
public:
    void Init() { mListeners.clear(); }

    Error SubscribeListener(const Array<AlertTag>& tags, alerts::AlertsListenerItf& listener) override
    {
        mListeners.push_back({&listener, tags});
        return ErrorEnum::eNone;
    }

    Error UnsubscribeListener(alerts::AlertsListenerItf& listener) override
    {
        for (auto it = mListeners.begin(); it != mListeners.end(); ++it) {
            if (it->mListener == &listener) {
                mListeners.erase(it);
                return ErrorEnum::eNone;
            }
        }

        return ErrorEnum::eNotFound;
    }

    void TriggerSystemQuotaAlert()
    {
        SystemQuotaAlert alert;
        AlertVariant     variant;

        alert.mState = QuotaAlertStateEnum::eFall;
        variant.SetValue(alert);

        for (const auto& listenerInfo : mListeners) {
            listenerInfo.mListener->OnAlertReceived(variant);
        }
    }

private:
    struct ListenerInfo {
        alerts::AlertsListenerItf*              mListener;
        StaticArray<AlertTag, cAlertItemsCount> mTags;
    };

    std::vector<ListenerInfo> mListeners;
};

} // namespace aos::cm::alerts

#endif
