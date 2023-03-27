/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_FS_HPP_
#define AOS_FS_HPP_

#include <dirent.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#include "aos/common/tools/config.hpp"
#include "aos/common/tools/string.hpp"

namespace aos {

/*
 * File path len.
 */
constexpr auto cFilePathLen = AOS_CONFIG_FS_FILE_PATH_LEN;

/**
 * File system instance.
 */
class FS {
public:
    /**
     * Appends path to string.
     */
    template <typename T>
    static String& AppendPath(String& path, T& item)
    {
        if (item[0] == '/') {
            path.Append(item);
        } else {
            path.Append("/").Append(item);
        }

        return path;
    }

    /**
     * Appends path to string.
     */
    template <typename T, typename... Args>
    static String& AppendPath(String& path, T& item, Args... args)
    {
        AppendPath(path, item);
        AppendPath(path, args...);

        return path;
    }

    /**
     * Joins path items.
     */
    template <typename T, typename... Args>
    static StaticString<cFilePathLen> JoinPath(T& first, Args... args)
    {
        StaticString<cFilePathLen> path = first;

        AppendPath(path, args...);

        return path;
    }

    /**
     * Joins path items.
     */
    template <typename T, typename... Args>
    static StaticString<cFilePathLen> JoinPath(T* first, Args... args)
    {
        StaticString<cFilePathLen> path = first;

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
        StaticString<cFilePathLen> dir = path;

        auto it = dir.end();

        while (it != dir.begin()) {
            it--;

            if (*it == '/') {
                *it = 0;

                break;
            }
        }

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
        if (ret != 0) {
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

            auto ret = mkdir(parentPath.CStr(), S_IRWXU | S_IRWXG | S_IRWXO);
            if (ret != 0) {
                if (errno == EEXIST) {
                    continue;
                }

                return errno;
            }
        }

        auto ret = mkdir(path.CStr(), S_IRWXU | S_IRWXG | S_IRWXO);
        if (ret != 0 && errno != EEXIST) {
            return errno;
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
#ifdef __ZEPHYR__
            // TODO: zephyr doesn't provide possibility to check if dir entry is file or dir. As WA, try to clear or
            // unlink any item.

            ClearDir(entryPath);

            auto ret = unlink(entryPath.CStr());
            if (ret != 0) {
                return errno;
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
            } else if (entry->d_type == DT_REG) {
            }
#endif
        }

        closedir(dir);

        return ErrorEnum::eNone;
    }

    static Error RemoveDir(const String& path, bool recursive = false)
    {
        if (recursive) {
            auto dirExist = DirExist(path);
            if (!dirExist.mError.IsNone()) {
                return dirExist.mError;
            }

            if (!dirExist.mValue) {
                return ErrorEnum::eNone;
            }

            auto err = ClearDir(path);
            if (!err.IsNone()) {
                if (err.Errno() == ENOENT) {
                    return ErrorEnum::eNone;
                }

                return err;
            }
        }

#ifdef __ZEPHYR__
        auto ret = unlink(path.CStr());
        if (ret != 0) {
            return errno;
        }
#else
        auto ret = rmdir(path.CStr());
        if (ret != 0 && errno != ENOENT) {
            return errno;
        }
#endif
        return ErrorEnum::eNone;
    }
};

} // namespace aos

#endif
