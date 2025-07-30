/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TOOLS_LOGGER_HPP_
#define AOS_CORE_COMMON_TOOLS_LOGGER_HPP_

#include "log.hpp"

#ifndef LOG_MODULE
#define LOG_MODULE "default"
#endif

#define LOG_DBG() LOG_MODULE_DBG(LOG_MODULE)
#define LOG_INF() LOG_MODULE_INF(LOG_MODULE)
#define LOG_WRN() LOG_MODULE_WRN(LOG_MODULE)
#define LOG_ERR() LOG_MODULE_ERR(LOG_MODULE)

#endif
