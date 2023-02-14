/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LOG_HPP_
#define LOG_HPP_

#include "log/log.hpp"

#define LOG_DBG() LOG_MODULE_DBG(LogModuleEnum::eIAMCertHandler)
#define LOG_INF() LOG_MODULE_INF(LogModuleEnum::eIAMCertHandler)
#define LOG_WRN() LOG_MODULE_WRN(LogModuleEnum::eIAMCertHandler)
#define LOG_ERR() LOG_MODULE_ERR(LogModuleEnum::eIAMCertHandler)

#endif
