/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_DATABASE_ITF_DATABASE_HPP_
#define AOS_CORE_CM_DATABASE_ITF_DATABASE_HPP_

#include <core/cm/imagemanager/itf/storage.hpp>
#include <core/cm/launcher/itf/storage.hpp>
#include <core/cm/storagestate/itf/storage.hpp>

namespace aos::cm::database {

/** @addtogroup cm Communication Manager
 *  @{
 */

/**
 * Database interface.
 */
class DatabaseItf : public imagemanager::StorageItf, public launcher::StorageItf, public storagestate::StorageItf { };

/** @}*/

} // namespace aos::cm::database

#endif
