/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CERTHANDLER_HPP_
#define CERTHANDLER_HPP_

#include "aos/common/error.hpp"

namespace aos {
namespace iam {
namespace certhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Handles keys and certificates.
 */
class CertHandler {
public:
    /**
     * Creates key.
     *
     * @return Error
     */
    Error CreateKey();
};

/** @}*/

} // namespace certhandler
} // namespace iam
} // namespace aos

#endif
