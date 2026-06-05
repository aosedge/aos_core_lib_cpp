/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CM_LAUNCHER_STUBS_STORAGESTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_STORAGESTUB_HPP_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <core/cm/launcher/itf/storage.hpp>

namespace aos::cm::launcher {

class StorageStub : public StorageItf {
public:
    void Init(const Array<InstanceInfo>& instances   = Array<InstanceInfo>(),
        const Array<RunInstanceRequest>& runRequests = Array<RunInstanceRequest>())
    {
        mInstanceInfo.clear();
        mOverrideEnvVarsRequest->mItems.Clear();
        mRunRequests.clear();

        for (const auto& instance : instances) {
            mInstanceInfo[StorageKey {instance.mInstanceIdent, std::string(instance.mVersion.CStr())}] = instance;
        }

        mRunRequests.insert(mRunRequests.end(), runRequests.begin(), runRequests.end());
    }

    Error AddInstance(const InstanceInfo& info) override
    {
        StorageKey key {info.mInstanceIdent, std::string(info.mVersion.CStr())};
        auto       it = mInstanceInfo.find(key);
        if (it != mInstanceInfo.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eAlreadyExist));
        }

        mInstanceInfo.emplace(std::move(key), info);

        return Error();
    }

    Error UpdateInstance(const InstanceInfo& info) override
    {
        auto it = mInstanceInfo.find(StorageKey {info.mInstanceIdent, std::string(info.mVersion.CStr())});
        if (it == mInstanceInfo.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound));
        }

        it->second = info;

        return Error();
    }

    Error RemoveInstance(const InstanceIdent& instanceID, const String& version) override
    {
        auto it = mInstanceInfo.find(StorageKey {instanceID, std::string(version.CStr())});
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

    bool HasInstance(const InstanceIdent& instanceID, const String& version) const
    {
        return mInstanceInfo.find(StorageKey {instanceID, std::string(version.CStr())}) != mInstanceInfo.end();
    }

    void ClearInstances() { mInstanceInfo.clear(); }

private:
    using StorageKey = std::pair<InstanceIdent, std::string>;

    std::map<StorageKey, InstanceInfo>      mInstanceInfo;
    std::unique_ptr<OverrideEnvVarsRequest> mOverrideEnvVarsRequest = std::make_unique<OverrideEnvVarsRequest>();
    std::vector<RunInstanceRequest>         mRunRequests;
};

} // namespace aos::cm::launcher

#endif
