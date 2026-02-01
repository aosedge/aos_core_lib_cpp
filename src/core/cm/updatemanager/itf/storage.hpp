/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_ITF_STORAGE_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_ITF_STORAGE_HPP_

#include <core/common/types/desiredstatus.hpp>

namespace aos::cm::updatemanager {

/**
 * @addtogroup cm_updatemanager
 * @{
 */

/**
 * Update state.
 */
class UpdateStateType {
public:
    enum class Enum {
        eNone,
        eDownloading,
        ePending,
        eInstalling,
        eLaunching,
        eFinalizing,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "none",
            "downloading",
            "pending",
            "installing",
            "launching",
            "finalizing",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using UpdateStateEnum = UpdateStateType::Enum;
using UpdateState     = EnumStringer<UpdateStateType>;

/**
 * Update manager storage interface.
 */
class StorageItf {
public:
    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;

    /**
     * Stores desired status in storage.
     *
     * @param desiredStatus desired status to store.
     * @return Error.
     */
    virtual Error StoreDesiredStatus(const DesiredStatus& desiredStatus) = 0;

    /**
     * Stores update state in storage.
     *
     * @param state update state to store.
     * @return Error.
     */
    virtual Error StoreUpdateState(const UpdateState& state) = 0;

    /**
     * Retrieves desired status from storage.
     *
     * @param desiredStatus desired status to retrieve.
     * @return Error.
     */
    virtual Error GetDesiredStatus(DesiredStatus& desiredStatus) = 0;

    /**
     * Retrieves update state from storage.
     *
     * @return RetWithError<UpdateState>.
     */
    virtual RetWithError<UpdateState> GetUpdateState() = 0;
};

/** @}*/

} // namespace aos::cm::updatemanager

#endif
