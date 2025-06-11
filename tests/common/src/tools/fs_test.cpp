/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <stdio.h>

#include <gtest/gtest.h>

#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/memory.hpp"

#include "aos/test/log.hpp"

using namespace testing;

using namespace aos;

static const auto cBaseTestDir = std::filesystem::current_path() / "fs_test";

static void CreateFile(const char* path, const char* text = "test file", size_t permissions = 0666U)
{
    std::ofstream stream {path, std::ios_base::trunc};

    stream << text;
    stream.close();

    std::filesystem::permissions(path, std::filesystem::perms(permissions));
}

static void CheckFile(const char* path, const char* text, const size_t permissions)
{
    StaticArray<uint8_t, 100> data;
    Array<uint8_t>            expData {reinterpret_cast<const uint8_t*>(text), strlen(text)};

    EXPECT_TRUE(fs::ReadFile(path, data).IsNone());
    EXPECT_EQ(data, expData) << "Wrong data in file: " << path;
    EXPECT_EQ(std::filesystem::status(path).permissions(), std::filesystem::perms(permissions))
        << "Wrong permissions for file: " << path;
}

class FSTest : public Test {
private:
    void SetUp() override
    {
        fs::RemoveAll(cBaseTestDir.c_str());
        fs::MakeDirAll(cBaseTestDir.c_str());

        aos::test::InitLog();
    }
};

TEST_F(FSTest, AppendPath)
{
    const auto home  = "/home/root";
    const auto build = "work/aos_core_lib_cpp/build";

    StaticString<cFilePathLen> src1 = home;

    fs::AppendPath(src1, build);
    EXPECT_EQ(src1, "/home/root/work/aos_core_lib_cpp/build");

    StaticString<cFilePathLen> src2 = home;

    fs::AppendPath(src2, "misc/", build);
    EXPECT_EQ(src2, "/home/root/misc/work/aos_core_lib_cpp/build");

    StaticString<cFilePathLen> src3;

    fs::AppendPath(src3, home, build);
    EXPECT_EQ(src3, "/home/root/work/aos_core_lib_cpp/build");
}

TEST_F(FSTest, JoinPath)
{
    const auto home  = "/home/root";
    const auto build = "work/aos_core_lib_cpp/build";

    auto path1 = fs::JoinPath(home, "misc", build);
    EXPECT_EQ(path1, "/home/root/misc/work/aos_core_lib_cpp/build");
}

TEST_F(FSTest, Dir)
{
    const auto test1 = "/home/root/test.txt";

    auto path1 = fs::Dir(test1);
    EXPECT_EQ(path1, "/home/root");

    const auto test2 = "/home/root/";

    auto path2 = fs::Dir(test2);
    EXPECT_EQ(path2, "/home/root");
}

TEST_F(FSTest, DirExist)
{
    EXPECT_EQ(fs::DirExist(cBaseTestDir.c_str()), RetWithError<bool>(true));

    const auto notExistingDir = fs::JoinPath(cBaseTestDir.c_str(), "dir-doesnt-exist");

    EXPECT_EQ(fs::DirExist(notExistingDir), RetWithError<bool>(false));
}

TEST_F(FSTest, MakeDir)
{
    const auto testDir = cBaseTestDir / "make-dir-test";

    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(false));
    EXPECT_TRUE(fs::MakeDir(testDir.c_str()).IsNone());
    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(true));

    fs::RemoveAll(testDir.c_str());
}

TEST_F(FSTest, MakeDirPathExists)
{
    EXPECT_EQ(fs::DirExist(cBaseTestDir.c_str()), RetWithError<bool>(true));
    EXPECT_TRUE(fs::MakeDir(cBaseTestDir.c_str()).IsNone());
    EXPECT_EQ(fs::DirExist(cBaseTestDir.c_str()), RetWithError<bool>(true));
}

TEST_F(FSTest, MakeDirAll)
{
    const auto testDir  = cBaseTestDir / "make-dir-all-test";
    const auto childDir = testDir / "child";

    EXPECT_EQ(fs::DirExist(childDir.c_str()), RetWithError<bool>(false));
    EXPECT_TRUE(fs::MakeDirAll(childDir.c_str()).IsNone());
    EXPECT_EQ(fs::DirExist(childDir.c_str()), RetWithError<bool>(true));

    fs::RemoveAll(testDir.c_str());
}

