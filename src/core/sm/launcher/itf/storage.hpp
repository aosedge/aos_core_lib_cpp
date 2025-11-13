/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AOS_CORE_SM_LAUNCHER_ITF_STORAGE_HPP_
#define AOS_CORE_SM_LAUNCHER_ITF_STORAGE_HPP_

namespace aos::sm::launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Instance status sender interface.
 */
class StorageItf {
public:
    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;
};

} // namespace aos::sm::launcher

#endif
