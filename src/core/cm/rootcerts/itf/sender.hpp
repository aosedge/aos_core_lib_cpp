/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_ROOTCERTS_ITF_SENDER_HPP_
#define AOS_CORE_CM_ROOTCERTS_ITF_SENDER_HPP_

#include <core/common/types/certificates.hpp>

namespace aos::cm::rootcerts {

/**
 * Unit root certificates sender interface.
 */
class SenderItf {
public:
    /**
     * Destructor.
     */
    virtual ~SenderItf() = default;

    /**
     * Sends unit root certificates.
     *
     * @param unitRootCertificates unit root certificates.
     * @return Error.
     */
    virtual Error SendUnitRootCertificates(const UnitRootCertificates& unitRootCertificates) = 0;
};

} // namespace aos::cm::rootcerts

#endif
