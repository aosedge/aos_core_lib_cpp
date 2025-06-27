/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_STORAGESTATE_HPP_
#define AOS_STORAGESTATE_HPP_

#include "aos/common/cloudprotocol/state.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/tools/thread.hpp"
#include "aos/common/types.hpp"

namespace aos::cm::storagestate {

/**
 * Storage state interface.
 */
class StorageStateItf : public NonCopyable {
public:
    /**
     * Setups storage state instance.
     *
     * @param storagePath[out] storage path.
     * @param statePath[out] state path.
     * @return Error.
     */
    virtual Error Setup(String& storagePath, String& statePath) = 0;

    /**
     * Clean-ups storage state  instance.
     *
     * @param instanceIdent instance ident.
     * @return Error.
     */
    virtual Error Cleanup(const InstanceIdent& instanceIdent) = 0;

    /**
     * Removes storage state instance.
     *
     * @param instanceIdent instance ident.
     * @return Error.
     */
    virtual Error RemoveServiceInstance(const InstanceIdent& instanceIdent) = 0;

    /**
     * Updates storage state with new state.
     *
     * @param state new state.
     * @return Error.
     */
    virtual Error UpdateState(const cloudprotocol::UpdateState& state) = 0;

    /**
     * Accepts state from storage.
     *
     * @param state state to accept.
     * @return Error.
     */
    virtual Error StateAcceptance(const cloudprotocol::StateAcceptance& state) = 0;

    /**
     * Returns instance's checksum.
     *
     * @param instanceIdent instance ident.
     * @param checkSum[out] checksum.
     * @return Error
     */
    virtual Error GetInstanceCheckSum(const InstanceIdent& instanceIdent, String& checkSum) = 0;

    /**
     * Destructor.
     */
    virtual ~StorageStateItf() = default;
};

} // namespace aos::cm::storagestate

#endif
