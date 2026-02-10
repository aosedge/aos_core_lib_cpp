/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_LAUNCHER_NODEITF_HPP_
#define AOS_CORE_CM_LAUNCHER_NODEITF_HPP_

#include <core/cm/networkmanager/itf/networkmanager.hpp>
#include <core/common/types/instance.hpp>
#include <core/common/types/unitconfig.hpp>

namespace aos::cm::launcher {

/** @addtogroup CM Launcher
 *  @{
 */

/**
 * Provides interface for instance to reserve node resources and schedule itself on the node.
 */
class NodeItf {
public:
    /**
     * Destructor.
     */
    virtual ~NodeItf() = default;

    /**
     * Reserve runtime resources for instance.
     *
     * @param instanceIdent instance identifier.
     * @param runtimeID runtime identifier.
     * @param reqCPU requested CPU.
     * @param reqRAM requested RAM.
     * @param reqResources requested shared resources.
     * @return Error.
     */
    virtual Error ReserveResources(const InstanceIdent& instanceIdent, const String& runtimeID, size_t reqCPU,
        size_t reqRAM, const Array<StaticString<cResourceNameLen>>& reqResources)
        = 0;

    /**
     * Returns node configuration.
     *
     * @return const NodeConfig&
     */
    const NodeConfig& GetConfig() const { return mConfig; }

protected:
    NodeConfig mConfig {};
};

/** @}*/

} // namespace aos::cm::launcher

#endif
