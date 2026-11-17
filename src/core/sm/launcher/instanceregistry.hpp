/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_INSTANCEREGISTRY_HPP_
#define AOS_CORE_SM_LAUNCHER_INSTANCEREGISTRY_HPP_

#include <core/common/tools/array.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/tools/time.hpp>
#include <core/common/types/instance.hpp>

#include "itf/instanceidprovider.hpp"

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Launcher data of the node instance.
 */
struct InstanceData {
    InstanceInfo         mInfo;
    InstanceStatus       mStatus;
    StaticString<cIDLen> mInstanceID;
    Duration             mOfflineTTL;
    bool                 mConfigsLoaded {};
};

/**
 * Keeps the data of the node instances.
 *
 * The registry is not thread safe: the launcher serializes the access to it. The pointers to the instance data stay
 * valid until the instance is removed.
 */
class InstanceRegistry {
public:
    /**
     * Initializes instance registry.
     *
     * @param instanceIDProvider instance ID provider.
     */
    void Init(InstanceIDProviderItf& instanceIDProvider);

    /**
     * Adds instance in the inactive state.
     *
     * @param instanceInfo instance info.
     * @return RetWithError<InstanceData*>.
     */
    RetWithError<InstanceData*> Add(const InstanceInfo& instanceInfo);

    /**
     * Removes instance.
     *
     * @param instanceIdent instance ident.
     */
    void Remove(const InstanceIdent& instanceIdent);

    /**
     * Finds instance by ident.
     *
     * @param instanceIdent instance ident.
     * @return InstanceData* nullptr if not found.
     */
    InstanceData* Find(const InstanceIdent& instanceIdent);

    /**
     * Finds instance by ident.
     *
     * @param instanceIdent instance ident.
     * @return const InstanceData* nullptr if not found.
     */
    const InstanceData* Find(const InstanceIdent& instanceIdent) const;

    /**
     * Finds instance by instance ID.
     *
     * @param instanceID instance ID.
     * @return InstanceData* nullptr if not found.
     */
    InstanceData* FindByID(const String& instanceID);

    /**
     * Returns statuses of all instances.
     *
     * @param[out] statuses instances statuses.
     * @return Error.
     */
    Error GetStatuses(Array<InstanceStatus>& statuses) const;

    /**
     * Returns the minimal offline TTL of the active and activating instances.
     *
     * @return Optional<Duration> empty if no such instance has offline TTL.
     */
    Optional<Duration> GetMinOfflineTTL() const;

    /**
     * Returns number of instances.
     *
     * @return size_t.
     */
    size_t Size() const { return mInstances.Size(); }

    /**
     * Returns iterator to the first instance.
     *
     * @return InstanceData*.
     */
    InstanceData* begin() { return mInstances.begin(); }

    /**
     * Returns iterator past the last instance.
     *
     * @return InstanceData*.
     */
    InstanceData* end() { return mInstances.end(); }

    /**
     * Returns const iterator to the first instance.
     *
     * @return const InstanceData*.
     */
    const InstanceData* begin() const { return mInstances.begin(); }

    /**
     * Returns const iterator past the last instance.
     *
     * @return const InstanceData*.
     */
    const InstanceData* end() const { return mInstances.end(); }

private:
    InstanceIDProviderItf*                      mInstanceIDProvider {};
    StaticArray<InstanceData, cMaxNumInstances> mInstances;
};

/** @}*/

} // namespace aos::sm::launcher

#endif
