/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_DATABASE_ITF_DATABASE_HPP_
#define AOS_CORE_SM_DATABASE_ITF_DATABASE_HPP_

#include <core/sm/imagemanager/itf/storage.hpp>
#include <core/sm/launcher/itf/storage.hpp>
#include <core/sm/networkmanager/itf/storage.hpp>

namespace aos::sm::database {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Database interface.
 */
class DatabaseItf : public imagemanager::StorageItf, public launcher::StorageItf, public networkmanager::StorageItf { };

/** @}*/

} // namespace aos::sm::database

#endif
