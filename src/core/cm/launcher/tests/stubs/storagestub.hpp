/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CM_LAUNCHER_STUBS_STORAGESTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_STORAGESTUB_HPP_

#include <map>
#include <memory>
#include <vector>

#include <core/cm/launcher/itf/storage.hpp>

namespace aos::cm::launcher {

class StorageStub : public StorageItf {
public:
    void Init(const Array<InstanceInfo>& instances = Array<InstanceInfo>())
    {
        mInstanceInfo.clear();
        mOverrideEnvVarsRequest->mItems.Clear();
        mRunRequests.clear();

        for (const auto& instance : instances) {
            mInstanceInfo[instance.mInstanceIdent] = instance;
        }
    }

    Error AddInstance(const InstanceInfo& info) override
    {
        auto it = mInstanceInfo.find(info.mInstanceIdent);
        if (it != mInstanceInfo.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eAlreadyExist));
        }

        mInstanceInfo[info.mInstanceIdent] = info;

        return Error();
    }

    Error UpdateInstance(const InstanceInfo& info) override
    {
        auto it = mInstanceInfo.find(info.mInstanceIdent);
        if (it == mInstanceInfo.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound));
        }

        mInstanceInfo[info.mInstanceIdent] = info;

        return Error();
    }

    Error RemoveInstance(const InstanceIdent& instanceID) override
    {
        auto it = mInstanceInfo.find(instanceID);
        if (it == mInstanceInfo.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound));
        }

        mInstanceInfo.erase(it);

        return Error();
    }

    Error LoadActiveInstances(Array<InstanceInfo>& instances) const override
    {
        instances.Clear();

        for (const auto& pair : mInstanceInfo) {
            if (auto err = instances.PushBack(pair.second); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        return Error();
    }

    Error LoadOverrideEnvVars(OverrideEnvVarsRequest& envVars) const override
    {
        envVars = *mOverrideEnvVarsRequest;

        return Error();
    }

    Error SaveOverrideEnvVars(const OverrideEnvVarsRequest& envVars) override
    {
        *mOverrideEnvVarsRequest = envVars;

        return Error();
    }

    Error LoadRunRequests(Array<RunInstanceRequest>& requests) const override
    {
        return requests.Assign(Array<RunInstanceRequest>(mRunRequests.data(), mRunRequests.size()));
    }

    Error SaveRunRequests(const Array<RunInstanceRequest>& requests) override
    {
        mRunRequests.clear();
        mRunRequests.insert(mRunRequests.end(), requests.begin(), requests.end());

        return ErrorEnum::eNone;
    }

    bool HasInstance(const InstanceIdent& instanceID) const
    {
        return mInstanceInfo.find(instanceID) != mInstanceInfo.end();
    }

    void ClearInstances() { mInstanceInfo.clear(); }

private:
    std::map<InstanceIdent, InstanceInfo>   mInstanceInfo;
    std::unique_ptr<OverrideEnvVarsRequest> mOverrideEnvVarsRequest = std::make_unique<OverrideEnvVarsRequest>();
    std::vector<RunInstanceRequest>         mRunRequests;
};

} // namespace aos::cm::launcher

#endif
