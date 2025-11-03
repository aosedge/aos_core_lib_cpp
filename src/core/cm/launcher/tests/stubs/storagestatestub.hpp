/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_STORAGESTATESTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_STORAGESTATESTUB_HPP_

#include <map>
#include <vector>

#include <core/cm/storagestate/itf/storagestate.hpp>
#include <core/common/crypto/itf/hash.hpp>

namespace aos::cm::storagestate {

class StorageStateStub : public StorageStateItf {
public:
    // Constants
    static constexpr uint8_t cMagicSum[] = {0x12, 0x34, 0x56, 0x78};

    // Setters

    void Init()
    {
        mInstances.clear();
        mRemovedInstances.clear();
        mCleanedInstances.clear();
        mCheckSums.clear();
    }

    void SetInstanceCheckSum(const InstanceIdent& instanceIdent, const Array<uint8_t>& checkSum)
    {
        mCheckSums[instanceIdent] = checkSum;
    }

    void SetTotalStateSize(size_t size) { mTotalStateSize = size; }
    void SetTotalStorageSize(size_t size) { mTotalStorageSize = size; }

    // Getters
    const std::vector<InstanceIdent>&           GetRemovedInstances() const { return mRemovedInstances; }
    const std::vector<InstanceIdent>&           GetCleanedInstances() const { return mCleanedInstances; }
    const std::map<InstanceIdent, SetupParams>& GetInstances() const { return mInstances; }

    const std::map<InstanceIdent, StaticArray<uint8_t, crypto::cSHA256Size>>& GetCheckSums() const
    {
        return mCheckSums;
    }

    // StorageStateItf implementation
    Error Setup(const InstanceIdent& instanceIdent, const SetupParams& setupParams, String& storagePath,
        String& statePath) override
    {
        mInstances[instanceIdent] = setupParams;
        storagePath               = "storage_path";
        statePath                 = "state_path";

        return ErrorEnum::eNone;
    }

    Error Cleanup(const InstanceIdent& instanceIdent) override
    {
        mCleanedInstances.push_back(instanceIdent);

        return ErrorEnum::eNone;
    }

    Error Remove(const InstanceIdent& instanceIdent) override
    {
        mRemovedInstances.push_back(instanceIdent);

        return ErrorEnum::eNone;
    }

    Error GetInstanceCheckSum(const InstanceIdent& instanceIdent, Array<uint8_t>& checkSum) override
    {
        auto it = mCheckSums.find(instanceIdent);
        if (it == mCheckSums.end()) {
            return ErrorEnum::eNotFound;
        }

        checkSum = it->second;

        return ErrorEnum::eNone;
    }

    RetWithError<size_t> GetTotalStateSize() const override { return RetWithError<size_t>(mTotalStateSize); }

    RetWithError<size_t> GetTotalStorageSize() const override { return RetWithError<size_t>(mTotalStorageSize); }

    bool IsSamePartition() const override { return true; }

private:
    std::map<InstanceIdent, SetupParams>                               mInstances;
    std::vector<InstanceIdent>                                         mRemovedInstances;
    std::vector<InstanceIdent>                                         mCleanedInstances;
    std::map<InstanceIdent, StaticArray<uint8_t, crypto::cSHA256Size>> mCheckSums;
    size_t                                                             mTotalStateSize   = 1024;
    size_t                                                             mTotalStorageSize = 1024;
};

} // namespace aos::cm::storagestate

#endif
