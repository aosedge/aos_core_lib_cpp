/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_INTERFACEFACTORY_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_INTERFACEFACTORY_HPP_

#include <core/common/types/common.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Network interface factory interface.
 */
class InterfaceFactoryItf {
public:
    /**
     * Creates bridge interface.
     *
     * @param name bridge name.
     * @param ip IP address.
     * @param subnet subnet.
     * @return Error.
     */
    virtual Error CreateBridge(const String& name, const String& ip, const String& subnet) = 0;

    /**
     * Creates vlan interface.
     *
     * @param name vlan name.
     * @param vlanID vlan ID.
     * @return Error.
     */
    virtual Error CreateVlan(const String& name, uint64_t vlanID) = 0;

    /**
     * Creates a parameter-less link of the given kind (e.g. "ifb", "dummy").
     *
     * For link kinds that need only a name and a type. Other typed factories
     * (CreateBridge, CreateVlan, ...) remain the preferred entry point when
     * extra attributes are required.
     *
     * @param name link name.
     * @param kind link kind (rtnetlink link-type string).
     * @return Error.
     */
    virtual Error CreateLink(const String& name, const String& kind) = 0;

    /**
     * Destructor.
     */
    virtual ~InterfaceFactoryItf() = default;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
