/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fs.hpp"
#include "memory.hpp"

namespace aos::fs {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

Mutex                                     sCalculateSizeMutex;
StaticAllocator<sizeof(DirIteratorArray)> sCalculateSizeAllocator;

} // namespace

/***********************************************************************************************************************
 * DirIterator implementation
 **********************************************************************************************************************/

DirIterator::DirIterator(const String& path)
    : mDir(opendir(path.CStr()))
    , mRoot(path)
{
}

DirIterator::DirIterator(DirIterator&& other)
    : mDir(other.mDir)
    , mEntry(other.mEntry)
    , mRoot(other.mRoot)
{
    other.mDir = nullptr;
}

DirIterator& DirIterator::operator=(DirIterator&& other)
{
    if (this != &other) {
        mDir   = other.mDir;
        mEntry = other.mEntry;
        mRoot  = other.mRoot;

        other.mDir = nullptr;
    }

    return *this;
}

DirIterator::~DirIterator()
{
    if (mDir) {
        closedir(mDir);
    }
}

bool DirIterator::Next()
{
    if (mDir == nullptr) {
        return false;
    }

    struct dirent* entry = nullptr;

    while ((entry = readdir(mDir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        auto        path = JoinPath(mRoot, entry->d_name);
        struct stat entryStat;

        if (stat(path.CStr(), &entryStat) == -1) {
            return false;
        }

        mEntry.mPath  = entry->d_name;
        mEntry.mIsDir = S_ISDIR(entryStat.st_mode);

        return true;
    }

    return false;
}

/***********************************************************************************************************************
 * FileInfoProvider implementation
 **********************************************************************************************************************/

Error FileInfoProvider::Init(crypto::HasherItf& hashProvider)
{
    mHashProvider = &hashProvider;

    return ErrorEnum::eNone;
}

Error FileInfoProvider::GetFileInfo(const String& path, FileInfo& info)
{
    auto [size, err] = CalculateSize(path);
    if (!err.IsNone()) {
        return err;
    }

    info.mSize = size;

    return GetSHA256(path, info.mSHA256);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error FileInfoProvider::GetSHA256(const String& path, Array<uint8_t>& sha256)
{
    auto [hasherPtr, errHash] = mHashProvider->CreateHash(crypto::HashEnum::eSHA256);
    if (!errHash.IsNone()) {
        return errHash;
    }

    mReadFileBuffer.Clear();

    if (errHash = ReadFile(path, mReadFileBuffer); !errHash.IsNone()) {
        return errHash;
    }

    if (errHash = hasherPtr->Update(mReadFileBuffer); !errHash.IsNone()) {
        return errHash;
    }

    if (errHash = hasherPtr->Finalize(sha256); !errHash.IsNone()) {
        return errHash;
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * fs functions implementation
 **********************************************************************************************************************/

StaticString<cFilePathLen> Dir(const String& path)
{
    StaticString<cFilePathLen> dir;

    auto it = path.end();

    while (it != path.begin()) {
        it--;

        if (*it == '/') {
            break;
        }
    }

    dir.Insert(dir.end(), path.begin(), it);

    return dir;
}

RetWithError<bool> DirExist(const String& path)
{
    auto dir = opendir(path.CStr());
    if (dir == nullptr) {
        if (errno == ENOENT) {
            return false;
        }

        return {false, errno};
    }

    closedir(dir);

    return true;
}

RetWithError<bool> FileExist(const String& path)
{
    struct stat s;

    auto ret = stat(path.CStr(), &s);
    if (ret != 0) {
        if (errno == ENOENT) {
            return false;
        }

        return {false, errno};
    }

    if (S_ISREG(s.st_mode)) {
        return true;
    }

    return false;
}

Error MakeDir(const String& path)
{
    auto ret = mkdir(path.CStr(), S_IRWXU | S_IRWXG | S_IRWXO);
    if (ret != 0 && errno != EEXIST) {
        return errno;
    }

    return ErrorEnum::eNone;
}

Error MakeDirAll(const String& path)
{
    auto it = path.begin();

    while (it != path.end()) {
        if (*it == '/') {
            it++;
        }

        while (it != path.end() && *it != '/') {
            it++;
        }

        if (it == path.end()) {
            break;
        }

        StaticString<cFilePathLen> parentPath;

        auto err = parentPath.Insert(parentPath.end(), path.begin(), it);
        if (!err.IsNone()) {
            return err;
        }

        err = MakeDir(parentPath);
        if (!err.IsNone()) {
            return err;
        }
    }

    auto err = MakeDir(path);
    if (!err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

Error ClearDir(const String& path)
{
    auto dir = opendir(path.CStr());
    if (dir == nullptr) {
        if (errno == ENOENT) {
            return MakeDirAll(path);
        }

        return errno;
    }

    dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        auto entryName = String(entry->d_name);

        if (entryName == "." || entryName == "..") {
            continue;
        }

        StaticString<cFilePathLen> entryPath = JoinPath(path, entryName);
#if defined(__ZEPHYR__) && defined(CONFIG_POSIX_API)
        // TODO: zephyr doesn't provide possibility to check if dir entry is file or dir. As WA, try to clear or
        // unlink any item.

        auto ret = unlink(entryPath.CStr());
        if (ret != 0) {
            if (errno != ENOTEMPTY && errno != EACCES) {
                return errno;
            }

            auto err = ClearDir(entryPath);
            if (!err.IsNone()) {
                return err;
            }

            ret = unlink(entryPath.CStr());
            if (ret != 0) {
                return errno;
            }
        }
#else
        if (entry->d_type == DT_DIR) {
            auto err = ClearDir(entryPath);
            if (!err.IsNone()) {
                return err;
            }

            auto ret = rmdir(entryPath.CStr());
            if (ret != 0) {
                return errno;
            }
        } else {
            auto ret = unlink(entryPath.CStr());
            if (ret != 0) {
                return errno;
            }
        }
#endif
    }

    closedir(dir);

    return ErrorEnum::eNone;
}

Error Remove(const String& path)
{
#if defined(__ZEPHYR__) && defined(CONFIG_POSIX_API)
    auto ret = unlink(path.CStr());
    if (ret != 0 && errno != ENOENT) {
        return errno;
    }
#else
    struct stat s;

    auto ret = stat(path.CStr(), &s);
    if (ret != 0) {
        if (errno == ENOENT) {
            return ErrorEnum::eNone;
        }

        return errno;
    }

    if (S_ISDIR(s.st_mode)) {
        ret = rmdir(path.CStr());
    } else {
        ret = unlink(path.CStr());
    }
    if (ret != 0) {
        return errno;
    }

#endif

    return ErrorEnum::eNone;
}

Error RemoveAll(const String& path)
{
#if defined(__ZEPHYR__) && defined(CONFIG_POSIX_API)
    auto ret = unlink(path.CStr());
    if (ret != 0) {
        if (errno == ENOENT) {
            return ErrorEnum::eNone;
        }

        if (errno != ENOTEMPTY && errno != EACCES) {
            return errno;
        }

        auto err = ClearDir(path);
        if (!err.IsNone()) {
            return err;
        }

        ret = unlink(path.CStr());
        if (ret != 0) {
            return errno;
        }
    }
#else
    struct stat s;

    auto ret = stat(path.CStr(), &s);
    if (ret != 0) {
        if (errno == ENOENT) {
            return ErrorEnum::eNone;
        }

        return errno;
    }

    if (S_ISDIR(s.st_mode)) {
        auto err = ClearDir(path);
        if (!err.IsNone()) {
            return err;
        }

        ret = rmdir(path.CStr());
    } else {
        ret = unlink(path.CStr());
    }
    if (ret != 0) {
        return errno;
    }

#endif
    return ErrorEnum::eNone;
}

Error Rename(const String& oldPath, const String& newPath)
{
    if (auto ret = rename(oldPath.CStr(), newPath.CStr()); ret != 0) {
        return errno;
    }

    return ErrorEnum::eNone;
}

Error ReadFile(const String& fileName, Array<uint8_t>& buff)
{
    auto fd = open(fileName.CStr(), O_RDONLY);
    if (fd < 0) {
        return Error(errno);
    }

    auto closeFile = DeferRelease(&fd, [](const int* fd) { close(*fd); });

    auto size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        return errno;
    }

    auto pos = lseek(fd, 0, SEEK_SET);
    if (pos < 0) {
        return errno;
    }

    auto err = buff.Resize(size);
    if (!err.IsNone()) {
        return err;
    }

    while (pos < size) {
        auto count = read(fd, buff.Get() + pos, buff.Size() - pos);
        if (count < 0) {
            return errno;
        }

        pos += count;
    }

    if (close(fd) != 0) {
        return errno;
    }

    return ErrorEnum::eNone;
}

Error ReadFileToString(const String& fileName, String& text)
{
    text.Resize(text.MaxSize());

    auto buff = Array<uint8_t>(reinterpret_cast<uint8_t*>(text.Get()), text.Size());

    auto err = ReadFile(fileName, buff);
    if (!err.IsNone()) {
        return err;
    }

    return text.Resize(buff.Size());
}

Error ReadLine(int fd, size_t pos, String& line, const String& delimiter)
{
    if (lseek(fd, pos, SEEK_SET) < 0) {
        return Error(errno);
    }

    line.Resize(line.MaxSize());

    ssize_t bytes = read(fd, line.Get(), line.MaxSize());
    if (bytes < 0) {
        return Error(errno);
    }

    if (auto err = line.Resize(bytes); !err.IsNone()) {
        return err;
    }

    auto [eolPos, err] = line.FindAny(0, delimiter);
    if (!err.IsNone()) {
        return err;
    }

    return line.Resize(eolPos);
}

Error WriteFile(const String& fileName, const Array<uint8_t>& data, uint32_t perm)
{
    // zephyr doesn't support O_TRUNC flag. This is WA to trunc file if it exists.
    auto err = Remove(fileName);
    if (!err.IsNone()) {
        return err;
    }

    auto fd = open(fileName.CStr(), O_CREAT | O_WRONLY, perm);
    if (fd < 0) {
        return Error(errno);
    }

    size_t pos = 0;
    while (pos < data.Size()) {
        auto chunkSize = write(fd, data.Get() + pos, data.Size() - pos);
        if (chunkSize < 0) {
            err = errno;

            close(fd);

            return Error(err);
        }

        pos += chunkSize;
    }

    if (close(fd) != 0) {
        return Error(errno);
    }

// Zephyr doesn't implement chmod
#ifndef __ZEPHYR__
    if (chmod(fileName.CStr(), perm) != 0) {
        return Error(errno);
    }
#endif

    return ErrorEnum::eNone;
}

Error WriteStringToFile(const String& fileName, const String& text, uint32_t perm)
{
    const auto buff = Array<uint8_t>(reinterpret_cast<const uint8_t*>(text.Get()), text.Size());

    return WriteFile(fileName, buff, perm);
}

RetWithError<size_t> CalculateSize(const String& path)
{
    LockGuard lock {sCalculateSizeMutex};

    struct stat st;

    if (auto ret = stat(path.CStr(), &st); ret != 0) {
        return {0, AOS_ERROR_WRAP(Error(errno))};
    }

    if (S_ISREG(st.st_mode)) {
        return {static_cast<size_t>(st.st_size)};
    }

    size_t size         = 0;
    auto   dirIterators = MakeUnique<DirIteratorArray>(&sCalculateSizeAllocator);

    if (auto err = dirIterators->EmplaceBack(path); !err.IsNone()) {
        return {0, AOS_ERROR_WRAP(err)};
    }

    while (!dirIterators->IsEmpty()) {
        bool  stepIntoSubdir = false;
        auto& dirIt          = dirIterators->Back();

        while (dirIt.Next()) {
            const auto fullPath = JoinPath(dirIt.GetRootPath(), dirIt->mPath);

            if (dirIt->mIsDir) {
                if (auto err = dirIterators->EmplaceBack(fullPath); !err.IsNone()) {
                    return {0, AOS_ERROR_WRAP(err)};
                }

                stepIntoSubdir = true;

                break;
            }

            if (auto ret = stat(fullPath.CStr(), &st); ret != 0) {
                return {0, AOS_ERROR_WRAP(Error(ret))};
            }

            size += st.st_size;
        }

        if (stepIntoSubdir) {
            continue;
        }

        dirIterators->Erase(&dirIt);
    }

    return {size};
}

/***********************************************************************************************************************
 * File implementation
 **********************************************************************************************************************/

File::~File()
{
    Close();
}

Error File::Open(const String& path, Mode mode)
{
    if (auto err = Close(); !err.IsNone()) {
        return err;
    }

    int flags = (mode == Mode::Read) ? O_RDONLY : (O_WRONLY | O_CREAT | O_TRUNC);

    mFd = open(path.CStr(), flags, 0644);
    if (mFd < 0) {
        return Error(errno, "file open failed");
    }

    return ErrorEnum::eNone;
}

Error File::Close()
{
    if (mFd >= 0) {
        if (close(mFd) < 0) {
            return Error(errno, "file close failed");
        }

        mFd = -1;
    }

    return ErrorEnum::eNone;
}

Error File::ReadBlock(Array<uint8_t>& buffer)
{
    if (mFd < 0) {
        return ErrorEnum::eWrongState;
    }

    auto blockSize = buffer.MaxSize();
    buffer.Resize(blockSize);

    bool   eof       = false;
    size_t totalRead = 0;

    while (totalRead < blockSize) {
        ssize_t result = read(mFd, buffer.Get() + totalRead, blockSize - totalRead);
        if (result < 0) {
            return Error(errno, "file read failed");
        } else if (result == 0) {
            eof = true;
            break; // EOF
        }

        totalRead += result;
    }

    buffer.Resize(totalRead);

    return eof ? ErrorEnum::eEOF : ErrorEnum::eNone;
}

Error File::WriteBlock(const Array<uint8_t>& buffer)
{
    if (mFd < 0) {
        return ErrorEnum::eWrongState;
    }

    size_t blockSize    = buffer.Size();
    size_t totalWritten = 0;

    while (totalWritten < blockSize) {
        ssize_t result = write(mFd, buffer.Get() + totalWritten, blockSize - totalWritten);
        if (result < 0) {
            return Error(errno, "file write failed");
        }

        totalWritten += result;
    }

    return ErrorEnum::eNone;
}

Error BaseName(const String& path, String& base)
{
    if (auto err = base.Assign(path); !err.IsNone()) {
        return err;
    }

    if (base.Size() == 0) {
        base = ".";

        return ErrorEnum::eNone;
    }

    base.RightTrim("/");

    if (base.Size() == 0) {
        base = "/";
        return ErrorEnum::eNone;
    }

    size_t lastSlashPos = base.Size();

    for (size_t i = base.Size(); i > 0; --i) {
        if (base[i - 1] == '/') {
            lastSlashPos = i - 1;

            break;
        }
    }

    if (lastSlashPos < base.Size()) {
        return base.Remove(base.Get(), base.Get() + lastSlashPos + 1);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::fs
