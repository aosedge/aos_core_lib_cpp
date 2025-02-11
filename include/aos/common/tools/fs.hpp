/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_FS_HPP_
#define AOS_FS_HPP_

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "aos/common/tools/config.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/common/tools/string.hpp"

namespace aos {

/*
 * File path len.
 */
constexpr auto cFilePathLen = AOS_CONFIG_FS_FILE_PATH_LEN;

/**
 * File system interface.
 */
class HostFSItf {
public:
    /**
     * Gets mount point for the given directory.
     *
     * @param dir directory path.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> GetMountPoint(const String& dir) const = 0;

    /**
     * Gets total size of the given directory.
     *
     * @param dir directory path.
     * @return RetWithError<int64_t>.
     */
    virtual RetWithError<int64_t> GetTotalSize(const String& dir) const = 0;

    /**
     * Gets directory size.
     *
     * @param dir directory path.
     * @return RetWithError<int64_t>.
     */
    virtual RetWithError<int64_t> GetDirSize(const String& dir) const = 0;

    /**
     * Gets available size.
     *
     * @param dir directory path.
     * @return RetWithError<int64_t>.
     */
    virtual RetWithError<int64_t> GetAvailableSize(const String& dir) const = 0;
};

/**
 * File system instance.
 */
class FS {
public:
    /**
     * Directory iterator.
     * The iteration order is unspecified, except that each directory entry is visited only once.
     */
    class DirIterator {
    public:
        /**
         * Directory entry.
         */
        struct Entry {
            StaticString<cFilePathLen> mPath;
            bool                       mIsDir = false;
        };

        /**
         * Constructor.
         *
         * @param path directory path.
         */
        explicit DirIterator(const String& path)
            : mDir(opendir(path.CStr()))
            , mRoot(path)
        {
        }

        /**
         * Destructor.
         */
        ~DirIterator()
        {
            if (mDir) {
                closedir(mDir);
            }
        }

        /**
         * Moves to the next entry. The special pathnames dot and dot-dot are skipped.
         *
         * @return bool.
         */
        bool Next()
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

        /**
         * Returns current entry reference.
         *
         * @return const Dir&.
         */
        const Entry& operator*() const { return mEntry; }

        /**
         * Returns current entry pointer.
         *
         * @return const DirEntry*.
         */
        const Entry* operator->() const { return &mEntry; }

    private:
        DIR*                       mDir = nullptr;
        Entry                      mEntry;
        StaticString<cFilePathLen> mRoot;
    };

    /**
     * Appends path to string.
     */
    template <typename... Args>
    static String& AppendPath(String& path, const Args&... args)
    {
        (AppendPathEntry(path, args), ...);

        return path;
    }

    /**
     * Joins path items.
     */
    template <typename... Args>
    static StaticString<cFilePathLen> JoinPath(const Args&... args)
    {
        StaticString<cFilePathLen> path;

        AppendPath(path, args...);

        return path;
    }

    /**
     * Returns directory part of path.
     *
     * @param path path for find directory.
     * @return StaticString<cFilePathLen>.
     */
    static StaticString<cFilePathLen> Dir(const String& path)
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

    /**
     * Checks if directory exists.
     *
     * @param path directory path.
     * @return Error
     */
    static RetWithError<bool> DirExist(const String& path)
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

    /**
     * Creates one directory.
     *
     * @param path directory path.
     * @param parents indicates if parent dirs should be created.
     * @return Error
     */

    static Error MakeDir(const String& path)
    {
        auto ret = mkdir(path.CStr(), S_IRWXU | S_IRWXG | S_IRWXO);
        if (ret != 0 && errno != EEXIST) {
            return errno;
        }

        return ErrorEnum::eNone;
    }

    /**
     * Creates directory including parents.
     *
     * @param path directory path.
     * @param parents indicates if parent dirs should be created.
     * @return Error
     */

    static Error MakeDirAll(const String& path)
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

    /**
     * Clears directory.
     *
     * @param path directory path.
     * @return Error.
     */
    static Error ClearDir(const String& path)
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

    /**
     * Removes file or directory which must be empty.
     *
     * @param path path to file or directory
     * @return Error
     */
    static Error Remove(const String& path)
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

    /**
     * Removes file or directory.
     *
     * @param path path to file or directory
     * @return Error
     */
    static Error RemoveAll(const String& path)
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

    /**
     * Reads content of the file named by fileName into the buffer.
     *
     * @param fileName file name.
     * @param[out] buff destination buffer.
     * @return Error.
     */
    static Error ReadFile(const String& fileName, Array<uint8_t>& buff)
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

    /**
     * Reads content of the file named by fileName into the given string.
     *
     * @param fileName file name.
     * @param[out] text result string.
     * @return Error.
     */
    static Error ReadFileToString(const String& fileName, String& text)
    {
        text.Resize(text.MaxSize());

        auto buff = Array<uint8_t>(reinterpret_cast<uint8_t*>(text.Get()), text.Size());

        auto err = ReadFile(fileName, buff);
        if (!err.IsNone()) {
            return err;
        }

        return text.Resize(buff.Size());
    }

    /**
     * Overwrites file with a specified data.
     *
     * @param fileName file name.
     * @param data input data.
     * @param perm permissions.
     * @return Error.
     */
    static Error WriteFile(const String& fileName, const Array<uint8_t>& data, uint32_t perm)
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

    /**
     * Overwrites file with a specified text.
     *
     * @param fileName file name.
     * @param text input text.
     * @param perm permissions.
     * @return Error.
     */
    static Error WriteStringToFile(const String& fileName, const String& text, uint32_t perm)
    {
        const auto buff = Array<uint8_t>(reinterpret_cast<const uint8_t*>(text.Get()), text.Size());

        return WriteFile(fileName, buff, perm);
    }

private:
    static String& AppendPathEntry(String& path, const String& item)
    {
        if (path.Size() == 0 || *(path.end() - 1) == '/') {
            path.Append(item);
        } else {
            path.Append("/").Append(item);
        }

        return path;
    }
};

} // namespace aos

#endif
