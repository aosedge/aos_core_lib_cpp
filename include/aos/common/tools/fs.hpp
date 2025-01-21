/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_FS_HPP_
#define AOS_FS_HPP_

#include <dirent.h>

#include "aos/common/tools/config.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/tools/string.hpp"

namespace aos {

/*
 * File path len.
 */
constexpr auto cFilePathLen = AOS_CONFIG_FS_FILE_PATH_LEN;

/**
 * Directory iterator max count.
 */
constexpr auto cDirIteratorMaxSize = AOS_CONFIG_FS_DIR_ITERATOR_MAX_COUNT;

namespace fs {
/**
 * File system platform interface.
 */
class FSPlatformItf {
public:
    /**
     * Destructor.
     */
    virtual ~FSPlatformItf() = default;

    /**
     * Gets mount point for the given directory.
     *
     * @param dir directory path.
     * @return RetWithError<StaticString<cFilePathLen>>.
     */
    virtual RetWithError<StaticString<cFilePathLen>> GetMountPoint(const String& dir) const = 0;

    /**
     * Gets total size of the file system.
     *
     * @param dir directory path.
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> GetTotalSize(const String& dir) const = 0;

    /**
     * Gets directory size.
     *
     * @param dir directory path.
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> GetDirSize(const String& dir) const = 0;

    /**
     * Gets available size.
     *
     * @param dir directory path.
     * @return RetWithError<size_t>.
     */
    virtual RetWithError<size_t> GetAvailableSize(const String& dir) const = 0;
};

/**
 * Directory iterator.
 * The iteration order is unspecified, except that each directory entry is visited only once.
 */
class DirIterator : public NonCopyable {
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
    explicit DirIterator(const String& path);

    /**
     * Move constructor.
     *
     * @param other iterator to move from.
     */
    DirIterator(DirIterator&& other);

    /**
     * Move assignment.
     *
     * @param other iterator to move from.
     * @return DirIterator&.
     */
    DirIterator& operator=(DirIterator&& other);

    /**
     * Destructor.
     */
    ~DirIterator();

    /**
     * Moves to the next entry. The special pathnames dot and dot-dot are skipped.
     *
     * @return bool.
     */
    bool Next();

    /**
     * Returns root path.
     *
     * @return String.
     */
    String GetRootPath() const { return mRoot; }

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

using DirIteratorStaticArray = StaticArray<DirIterator, cDirIteratorMaxSize>;

/**
 * Appends path to string.
 */
template <typename... Args>
String& AppendPath(String& path, const Args&... args)
{
    auto AppendPathEntry = [](String& path, const String& item) -> String& {
        if (path.Size() == 0 || *(path.end() - 1) == '/') {
            path.Append(item);
        } else {
            path.Append("/").Append(item);
        }

        return path;
    };

    (AppendPathEntry(path, args), ...);

    return path;
}

/**
 * Joins path items.
 */
template <typename... Args>
StaticString<cFilePathLen> JoinPath(const Args&... args)
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
StaticString<cFilePathLen> Dir(const String& path);

/**
 * Checks if directory exists.
 *
 * @param path directory path.
 * @return Error
 */
RetWithError<bool> DirExist(const String& path);

/**
 * Creates one directory.
 *
 * @param path directory path.
 * @param parents indicates if parent dirs should be created.
 * @return Error
 */

Error MakeDir(const String& path);

/**
 * Creates directory including parents.
 *
 * @param path directory path.
 * @param parents indicates if parent dirs should be created.
 * @return Error
 */

Error MakeDirAll(const String& path);

/**
 * Clears directory.
 *
 * @param path directory path.
 * @return Error.
 */
Error ClearDir(const String& path);

/**
 * Removes file or directory which must be empty.
 *
 * @param path path to file or directory
 * @return Error
 */
Error Remove(const String& path);

/**
 * Removes file or directory.
 *
 * @param path path to file or directory
 * @return Error
 */
Error RemoveAll(const String& path);

/**
 * Renames file or directory.
 *
 * @param oldPath old path.
 * @param newPath new path.
 * @return Error.
 */
Error Rename(const String& oldPath, const String& newPath);

/**
 * Reads content of the file named by fileName into the buffer.
 *
 * @param fileName file name.
 * @param[out] buff destination buffer.
 * @return Error.
 */
Error ReadFile(const String& fileName, Array<uint8_t>& buff);

/**
 * Reads content of the file named by fileName into the given string.
 *
 * @param fileName file name.
 * @param[out] text result string.
 * @return Error.
 */
Error ReadFileToString(const String& fileName, String& text);

/**
 * Reads line from file.
 *
 * @param fd file descriptor.
 * @param pos position in the file.
 * @param line line to read.
 * @param delimiter line delimiter.
 * @return Error.
 */
Error ReadLine(int fd, size_t pos, String& line, const String& delimiter = "\n\0");

/**
 * Overwrites file with a specified data.
 *
 * @param fileName file name.
 * @param data input data.
 * @param perm permissions.
 * @return Error.
 */
Error WriteFile(const String& fileName, const Array<uint8_t>& data, uint32_t perm);

/**
 * Overwrites file with a specified text.
 *
 * @param fileName file name.
 * @param text input text.
 * @param perm permissions.
 * @return Error.
 */
Error WriteStringToFile(const String& fileName, const String& text, uint32_t perm);

/**
 * Calculates size of the file or directory.
 *
 * @param path file or directory path.
 * @return RetWithError<size_t>.
 */
RetWithError<size_t> CalculateSize(const String& path);

} // namespace fs
} // namespace aos

#endif
