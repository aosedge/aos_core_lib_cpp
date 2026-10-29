/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_CURRENTNODE_ITF_HWINFOPROVIDER_HPP_
#define AOS_CORE_IAM_CURRENTNODE_ITF_HWINFOPROVIDER_HPP_

#include <core/common/tools/string.hpp>

namespace aos::iam::currentnode {

/**
 * Hardware information provider interface.
 */
class HWInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~HWInfoProviderItf() = default;

    /**
     * Gets hardware ID.
     *
     * @param hwID hardware ID.
     * @return Error.
     */
    virtual Error GetHWID(String& hwID) = 0;
};

} // namespace aos::iam::currentnode

#endif
