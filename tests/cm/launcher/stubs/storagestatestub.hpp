/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CM_LAUNCHER_STUBS_STORAGESTATESTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_STORAGESTATESTUB_HPP_

#include "aos/cm/storagestate/storagestate.hpp"

#include <array>
#include <vector>

namespace aos::cm::storagestate {

/**
 * Stub implementation of StorageStateItf interface
 */
class StorageStateStub : public StorageStateItf {
public:
    /**
     * Initializes stub object.
     */
    void Init()
    {
        mCleanedInstances.clear();
        mRemovedInstances.clear();
    }

    /**
     * Setups storage state instance.
     *
     * @param setupParams setup parameters.
     * @param storagePath[out] storage path.
     * @param statePath[out] state path.
     * @return Error.
     */
    Error Setup(const SetupParams& setupParams, String& storagePath, String& statePath) override
    {
        (void)setupParams;

        storagePath = "";
        statePath   = "";

        return ErrorEnum::eNone;
    }

    /**
     * Clean-ups storage state instance.
     *
     * @param instanceIdent instance ident.
     * @return Error.
     */
    Error Cleanup(const InstanceIdent& instanceIdent) override
    {
        mCleanedInstances.push_back(instanceIdent);

        return ErrorEnum::eNone;
    }

    /**
     * Removes storage state instance.
     *
     * @param instanceIdent instance ident.
     * @return Error.
     */
    Error Remove(const InstanceIdent& instanceIdent) override
    {
        mRemovedInstances.push_back(instanceIdent);

        return ErrorEnum::eNone;
    }

    /**
     * Updates storage state with new state.
     *
     * @param state new state.
     * @return Error.
     */
    Error UpdateState(const cloudprotocol::UpdateState& state) override
    {
        (void)state;

        return ErrorEnum::eNone;
    }

    /**
     * Accepts state from storage.
     *
     * @param state state to accept.
     * @return Error.
     */
    Error AcceptState(const cloudprotocol::StateAcceptance& state) override
    {
        (void)state;

        return ErrorEnum::eNone;
    }

    /**
     * Returns instance's checksum.
     *
     * @param instanceIdent instance ident.
     * @param[out] checkSum checksum.
     * @return Error
     */
    Error GetInstanceCheckSum(const InstanceIdent& instanceIdent, String& checkSum) override
    {
        (void)instanceIdent;

        checkSum = cMagicSum;

        return ErrorEnum::eNone;
    }

    /**
     * Returns list of cleaned instances.
     *
     * @return const std::vector<InstanceIdent>&.
     */
    const std::vector<InstanceIdent>& GetCleanedInstances() const { return mCleanedInstances; }

    /**
     * Returns list of removed instances.
     *
     * @return const std::vector<InstanceIdent>&.
     */
    const std::vector<InstanceIdent>& GetRemovedInstances() const { return mRemovedInstances; }

    /**
     * Magic checksum.
     */
    static constexpr auto cMagicSum = "magic-sum";

private:
    std::vector<InstanceIdent> mCleanedInstances;
    std::vector<InstanceIdent> mRemovedInstances;
};

} // namespace aos::cm::storagestate

#endif
