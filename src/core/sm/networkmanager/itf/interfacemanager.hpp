/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NETWORKMANAGER_ITF_INTERFACEMANAGER_HPP_
#define AOS_CORE_SM_NETWORKMANAGER_ITF_INTERFACEMANAGER_HPP_

#include <core/common/types/common.hpp>

namespace aos::sm::networkmanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Network interface manager interface.
 */
class InterfaceManagerItf {
public:
    /**
     * Destructor.
     */
    virtual ~InterfaceManagerItf() = default;

    /**
     * Removes interface.
     *
     * @param ifname interface name.
     * @return Error.
     */
    virtual Error DeleteLink(const String& ifname) = 0;

    /**
     * Sets up link.
     *
     * @param ifname interface name.
     * @return Error.
     */
    virtual Error SetupLink(const String& ifname) = 0;

    /**
     * Sets master.
     *
     * @param ifname interface name.
     * @param master master interface name.
     * @return Error.
     */
    virtual Error SetMasterLink(const String& ifname, const String& master) = 0;
};

/** @}*/

} // namespace aos::sm::networkmanager

#endif
