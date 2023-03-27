/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>

#include <gtest/gtest.h>

#include "aos/common/tools/log.hpp"

using namespace aos;

class TestLog : private NonCopyable {
public:
    static void LogCallback(LogModule module, LogLevel level, const String& message)
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

    bool CheckLog(LogModule module, LogLevel level, const String& message)
    {
        if (module != mLogModule) {
            return false;
        }

        if (level != mLogLevel) {
            return false;
        }

        if (mLogMessage != message) {
            return false;
        }

        return true;
    };

private:
    TestLog()
        : mLogModule()
        , mLogLevel()
    {
    }

    LogModule                      mLogModule;
    LogLevel                       mLogLevel;
    StaticString<Log::cMaxLineLen> mLogMessage;
};

class TestStringer : public Stringer {
public:
    explicit TestStringer(const char* str)
        : mStr(str)
    {
    }

    const String ToString() const override { return String(mStr); }

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

    for (size_t i = 0; i <= Log::cMaxLineLen;) {
        auto word = "word ";

        i += std::string(word).length();
        longString += word;
    }

    LOG_DBG() << longString.c_str();

    longString.resize(Log::cMaxLineLen - 3);
    longString += "...";

    EXPECT_TRUE(testLog.CheckLog(LogModuleEnum::eDefault, LogLevelEnum::eDebug, longString.c_str()));

    // Test log level strings

    EXPECT_EQ(LogLevel(LogLevelEnum::eDebug).ToString(), "debug");
    EXPECT_EQ(LogLevel(LogLevelEnum::eInfo).ToString(), "info");
    EXPECT_EQ(LogLevel(LogLevelEnum::eWarning).ToString(), "warning");
    EXPECT_EQ(LogLevel(LogLevelEnum::eError).ToString(), "error");

    // Test log module strings

    EXPECT_EQ(LogModule(LogModuleEnum::eSMLauncher).ToString(), "launcher");
    EXPECT_EQ(LogModule(LogModuleEnum::eSMServiceManager).ToString(), "servicemanager");
    EXPECT_EQ(LogModule(LogModuleEnum::eIAMCertHandler).ToString(), "certhandler");

    // Test error with function name and line number

    auto err = Error(ErrorEnum::eFailed, "file.cpp", 123);

    LOG_ERR() << "This is error: " << err;
    EXPECT_TRUE(
        testLog.CheckLog(LogModuleEnum::eDefault, LogLevelEnum::eError, "This is error: failed (file.cpp:123)"));
}
