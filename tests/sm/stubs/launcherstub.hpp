/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LAUNCHER_STUB_HPP_
#define AOS_LAUNCHER_STUB_HPP_

#include <algorithm>
#include <future>

#include "aos/sm/launcher.hpp"

namespace aos::sm::launcher {

/**
 * Storage stub.
 */
class StorageStub : public sm::launcher::StorageItf {
public:
    Error AddInstance(const InstanceData& instance) override
    {
        std::lock_guard lock {mMutex};

        if (std::find_if(mInstances.begin(), mInstances.end(),
                [&instance](const InstanceData& info) { return instance == info; })
            != mInstances.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        mInstances.push_back(instance);

        return ErrorEnum::eNone;
    }

    Error UpdateInstance(const InstanceData& instance) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mInstances.begin(), mInstances.end(),
            [&instance](const auto& info) { return instance.mInstanceID == info.mInstanceID; });
        if (it == mInstances.end()) {
            return ErrorEnum::eNotFound;
        }

        *it = instance;

        return ErrorEnum::eNone;
    }

    Error RemoveInstance(const String& instanceID) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mInstances.begin(), mInstances.end(),
            [&instanceID](const auto& instance) { return instance.mInstanceID == instanceID; });
        if (it == mInstances.end()) {
            return ErrorEnum::eNotFound;
        }

        mInstances.erase(it);

        return ErrorEnum::eNone;
    }

    Error GetAllInstances(Array<InstanceData>& instances) override
    {
        std::lock_guard lock {mMutex};

        for (const auto& instance : mInstances) {
            auto err = instances.PushBack(instance);
            if (!err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    RetWithError<uint64_t> GetOperationVersion() const override
    {
        std::lock_guard lock {mMutex};

        return {mOperationVersion, ErrorEnum::eNone};
    }

    Error SetOperationVersion(uint64_t version) override
    {
        std::lock_guard lock {mMutex};

        mOperationVersion = version;

        return ErrorEnum::eNone;
    }

    Error GetOverrideEnvVars(Array<cloudprotocol::EnvVarsInstanceInfo>& envVarsInstanceInfos) const override
    {
        std::lock_guard lock {mMutex};

        envVarsInstanceInfos = mEnvVarsInstanceInfos;

        return ErrorEnum::eNone;
    }

    Error SetOverrideEnvVars(const Array<cloudprotocol::EnvVarsInstanceInfo>& envVarsInstanceInfos) override
    {
        std::lock_guard lock {mMutex};

        mEnvVarsInstanceInfos = envVarsInstanceInfos;

        return ErrorEnum::eNone;
    }

    RetWithError<Time> GetOnlineTime() const override
    {
        std::lock_guard lock {mMutex};

        return {mOnlineTime, ErrorEnum::eNone};
    }

    Error SetOnlineTime(const Time& time) override
    {
        std::lock_guard lock {mMutex};

        mOnlineTime = time;

        return ErrorEnum::eNone;
    }

private:
    std::vector<InstanceData> mInstances;
    mutable std::mutex        mMutex;

    uint64_t                                mOperationVersion = launcher::Launcher::cOperationVersion;
    cloudprotocol::EnvVarsInstanceInfoArray mEnvVarsInstanceInfos;
    Time                                    mOnlineTime = Time::Now();
};

/**
 * Instance status receiver stub.
 */
class StatusReceiverStub : public InstanceStatusReceiverItf {
public:
    /**
     * Sends instances run status.
     *
     * @param instances instances status array.
     * @return Error.
     */
    Error InstancesRunStatus(const Array<InstanceStatus>& status) override
    {
        mPromise.set_value(status);

        return ErrorEnum::eNone;
    }

    /**
     * Sends instances update status.
     * @param instances instances status array.
     *
     * @return Error.
     */
    Error InstancesUpdateStatus(const Array<InstanceStatus>& status) override
    {
        mPromise.set_value(status);

        return ErrorEnum::eNone;
    }

    /**
     * Returns feature to wait when instances run status is received.
     *
     * @return future.
     */
    auto GetFeature()
    {
        mPromise = std::promise<const Array<InstanceStatus>>();

        return mPromise.get_future();
    }

private:
    std::promise<const Array<InstanceStatus>> mPromise;
};

} // namespace aos::sm::launcher

#endif
