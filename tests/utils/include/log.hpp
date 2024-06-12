/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_LOG_HPP_
#define TEST_LOG_HPP_

#include "aos/common/tools/log.hpp"

#define LOG_DBG() LOG_MODULE_DBG(aos::LogModuleEnum::eDefault)
#define LOG_INF() LOG_MODULE_INF(aos::LogModuleEnum::eDefault)
#define LOG_WRN() LOG_MODULE_WRN(aos::LogModuleEnum::eDefault)
#define LOG_ERR() LOG_MODULE_ERR(aos::LogModuleEnum::eDefault)

namespace aos {

inline void InitLogs()
{
    Log::SetCallback([](LogModule module, LogLevel level, const String& message) {
        static std::mutex           sLogMutex;
        std::lock_guard<std::mutex> lock(sLogMutex);

        std::cout << level.ToString().CStr() << " | " << module.ToString().CStr() << " | " << message.CStr()
                  << std::endl;
    });
}

} // namespace aos

#endif
