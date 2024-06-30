/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_LOG_HPP_
#define TEST_LOG_HPP_

#include <iostream>
#include <mutex>

#include "aos/common/tools/logger.hpp"

namespace aos {

inline void InitLogs()
{
    Log::SetCallback([](const char* module, LogLevel level, const String& message) {
        static std::mutex           sLogMutex;
        std::lock_guard<std::mutex> lock(sLogMutex);

        std::cout << level.ToString().CStr() << " | " << module << " | " << message.CStr() << std::endl;
    });
}

} // namespace aos

#endif
