/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "statusnotifier.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

void StatusNotifier::Init(AllocatorItf& allocator, SenderItf& sender)
{
    mAllocator = &allocator;
    mSender    = &sender;
}

Error StatusNotifier::Subscribe(instancestatusprovider::ListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe instance status listener";

    if (mListeners.Contains(&listener)) {
        return AOS_ERROR_WRAP(ErrorEnum::eAlreadyExist);
    }

    if (auto err = mListeners.EmplaceBack(&listener); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error StatusNotifier::Unsubscribe(instancestatusprovider::ListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unsubscribe instance status listener";

    return mListeners.Remove(&listener) == 0 ? AOS_ERROR_WRAP(ErrorEnum::eNotFound) : ErrorEnum::eNone;
}

void StatusNotifier::SendNodeStatuses(const InstanceRegistry& instances)
{
    LOG_INF() << "Send node instances statuses" << Log::Field("count", instances.Size());

    auto statuses = MakeUnique<InstanceStatusArray>(mAllocator);
    if (!statuses) {
        LOG_ERR() << "Failed to allocate instance statuses" << Log::Field(ErrorEnum::eNoMemory);

        return;
    }

    for (const auto& instance : instances) {
        LOG_INF() << "Node instance status" << Log::Field("instance", instance.mInfo)
                  << Log::Field("runtimeID", instance.mInfo.mRuntimeID) << Log::Field("state", instance.mStatus.mState)
                  << Log::Field(instance.mStatus.mError);

        if (auto err = statuses->EmplaceBack(instance.mStatus); !err.IsNone()) {
            LOG_ERR() << "Failed to add instance status to list" << Log::Field("instance", instance.mInfo)
                      << Log::Field(AOS_ERROR_WRAP(err));
        }
    }

    if (auto err = mSender->SendNodeInstancesStatuses(*statuses); !err.IsNone()) {
        LOG_ERR() << "Failed to send node instances statuses" << Log::Field(err);
    }
}

Error StatusNotifier::SendUpdateStatuses(const Array<InstanceStatus>& statuses)
{
    if (auto err = mSender->SendUpdateInstancesStatuses(statuses); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

void StatusNotifier::NotifyListeners(const Array<InstanceStatus>& statuses)
{
    LockGuard lock {mMutex};

    for (auto* listener : mListeners) {
        listener->OnInstancesStatusesChanged(statuses);
    }
}

} // namespace aos::sm::launcher
