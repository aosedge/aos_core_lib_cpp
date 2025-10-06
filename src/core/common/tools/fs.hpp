/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TOOLS_FS_HPP_
#define AOS_CORE_COMMON_TOOLS_FS_HPP_

#include <dirent.h>

#include <core/common/crypto/crypto.hpp>

#include "config.hpp"
#include "noncopyable.hpp"
#include "string.hpp"

namespace aos {

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

    /**
     * Sets user quota for the given path.
     *
     * @param path path to set quota for.
     * @param quota quota size in bytes.
     * @param uid user ID.
     * @return Error.
     */
    virtual Error SetUserQuota(const String& path, size_t quota, size_t uid) const = 0;

    /**
     * Changes the owner of a file or directory.
     *
     * @param path path to the file or directory.
     * @param uid new user ID.
     * @param gid new group ID.
     * @return Error.
     */
    virtual Error ChangeOwner(const String& path, uint32_t uid, uint32_t gid) const = 0;
};

/**
 * FS event type.
 */
class FSEventType {
public:
    enum class Enum {
        eAccess,
        eModify,
        eClose,
        eCreate,
        eDelete,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "access",
            "modify",
            "close",
            "create",
            "delete",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using FSEventEnum = FSEventType::Enum;
using FSEvent     = EnumStringer<FSEventType>;

/**
 * Interface to receive file system events.
 */
class FSEventSubscriberItf {
public:
    /**
     * Destructor.
     */
    virtual ~FSEventSubscriberItf() = default;

    /**
     * Called when file system event occurs for a specified path.
     *
     * @param path path to the file or directory that triggered the event.
     * @param events array of events that occurred.
     */
    virtual void OnFSEvent(const String& path, const Array<FSEvent>& events) = 0;
};

/**
 * Interface to watch file system events.
 */
class FSWatcherItf {
public:
    /**
     * Destructor.
     */
    virtual ~FSWatcherItf() = default;

    /**
     * Subscribes subscriber on fs events for the specified path.
     *
     * @param path path to watch.
     * @param subscriber subscriber object.
     * @return Error.
     */
    virtual Error Subscribe(const String& path, FSEventSubscriberItf& subscriber) = 0;

    /**
     * Unsubscribes subscriber.
     *
     * @param path path to unsubscribe from.
     * @param subscriber subscriber to unsubscribe.
     * @return Error.
     */
    virtual Error Unsubscribe(const String& path, FSEventSubscriberItf& subscriber) = 0;
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

/**
 * File info.
 */
struct FileInfo {
    /**
     * Constructor.
     */
    FileInfo() = default;

    StaticArray<uint8_t, crypto::cSHA256Size> mSHA256;
    size_t                                    mSize {};
};

/**
 * File info provider.
 */
class FileInfoProviderItf {
public:
    /**
     * Destructor.
     */
    virtual ~FileInfoProviderItf() = default;

    /**
     * Creates file info.
     *
     * @param path file path.
     * @param[out] info file info.
     * @return Error.
     */
    virtual Error CreateFileInfo(const String& path, FileInfo& info) = 0;
};

/**
 * File info provider implementation.
 */
class FileInfoProvider : public FileInfoProviderItf {
public:
    /**
     * Initializes file info provider.
     *
     * @param hashProvider hash provider.
     * @return Error.
     */
    Error Init(crypto::HasherItf& hashProvider);

    /**
     * Creates file info.
     *
     * @param path file path.
     * @param[out] info file info.
     * @return Error.
     */
    Error CreateFileInfo(const String& path, FileInfo& info) override;

private:
    static constexpr auto cReadFileBufferSize = AOS_CONFIG_TYPES_READ_FILE_BUFFER_SIZE;

    crypto::HasherItf*                        mHashProvider {};
    StaticArray<uint8_t, cReadFileBufferSize> mReadFileBuffer;
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

/**
 * File class.
 */
class File {
public:
    /**
     * File open mode.
     */
    enum class Mode { Read, Write };

    /**
     * Destructor.
     */
    ~File();

    /**
     * Opens a file in the specified mode.
     *
     * @param path path to the file.
     * @param mode read or write mode.
     * @return Error.
     */
    Error Open(const String& path, Mode mode);

    /**
     * Closes the file if open.
     *
     * @return Error.
     */
    Error Close();

    /**
     * Reads a block from the file.
     *
     * @param[out] buffer destination buffer.
     * @return Error.
     */
    Error ReadBlock(Array<uint8_t>& buffer);

    /**
     * Writes a block to the file.
     *
     * @param[out] buffer source buffer.
     * @return Error.
     */
    Error WriteBlock(const Array<uint8_t>& buffer);

private:
    int mFd = -1;
};

/**
 * Returns base name of the path.
 *
 * @param path path.
 * @param base base name.
 * @return Error.
 */
Error BaseName(const String& path, String& base);

} // namespace fs
} // namespace aos

#endif
