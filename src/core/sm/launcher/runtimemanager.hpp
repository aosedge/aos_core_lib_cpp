/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_LAUNCHER_RUNTIMEMANAGER_HPP_
#define AOS_CORE_SM_LAUNCHER_RUNTIMEMANAGER_HPP_

#include <core/common/tools/array.hpp>
#include <core/common/tools/error.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/string.hpp>
#include <core/common/types/instance.hpp>

#include "itf/runtime.hpp"

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Keeps the node runtimes with their runtime IDs.
 *
 * The runtime manager is not thread safe: the launcher serializes the access to it.
 */
class RuntimeManager {
public:
    /**
     * Initializes runtime manager.
     *
     * @param allocator allocator to use for temporary objects.
     * @param runtimes node runtimes.
     * @return Error.
     */
    Error Init(AllocatorItf& allocator, const Array<RuntimeItf*>& runtimes);

    /**
     * Starts the runtimes and reads their runtime IDs, by which the runtimes are found.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops all runtimes, even if some of them fail to stop.
     *
     * @return Error the first stop error.
     */
    Error Stop();

    /**
     * Finds runtime by ID.
     *
     * @param runtimeID runtime ID.
     * @return RuntimeItf* nullptr if not found.
     */
    RuntimeItf* Find(const String& runtimeID);

    /**
     * Returns the runtime infos.
     *
     * @param[out] runtimes runtime infos.
     * @return Error.
     */
    Error GetRuntimesInfos(Array<RuntimeInfo>& runtimes) const;

    /**
     * Passes to each runtime the instances which run on it.
     *
     * @param instances instances.
     * @return Error.
     */
    Error InitInstances(const Array<InstanceInfo>& instances);

    /**
     * Reboots the runtimes. An unknown runtime and a reboot failure are logged.
     *
     * @param runtimeIDs IDs of the runtimes to reboot.
     */
    void Reboot(const Array<StaticString<cIDLen>>& runtimeIDs);

private:
    AllocatorItf*                                                     mAllocator {};
    StaticMap<RuntimeItf*, StaticString<cIDLen>, cMaxNumNodeRuntimes> mRuntimes;
};

/** @}*/

} // namespace aos::sm::launcher

#endif