TEST_F(FSTest, ClearDir)
{
    const auto testDir   = cBaseTestDir / "clear-dir-test";
    const auto childDir  = testDir / "child1/child2";
    const auto childFile = testDir / "test.txt";

    ASSERT_TRUE(std::filesystem::create_directories(childDir));
    CreateFile(childFile.c_str());

    EXPECT_EQ(fs::DirExist(childDir.c_str()), RetWithError<bool>(true));
    EXPECT_TRUE(fs::ClearDir(testDir.c_str()).IsNone());
    EXPECT_EQ(fs::DirExist(childDir.c_str()), RetWithError<bool>(false));

    EXPECT_TRUE(fs::Remove(testDir.c_str()).IsNone());
}

TEST_F(FSTest, RemoveFile)
{
    const auto testDir   = cBaseTestDir / "remove-file-test";
    const auto childFile = testDir / "test.txt";

    EXPECT_TRUE(std::filesystem::create_directories(testDir));
    CreateFile(childFile.c_str());

    EXPECT_TRUE(fs::Remove(childFile.c_str()).IsNone());
    EXPECT_FALSE(std::filesystem::exists(childFile));

    fs::Remove(testDir.c_str());
}

TEST_F(FSTest, RemoveDirEmpty)
{
    const auto testDir = cBaseTestDir / "remove-dir-empty-test";

    EXPECT_TRUE(std::filesystem::create_directories(testDir));

    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(true));
    EXPECT_TRUE(fs::Remove(testDir.c_str()).IsNone());
    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(false));
}

TEST_F(FSTest, RemoveDirNotEmpty)
{
    const auto testDir  = cBaseTestDir / "remove-dir-notempty-test";
    const auto childDir = testDir / "child1";

    EXPECT_TRUE(std::filesystem::create_directories(childDir));

    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(true));
    EXPECT_FALSE(fs::Remove(testDir.c_str()).IsNone());
    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(true));

    fs::RemoveAll(testDir.c_str());
}

TEST_F(FSTest, RemoveAllFile)
{
    const auto testDir   = cBaseTestDir / "remove-all-file-test";
    const auto childFile = testDir / "test.txt";

    EXPECT_TRUE(std::filesystem::create_directories(testDir));
    CreateFile(childFile.c_str());

    EXPECT_TRUE(fs::RemoveAll(childFile.c_str()).IsNone());
    EXPECT_FALSE(std::filesystem::exists(childFile));

    fs::Remove(testDir.c_str());
}

TEST_F(FSTest, RemoveAllDirNotEmpty)
{
    const auto testDir   = cBaseTestDir / "remove-all-dir-notempty-test";
    const auto childDir  = testDir / "child1";
    const auto childFile = testDir / "test.txt";

    EXPECT_TRUE(std::filesystem::create_directories(childDir));
    CreateFile(childFile.c_str());

    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(true));
    EXPECT_TRUE(fs::RemoveAll(testDir.c_str()).IsNone());
    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(false));
}

TEST_F(FSTest, RemoveAllNotExistingDir)
{
    const auto testDir = cBaseTestDir / "remove-all-not-existing-dir-test";

    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(false));
    EXPECT_TRUE(fs::RemoveAll(testDir.c_str()).IsNone());
}

TEST_F(FSTest, RenameNotExisting)
{
    const auto testFile = cBaseTestDir / "rename-not-existing-test.txt";
    const auto newFile  = cBaseTestDir / "rename-not-existing-new-test.txt";

    EXPECT_EQ(fs::DirExist(testFile.c_str()), RetWithError<bool>(false));
    EXPECT_FALSE(fs::Rename(testFile.c_str(), newFile.c_str()).IsNone());
}

TEST_F(FSTest, RenameFolder)
{
    const auto testDir  = cBaseTestDir / "rename-folder-test";
    const auto childDir = testDir / "child";
    const auto newDir   = cBaseTestDir / "rename-folder-new-test";

    EXPECT_TRUE(std::filesystem::create_directories(childDir));
    CreateFile((childDir / "test.txt").c_str());

    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(true));
    EXPECT_EQ(fs::DirExist(newDir.c_str()), RetWithError<bool>(false));

    EXPECT_TRUE(fs::Rename(testDir.c_str(), newDir.c_str()).IsNone());

    EXPECT_EQ(fs::DirExist(testDir.c_str()), RetWithError<bool>(false));
    EXPECT_TRUE(std::filesystem::exists(newDir / "child/test.txt"));
}

