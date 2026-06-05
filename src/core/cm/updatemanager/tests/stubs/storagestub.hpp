/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_TESTS_STUBS_STORAGE_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_TESTS_STUBS_STORAGE_HPP_

#include <core/cm/updatemanager/itf/storage.hpp>

namespace aos::cm::updatemanager {

/**
 * Storage stub.
 */
class StorageStub : public StorageItf {
public:
    /**
     * Stores desired status in storage.
     *
     * @param desiredStatus desired status to store.
     * @return Error.
     */
    Error StoreDesiredStatus(const DesiredStatus& desiredStatus) override
    {
        mDesiredStatus = desiredStatus;

        return ErrorEnum::eNone;
    }

    /**
     * Stores update state in storage.
     *
     * @param state update state to store.
     * @return Error.
     */
    Error StoreUpdateState(const UpdateState& state) override
    {
        mUpdateState = state;

        return ErrorEnum::eNone;
    }

    /**
     * Retrieves desired status from storage.
     *
     * @param desiredStatus desired status to retrieve.
     * @return Error.
     */
    Error GetDesiredStatus(DesiredStatus& desiredStatus) override
    {
        desiredStatus = mDesiredStatus;

        return ErrorEnum::eNone;
    }

    /**
     * Retrieves update state from storage.
     *
     * @return RetWithError<UpdateState>.
     */
    RetWithError<UpdateState> GetUpdateState() override
    {
        return RetWithError<UpdateState>(mUpdateState, ErrorEnum::eNone);
    }

private:
    DesiredStatus mDesiredStatus;
    UpdateState   mUpdateState;
};

} // namespace aos::cm::updatemanager

#endif
