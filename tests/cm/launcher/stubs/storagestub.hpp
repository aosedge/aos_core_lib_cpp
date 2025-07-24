/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CM_LAUNCHER_STUBS_STORAGESTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_STORAGESTUB_HPP_

#include <aos/cm/launcher/storage.hpp>
#include <map>

namespace aos::cm::storage {

/**
 * Stub implementation for StorageItf interface.
 */
class StorageStub : public StorageItf {
public:
    /**
     * Initializes stub object with initial instances.
     *
     * @param instances initial instance information array.
     */
    void Init(const Array<InstanceInfo>& instances)
    {
        mInstanceInfo.clear();

        for (size_t i = 0; i < instances.Size(); ++i) {
            mInstanceInfo[instances[i].mInstanceID] = instances[i];
        }
    }

    /**
     * Adds a new instance to the storage.
     *
     * @param info instance information.
     * @return Error.
     */
    Error AddInstance(const InstanceInfo& info) override
    {
        auto it = mInstanceInfo.find(info.mInstanceID);
        if (it != mInstanceInfo.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eAlreadyExist));
        }

        mInstanceInfo[info.mInstanceID] = info;
        return Error();
    }

    /**
     * Updates an existing instance in the storage.
     *
     * @param info updated instance information.
     * @return Error.
     */
    Error UpdateInstance(const InstanceInfo& info) override
    {
        auto it = mInstanceInfo.find(info.mInstanceID);
        if (it == mInstanceInfo.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound));
        }

        mInstanceInfo[info.mInstanceID] = info;
        return Error();
    }

    /**
     * Removes an instance from the storage.
     *
     * @param instanceID instance identifier.
     * @return Error.
     */
    Error RemoveInstance(const InstanceIdent& instanceID) override
    {
        auto it = mInstanceInfo.find(instanceID);
        if (it == mInstanceInfo.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound));
        }

        mInstanceInfo.erase(it);
        return Error();
    }

    /**
     * Get information about a stored instance.
     *
     * @param instanceID instance identifier.
     * @param[out] info instance info.
     * @return Error.
     */
    Error GetInstance(const InstanceIdent& instanceID, InstanceInfo& info) const override
    {
        auto it = mInstanceInfo.find(instanceID);
        if (it == mInstanceInfo.end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eNotFound));
        }

        info = it->second;
        return Error();
    }

    /**
     * Get all stored instances.
     *
     * @param[out] instances all stored instances.
     * @return Error.
     */
    Error GetInstances(Array<InstanceInfo>& instances) const override
    {
        instances.Clear();

        for (const auto& pair : mInstanceInfo) {
            if (auto err = instances.PushBack(pair.second); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        return Error();
    }

    /**
     * Helper method to check if instance exists.
     *
     * @param instanceID instance identifier.
     * @return true if instance exists, false otherwise.
     */
    bool HasInstance(const InstanceIdent& instanceID) const
    {
        return mInstanceInfo.find(instanceID) != mInstanceInfo.end();
    }

    /**
     * Helper method to clear all instances.
     */
    void ClearInstances() { mInstanceInfo.clear(); }

private:
    std::map<InstanceIdent, InstanceInfo> mInstanceInfo;
};

} // namespace aos::cm::storage

#endif
