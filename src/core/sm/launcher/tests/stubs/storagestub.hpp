/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_TESTS_STUBS_STORAGESTUB_HPP_
#define AOS_CORE_SM_LAUNCHER_TESTS_STUBS_STORAGESTUB_HPP_

#include <mutex>
#include <vector>

#include <core/sm/launcher/itf/storage.hpp>

namespace aos::sm::launcher {

class StorageStub : public StorageItf {
public:
    Error Init(const std::vector<InstanceInfo>& data)
    {
        std::lock_guard lock {mMutex};

        for (const auto& instance : data) {
            if (auto err = mData.PushBack(instance); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }

        return ErrorEnum::eNone;
    }

    void SetGetAllError(const Error& err)
    {
        std::lock_guard lock {mMutex};

        mGetAllError = err;
    }

    void SetUpdateError(const Error& err)
    {
        std::lock_guard lock {mMutex};

        mUpdateError = err;
    }

    void SetRemoveError(const Error& err)
    {
        std::lock_guard lock {mMutex};

        mRemoveError = err;
    }

    Error GetAllInstancesInfos(Array<InstanceInfo>& infos) override
    {
        std::lock_guard lock {mMutex};

        if (!mGetAllError.IsNone()) {
            return mGetAllError;
        }

        return infos.Assign(mData);
    }

    Error UpdateInstanceInfo(const InstanceInfo& info) override
    {
        std::lock_guard lock {mMutex};

        if (!mUpdateError.IsNone()) {
            return mUpdateError;
        }

        auto it = mData.FindIf([&info](const InstanceInfo& existingInfo) {
            return static_cast<const InstanceIdent&>(existingInfo) == static_cast<const InstanceIdent&>(info);
        });

        if (it != mData.end()) {
            *it = info;

            return ErrorEnum::eNone;
        }

        return mData.PushBack(info);
    }

    Error RemoveInstanceInfo(const InstanceIdent& ident) override
    {
        std::lock_guard lock {mMutex};

        if (!mRemoveError.IsNone()) {
            return mRemoveError;
        }

        return mData.RemoveIf(
                   [&ident](const InstanceInfo& info) { return static_cast<const InstanceIdent&>(info) == ident; })
            ? ErrorEnum::eNone
            : ErrorEnum::eNotFound;
    }

private:
    std::mutex        mMutex;
    InstanceInfoArray mData;
    Error             mGetAllError;
    Error             mUpdateError;
    Error             mRemoveError;
};

} // namespace aos::sm::launcher

#endif
