/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_INSTANCESTATUSPROVIDERSTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_INSTANCESTATUSPROVIDERSTUB_HPP_

#include <algorithm>
#include <vector>

#include <core/cm/launcher/itf/instancestatusprovider.hpp>

namespace aos::cm::launcher {

class InstanceStatusProviderStub : public InstanceStatusProviderItf {
public:
    void Init()
    {
        mStatuses.clear();
        mListeners.clear();
    }

    void SetStatuses(const std::vector<InstanceStatus>& statuses)
    {
        mStatuses = statuses;

        StaticArray<InstanceStatus, cMaxNumInstances> arr;

        for (const auto& st : mStatuses) {
            [[maybe_unused]] auto err = arr.PushBack(st);
        }

        for (auto* l : mListeners) {
            l->OnInstancesStatusesChanged(arr);
        }
    }

    const std::vector<InstanceStatus>& GetStatuses() const { return mStatuses; }

    Error GetInstancesStatuses(Array<InstanceStatus>& statuses) override
    {
        statuses.Clear();

        for (const auto& st : mStatuses) {
            if (auto err = statuses.PushBack(st); !err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    Error SubscribeListener(InstanceStatusListenerItf& listener) override
    {
        mListeners.push_back(&listener);

        return ErrorEnum::eNone;
    }

    Error UnsubscribeListener(InstanceStatusListenerItf& listener) override
    {
        mListeners.erase(std::remove(mListeners.begin(), mListeners.end(), &listener), mListeners.end());

        return ErrorEnum::eNone;
    }

private:
    std::vector<InstanceStatus>             mStatuses;
    std::vector<InstanceStatusListenerItf*> mListeners;
};

} // namespace aos::cm::launcher

#endif