TEST_F(FSTest, ReadFile)
{
    const auto testFile      = cBaseTestDir / "read-file-test.txt";
    const auto wrongFileName = cBaseTestDir / "wrong-file-name.txt";

    const char text[] = "Hello World";

    const Array<uint8_t> expData {reinterpret_cast<const uint8_t*>(text), strlen(text)};

    CreateFile(testFile.c_str(), text);

    StaticArray<uint8_t, 100> bigBuff;
    StaticArray<uint8_t, 1>   smallBuff;

    EXPECT_TRUE(fs::ReadFile(testFile.c_str(), bigBuff).IsNone());
    EXPECT_EQ(expData, bigBuff);

    EXPECT_FALSE(fs::ReadFile(testFile.c_str(), smallBuff).IsNone());
    EXPECT_FALSE(fs::ReadFile(wrongFileName.c_str(), bigBuff).IsNone());

    fs::RemoveAll(testFile.c_str());
}

TEST_F(FSTest, ReadFileToString)
{
    const auto testFile      = cBaseTestDir / "read-file-to-string-test.txt";
    const auto wrongFileName = cBaseTestDir / "wrong-file-name.txt";

    const char text[] = "Hello World";

    CreateFile(testFile.c_str(), text);

    StaticString<100> bigBuff;
    StaticString<1>   smallBuff;

    EXPECT_TRUE(fs::ReadFileToString(testFile.c_str(), bigBuff).IsNone());
    EXPECT_EQ(bigBuff, text);

    EXPECT_FALSE(fs::ReadFileToString(testFile.c_str(), smallBuff).IsNone());
    EXPECT_FALSE(fs::ReadFileToString(wrongFileName.c_str(), bigBuff).IsNone());

    fs::RemoveAll(testFile.c_str());
}

