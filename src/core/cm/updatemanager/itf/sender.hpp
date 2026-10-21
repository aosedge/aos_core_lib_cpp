/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_ITF_SENDER_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_ITF_SENDER_HPP_

#include <core/common/types/certificates.hpp>
#include <core/common/types/unitstatus.hpp>

namespace aos::cm::updatemanager {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Sender interface.
 */
class SenderItf {
public:
    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;

    /**
     * Sends unit status.
     *
     * @param unitStatus unit status.
     * @return Error.
     */
    virtual Error SendUnitStatus(const UnitStatus& unitStatus) = 0;

    /**
     * Sends unit root certificates.
     *
     * @param unitRootCertificates unit root certificates.
     * @return Error.
     */
    virtual Error SendUnitRootCertificates(const UnitRootCertificates& unitRootCertificates) = 0;
};

/** @}*/

} // namespace aos::cm::updatemanager

#endif
