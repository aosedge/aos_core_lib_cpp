/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>

#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>

using namespace aos;

class TestLog : private NonCopyable {
public:
    static void LogCallback(const String& module, LogLevel level, const String& message)
    {
        auto& instance = GetInstance();

        instance.mLogModule  = module;
        instance.mLogLevel   = level;
        instance.mLogMessage = message;
    }

    static TestLog& GetInstance()
    {
        static TestLog sInstance;

        return sInstance;
    }

    bool CheckLog(const char* module, LogLevel level, const String& message)
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

    StaticString<64>               mLogModule;
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

TEST(LogTest, Basic)
{
    Log::SetCallback(TestLog::LogCallback);

    auto& testLog = TestLog::GetInstance();

    // Test log levels

    LOG_DBG() << "Debug log";
    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eDebug, "Debug log"));

    LOG_INF() << "Info log";
    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eInfo, "Info log"));

    LOG_WRN() << "Warning log";
    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eWarning, "Warning log"));

    LOG_ERR() << "Error log";
    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eError, "Error log"));

    // Test int

    LOG_DBG() << "Int value: " << 123;
    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eDebug, "Int value: 123"));

    // Test stringer

    LOG_DBG() << TestStringer("This is test stringer");
    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eDebug, "This is test stringer"));

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

    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eDebug, longString.c_str()));

    // Test log level strings

    EXPECT_EQ(LogLevel(LogLevelEnum::eDebug).ToString(), "debug");
    EXPECT_EQ(LogLevel(LogLevelEnum::eInfo).ToString(), "info");
    EXPECT_EQ(LogLevel(LogLevelEnum::eWarning).ToString(), "warning");
    EXPECT_EQ(LogLevel(LogLevelEnum::eError).ToString(), "error");

    // Test error with function name and line number

    auto err = Error(ErrorEnum::eFailed, "err=error", "file.cpp", 123);

    LOG_ERR() << "This is error: " << err;
    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eError, "This is error: err=error (file.cpp:123)"));

    err = Error(ErrorEnum::eFailed, "", "file.cpp", 123);
    LOG_ERR() << "This is error: " << err;
    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eError, "This is error: failed (file.cpp:123)"));

    // Test with key-value pairs

    String url      = "http://test.com";
    String path     = "/hello/world";
    auto   fileSize = 20;

    LOG_DBG() << "Download completed" << Log::Field("url", url) << Log::Field("path", path)
              << Log::Field("size", fileSize);
    EXPECT_TRUE(testLog.CheckLog(
        "default", LogLevelEnum::eDebug, "Download completed: url=http://test.com, path=/hello/world, size=20"));

    LOG_DBG() << "Downloaded" << Log::Field("path", path) << Log::Field("size", fileSize);
    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eDebug, "Downloaded: path=/hello/world, size=20"));

    LOG_ERR() << "Download failed" << Log::Field(err);
    EXPECT_TRUE(testLog.CheckLog("default", LogLevelEnum::eError, "Download failed: err=failed (file.cpp:123)"));

    LOG_ERR() << "Download failed" << Log::Field("path", path) << Log::Field(err);
    EXPECT_TRUE(testLog.CheckLog(
        "default", LogLevelEnum::eError, "Download failed: path=/hello/world, err=failed (file.cpp:123)"));
}
