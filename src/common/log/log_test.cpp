// SPDX-License-Identifier: Apache-2.0
//
// Copyright (C) 2023 Renesas Electronics Corporation.
// Copyright (C) 2023 EPAM Systems, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include <gtest/gtest.h>

#include "log.hpp"
#include "utils/stringer.hpp"

using namespace aos;

class TestLog {
public:
    TestLog(TestLog const&) = delete;
    void operator=(TestLog const&) = delete;

    static void LogCallback(LogModule module, LogLevel level, const char* message)
    {
        auto& instance = GetInstance();

        instance.mLogModule = module;
        instance.mLogLevel = level;
        instance.mLogMessage = message;
    }

    static TestLog& GetInstance()
    {
        static TestLog sInstance;

        return sInstance;
    }

    bool CheckLog(LogModule module, LogLevel level, const char* message)
    {
        if (module != mLogModule) {
            return false;
        }

        if (level != mLogLevel) {
            return false;
        }

        if (mLogMessage == nullptr || strcmp(message, mLogMessage) != 0) {
            return false;
        }

        return true;
    };

private:
    TestLog()
        : mLogModule()
        , mLogLevel()
        , mLogMessage(nullptr)
    {
    }

    LogModule   mLogModule;
    LogLevel    mLogLevel;
    const char* mLogMessage;
};

class TestStringer : public Stringer {
public:
    explicit TestStringer(const char* str)
        : mStr(str)
    {
    }

    const char* ToString() const override { return mStr; }

private:
    const char* mStr;
};

#define LOG_DBG() LOG_MODULE_DBG(LogModuleEnum::eDefault)
#define LOG_INF() LOG_MODULE_INF(LogModuleEnum::eDefault)
#define LOG_WRN() LOG_MODULE_WRN(LogModuleEnum::eDefault)
#define LOG_ERR() LOG_MODULE_ERR(LogModuleEnum::eDefault)

TEST(common, Log)
{
    Log::SetCallback(TestLog::LogCallback);

    auto& testLog = TestLog::GetInstance();

    // Test log levels

    LOG_DBG() << "Debug log";
    EXPECT_TRUE(testLog.CheckLog(LogModuleEnum::eDefault, LogLevelEnum::eDebug, "Debug log"));

    LOG_INF() << "Info log";
    EXPECT_TRUE(testLog.CheckLog(LogModuleEnum::eDefault, LogLevelEnum::eInfo, "Info log"));

    LOG_WRN() << "Warning log";
    EXPECT_TRUE(testLog.CheckLog(LogModuleEnum::eDefault, LogLevelEnum::eWarning, "Warning log"));

    LOG_ERR() << "Error log";
    EXPECT_TRUE(testLog.CheckLog(LogModuleEnum::eDefault, LogLevelEnum::eError, "Error log"));

    // Test int

    LOG_DBG() << "Int value: " << 123;
    EXPECT_TRUE(testLog.CheckLog(LogModuleEnum::eDefault, LogLevelEnum::eDebug, "Int value: 123"));

    // Test stringer

    LOG_DBG() << TestStringer("This is test stringer");
    EXPECT_TRUE(testLog.CheckLog(LogModuleEnum::eDefault, LogLevelEnum::eDebug, "This is test stringer"));

    // Test unknown module

    LOG_MODULE_INF(static_cast<LogModuleEnum>(42)) << "Log of module 42";
    EXPECT_TRUE(testLog.CheckLog(static_cast<LogModuleEnum>(42), LogLevelEnum::eInfo, "Log of module 42"));

    // Test long log

    std::string longString;

    for (auto i = 0; i < CONFIG_LOG_LINE_SIZE;) {
        auto word = "word ";

        i += std::string(word).length();
        longString += word;
    }

    LOG_DBG() << longString.c_str();

    longString.resize(CONFIG_LOG_LINE_SIZE - 4);
    longString += "...";

    EXPECT_TRUE(testLog.CheckLog(LogModuleEnum::eDefault, LogLevelEnum::eDebug, longString.c_str()));

    // Test log level strings

    EXPECT_EQ(LogLevel(LogLevelEnum::eDebug).ToString(), "debug");
    EXPECT_EQ(LogLevel(LogLevelEnum::eInfo).ToString(), "info");
    EXPECT_EQ(LogLevel(LogLevelEnum::eWarning).ToString(), "warning");
    EXPECT_EQ(LogLevel(LogLevelEnum::eError).ToString(), "error");

    // Test log module strings

    EXPECT_EQ(LogModule(LogModuleEnum::eSMLauncher).ToString(), "launcher");
    EXPECT_EQ(LogModule(LogModuleEnum::eIAMCertHandler).ToString(), "certhandler");
}