TEST_F(FSTest, ReadLine)
{
    const auto testFile = cBaseTestDir / "read-line-test.txt";

    const char text[] = "Hello World0\nHello World1\nHello World2\n";

    CreateFile(testFile.c_str(), text);

    StaticString<100> bigBuff;
    StaticString<1>   smallBuff;

    size_t filePos = 0;

    auto fd = open(testFile.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    auto closeFile = DeferRelease(&fd, [](const int* fd) { close(*fd); });

    EXPECT_FALSE(fs::ReadLine(fd, std::string::npos, smallBuff).IsNone());

    EXPECT_EQ(fs::ReadLine(fd, filePos, smallBuff), ErrorEnum::eNotFound);

    EXPECT_EQ(fs::ReadLine(fd, 0, bigBuff, "\t"), ErrorEnum::eNotFound);

    EXPECT_TRUE(fs::ReadLine(fd, filePos, bigBuff).IsNone());
    EXPECT_EQ(bigBuff, "Hello World0");

    filePos += bigBuff.Size() + 1;

    EXPECT_TRUE(fs::ReadLine(fd, filePos, bigBuff).IsNone());
    EXPECT_EQ(bigBuff, "Hello World1");

    filePos += bigBuff.Size() + 1;

    EXPECT_TRUE(fs::ReadLine(fd, filePos, bigBuff).IsNone());
    EXPECT_EQ(bigBuff, "Hello World2");

    filePos += bigBuff.Size() + 1;

    fs::RemoveAll(testFile.c_str());

    EXPECT_FALSE(fs::ReadLine(fd, 0, smallBuff).IsNone());
}

TEST_F(FSTest, WriteFile)
{
    const auto newFile      = cBaseTestDir / "write-file-new.txt";
    const auto existingFile = cBaseTestDir / "write-file-overwrite.txt";

    CreateFile(existingFile.c_str(), "dlroW olleH", 0664);

    const char           text[] = "Hello World";
    const Array<uint8_t> data {reinterpret_cast<const uint8_t*>(text), strlen(text)};

    EXPECT_TRUE(fs::WriteFile(newFile.c_str(), data, 0664).IsNone());
    CheckFile(newFile.c_str(), text, 0664);

    EXPECT_TRUE(fs::WriteFile(existingFile.c_str(), data, 0664).IsNone());
    CheckFile(existingFile.c_str(), text, 0664);

    fs::RemoveAll(newFile.c_str());
    fs::RemoveAll(existingFile.c_str());
}

TEST_F(FSTest, WriteStringToFile)
{
    const auto newFile      = cBaseTestDir / "write-file-to-string-new.txt";
    const auto existingFile = cBaseTestDir / "write-file-to-string-overwrite.txt";

    CreateFile(existingFile.c_str(), "dlroW olleH", 0444);

    const char text[] = "Hello World";

    EXPECT_TRUE(fs::WriteStringToFile(newFile.c_str(), text, 0664).IsNone());
    CheckFile(newFile.c_str(), text, 0664);

    EXPECT_TRUE(fs::WriteStringToFile(existingFile.c_str(), text, 0666).IsNone());
    CheckFile(existingFile.c_str(), text, 0666);

    fs::RemoveAll(newFile.c_str());
    fs::RemoveAll(existingFile.c_str());
}

TEST_F(FSTest, DirIterator)
{
    const auto walkDirRoot = cBaseTestDir / "walk-dir-test";

    const std::vector folders = {
        walkDirRoot / "d1",
        walkDirRoot / "d2",
        walkDirRoot / "d3",
    };

    for (const auto& folder : folders) {
        ASSERT_TRUE(fs::MakeDirAll(folder.c_str()).IsNone());
    }

    std::vector<std::string> entries;

    for (auto iterator = fs::DirIterator(walkDirRoot.c_str()); iterator.Next();) {
        EXPECT_STREQ(walkDirRoot.c_str(), iterator.GetRootPath().CStr());

        if (iterator->mIsDir) {
            entries.push_back(iterator->mPath.CStr());
        }
    }

    EXPECT_EQ(entries.size(), folders.size());

    for (const auto& folder : folders) {
        EXPECT_TRUE(std::find(entries.begin(), entries.end(), folder.filename()) != entries.end());
    }

    auto iterator = fs::DirIterator((walkDirRoot / "not-existing-dir").c_str());
    EXPECT_FALSE(iterator.Next());
}

TEST_F(FSTest, CalculateSize)
{
    const auto walkDirRoot     = cBaseTestDir / "calculate-dir-test";
    const auto multipleSubDirs = walkDirRoot / "sd1" / "sd2" / "sd3";

    ASSERT_TRUE(fs::MakeDirAll(multipleSubDirs.c_str()).IsNone());
    CreateFile((multipleSubDirs / "fff.txt").c_str(), std::string(2048, 'a').c_str(), 0444);

    EXPECT_EQ(fs::CalculateSize(walkDirRoot.c_str()), RetWithError<size_t>(2048));

    const std::vector folders = {
        walkDirRoot / "d1",
        walkDirRoot / "d2",
        walkDirRoot / "d3",
    };

    for (const auto& folder : folders) {
        ASSERT_TRUE(fs::MakeDirAll(folder.c_str()).IsNone());

        CreateFile((folder / "f.txt").c_str(), std::string(1024, 'a').c_str(), 0444);
    }

    EXPECT_EQ(fs::CalculateSize(walkDirRoot.c_str()), RetWithError<size_t>(3 * 1024 + 2048));

    EXPECT_EQ(fs::CalculateSize("does-not-exists"), RetWithError<size_t>(0));
}

TEST_F(FSTest, CalculateNoMemory)
{
    constexpr auto cFilesInDir   = 5;
    constexpr auto cFileSize     = 1024U;
    constexpr auto cDirs1stLevel = 2 * cDirIteratorMaxSize;
    constexpr auto cDirs2ndLevel = 5;
    constexpr auto cExpectedSize = cFilesInDir * cFileSize * cDirs1stLevel;

    const auto walkDirRoot = cBaseTestDir / "calculate-dir-test-no-memory";

    auto cwd = walkDirRoot;
    for (size_t i = 0; i < cDirs1stLevel; ++i) {
        auto lvl1dir = cwd;
        lvl1dir /= "lvl1_sdir" + std::to_string(i);

        ASSERT_TRUE(fs::MakeDirAll(lvl1dir.c_str()).IsNone());

        auto lvl2dir = lvl1dir;
        for (size_t j = 0; j < cDirs2ndLevel; ++j) {
            lvl2dir /= "lvl2_sdir_" + std::to_string(j);

            ASSERT_TRUE(fs::MakeDirAll(lvl2dir.c_str()).IsNone());
        }

        for (size_t k = 0; k < cFilesInDir; ++k) {
            auto filePath = lvl2dir / ("file_" + std::to_string(k) + ".txt");
            CreateFile(filePath.c_str(), std::string(cFileSize, 'a').c_str(), 0444);
        }
    }

    EXPECT_EQ(fs::CalculateSize(walkDirRoot.c_str()), RetWithError<size_t>(cExpectedSize));
}
