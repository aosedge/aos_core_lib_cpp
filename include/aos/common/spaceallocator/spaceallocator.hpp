/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SPACEALLOCATOR_HPP_
#define AOS_SPACEALLOCATOR_HPP_

#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/map.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/common/tools/time.hpp"
#include "aos/common/types.hpp"

#include "log.hpp"

namespace aos::spaceallocator {

/**
 * Item remover interface.
 */
class ItemRemoverItf {
public:
    /**
     * Removes item.
     *
     * @param id item id.
     * @return Error.
     */
    virtual Error RemoveItem(const String& id) = 0;

    /**
     * Destructor.
     */
    virtual ~ItemRemoverItf() = default;
};

/**
 * Space interface.
 */
class SpaceItf {
public:
    /**
     * Accepts space.
     *
     * @return Error.
     */
    virtual Error Accept() = 0;

    /**
     * Releases space.
     *
     * @return Error.
     */
    virtual Error Release() = 0;

    /**
     * Resizes space.
     *
     * @param size new size.
     * @return Error.
     */
    virtual Error Resize(uint64_t size) = 0;

    /**
     * Returns space size.
     *
     * @return uint64_t.
     */
    virtual uint64_t Size() const = 0;

    /**
     * Destructor.
     */
    virtual ~SpaceItf() = default;
};

struct Partition;

/**
 * Space allocator interface.
 */
class SpaceAllocatorItf {
public:
    /**
     * Allocates space.
     *
     * @param size size to allocate.
     * @return RetWithError<UniquePtr<SpaceItf>>.
     */
    virtual RetWithError<UniquePtr<SpaceItf>> AllocateSpace(uint64_t size) = 0;

    /**
     * Frees space.
     *
     * @param size size to free.
     * @return void.
     */
    virtual void FreeSpace(uint64_t size) = 0;

    /**
     * Adds outdated item.
     *
     * @param id item id.
     * @param size item size.
     * @param timestamp item timestamp.
     * @return Error.
     */
    virtual Error AddOutdatedItem(const String& id, uint64_t size, const Time& timestamp) = 0;

    /**
     * Restores outdated item.
     *
     * @param id item id.
     * @return Error.
     */
    virtual Error RestoreOutdatedItem(const String& id) = 0;

    /**
     * Allocates done.
     *
     * @return Error.
     */
    virtual Error AllocateDone() = 0;

    /**
     * Destructor.
     */
    virtual ~SpaceAllocatorItf() = default;

protected:
    static inline StaticMap<String, SharedPtr<Partition>, cMaxNumPartitions> mPartitions;
    static inline Mutex                                                      mPartitionsMutex;

private:
    friend class Partition;

    virtual void Free(u_int64_t size) = 0;
};

/**
 * Space instance.
 */
class Space : public SpaceItf {
public:
    /**
     * Crates space instance.
     */
    explicit Space(size_t size, SpaceAllocatorItf* allocator)
        : mSize(size)
        , mAllocator(allocator)
    {
    }

    /**
     * Accepts space.
     *
     * @return Error.
     */
    Error Accept() override
    {
        LOG_DBG() << "Space accepted: size=" << mSize;

        return mAllocator->AllocateDone();
    }

    /**
     * Releases space.
     *
     * @return Error.
     */
    Error Release() override
    {
        LOG_DBG() << "Space released: size=" << mSize;

        mAllocator->FreeSpace(mSize);

        return mAllocator->AllocateDone();
    }

    /**
     * Resizes space.
     *
     * @param size new size.
     * @return Error.
     */
    Error Resize(uint64_t size) override
    {
        mSize = size;

        return ErrorEnum::eNone;
    }

    /**
     * Returns space size.
     *
     * @return uint64_t.
     */
    uint64_t Size() const override { return mSize; }

private:
    size_t             mSize;
    SpaceAllocatorItf* mAllocator {};
};

/**
 * Outdated item.
 */
struct OutdatedItem {
    StaticString<Max(cProviderIDLen, cServiceIDLen, cSubjectIDLen, cLayerIDLen, cSystemIDLen, cInstanceIDLen)> mID;
    uint64_t                                                                                                   mSize;
    SpaceAllocatorItf*   mAllocator {};
    SharedPtr<Partition> mPartition {};
    ItemRemoverItf*      mRemover {};
    Time                 mTimestamp;
};

/**
 * Partition.
 */
struct Partition {
    /**
     * Default constructor.
     */
    Partition() = default;

    /**
     * Add partition limit.
     *
     * @param limit partition limit.
     * @return Error.
     */
    Error AddLimit(u_int64_t limit)
    {
        LockGuard lock {mMutex};

        if (mLimit + limit > cMaxLimit) {
            return Error(ErrorEnum::eNoMemory, "partition limit exceeded");
        }

        mLimit += limit;

        return ErrorEnum::eNone;
    }

    /**
     * Remove partition limit.
     *
     * @param limit partition limit.
     * @return Error.
     */
    Error RemoveLimit(u_int64_t limit)
    {
        LockGuard lock {mMutex};

        if (mLimit < limit) {
            return Error(ErrorEnum::eNoMemory, "partition limit exceeded");
        }

        mLimit -= limit;

        return ErrorEnum::eNone;
    }

    /**
     * Allocate space.
     *
     * @param size size to allocate.
     * @return Error.
     */
    Error Allocate(u_int64_t size)
    {
        LockGuard lock {mMutex};

        if (mAllocationCount == 0) {
            auto [availableSize, err] = mHostFS->GetAvailableSize(mMountPoint);
            if (!err.IsNone()) {
                return err;
            }

            mAvailableSize = availableSize;

            LOG_DBG() << "Initial partition space: mountPoint=" << mMountPoint << ", size=" << mAvailableSize;
        }

        if (size > mAvailableSize) {
            auto [freedSize, err] = RemoveOutdatedItems(size - mAvailableSize);
            if (!err.IsNone()) {
                return err;
            }

            mAvailableSize += freedSize;
        }

        mAvailableSize -= size;
        mAllocationCount++;

        LOG_DBG() << "Available partition space: mountPoint=" << mMountPoint << ", size=" << mAvailableSize;

        return ErrorEnum::eNone;
    }

    /**
     * Free space.
     *
     * @param size size to free.
     * @return void.
     */
    void Free(u_int64_t size)
    {
        LockGuard lock {mMutex};

        if (mAllocationCount == 0) {
            return;
        }

        mAvailableSize += size;

        LOG_DBG() << "Available partition space: mountPoint=" << mMountPoint << ", size=" << mAvailableSize;
    }

    /**
     * Allocate done.
     *
     * @return Error.
     */
    Error Done()
    {
        LockGuard lock {mMutex};

        if (mAllocationCount == 0) {
            return Error(ErrorEnum::eNotFound, "no allocation");
        }

        mAllocationCount--;

        return ErrorEnum::eNone;
    }

    /**
     * Add outdated item.
     *
     * @param item outdated item.
     * @return void.
     */
    void AddOutdatedItem(const OutdatedItem& item)
    {
        LockGuard lock {mMutex};

        for (auto& outdatedItem : mOutdatedItems) {
            if (outdatedItem.mID == item.mID) {
                outdatedItem = item;

                return;
            }
        }

        mOutdatedItems.PushBack(item);
    }

    /**
     * Remove outdated item.
     *
     * @param id item id.
     * @return void.
     */
    void RestoreOutdatedItem(const String& id)
    {
        LockGuard lock {mMutex};

        for (size_t i = 0; i < mOutdatedItems.Size(); i++) {
            if (mOutdatedItems[i].mID == id) {
                mOutdatedItems.Erase(mOutdatedItems.begin() + i);

                break;
            }
        }
    }

    static constexpr u_int64_t cMaxLimit = 100;

    StaticString<cFilePathLen>                      mMountPoint;
    u_int64_t                                       mLimit {};
    u_int64_t                                       mTotalSize {};
    u_int32_t                                       mAllocatorCount {};
    HostFSItf*                                      mHostFS {};
    StaticArray<OutdatedItem, cMaxNumOutdatedItems> mOutdatedItems;

private:
    RetWithError<u_int64_t> RemoveOutdatedItems(uint64_t size)
    {
        u_int64_t totalSize = 0;

        for (const auto& item : mOutdatedItems) {
            totalSize += item.mSize;
        }

        if (size > totalSize) {
            return {0, Error(ErrorEnum::eNoMemory, "partition limit exceeded")};
        }

        LOG_DBG() << "Remove outdated items: mountPoint=" << mMountPoint << ", requiredSize=" << size;

        mOutdatedItems.Sort([](const OutdatedItem& a, const OutdatedItem& b) { return a.mTimestamp < b.mTimestamp; });

        u_int64_t freedSize = 0;
        size_t    i         = 0;

        for (; freedSize < size; i++) {
            const auto& item = mOutdatedItems[i];

            LOG_DBG() << "Remove outdated item: mountPoint=" << mMountPoint << ", id=" << item.mID
                      << ", size=" << item.mSize;

            if (auto err = item.mRemover->RemoveItem(item.mID); !err.IsNone()) {
                return {freedSize, err};
            }

            item.mAllocator->Free(item.mSize);

            freedSize += item.mSize;
        }

        mOutdatedItems.Erase(mOutdatedItems.begin(), mOutdatedItems.begin() + i);

        return {freedSize, ErrorEnum::eNone};
    }

    u_int32_t mAllocationCount {};
    u_int64_t mAvailableSize {};
    Mutex     mMutex {};
};

/**
 * Space allocator instance.
 */
template <size_t cNumAllocations>
class SpaceAllocator : public SpaceAllocatorItf {
public:
    /**
     * Initializes space allocator.
     *
     * @param path path to allocate space.
     * @param limit limit in percents.
     * @param remover item remover.
     * @param hostFS host file system.
     * @return Error.
     */
    Error Init(const String& path, u_int limit, ItemRemoverItf& remover, HostFSItf& hostFS)
    {
        LockGuard lock {mPartitionsMutex};

        LOG_DBG() << "Init space allocator path=" << path << ", limit=" << limit;

        mRemover = &remover;
        mHostFS  = &hostFS;
        mPath    = path;

        auto [mountPoint, errMountPoint] = mHostFS->GetMountPoint(path);
        if (!errMountPoint.IsNone()) {
            return errMountPoint;
        }

        LOG_DBG() << "Mount point: path=" << path << ", mountPoint=" << mountPoint;

        auto [partition, err] = mPartitions.At(mountPoint);
        if (!err.IsNone()) {
            if (Tie(partition, err) = NewPartition(mountPoint); !err.IsNone()) {
                return err;
            }

            if (err = mPartitions.TryEmplace(mountPoint, partition); !err.IsNone()) {
                return err;
            }
        }

        partition->mAllocatorCount++;

        if (limit != 0) {
            if (err = partition->AddLimit(limit); !err.IsNone()) {
                return err;
            }

            mSizeLimit = partition->mTotalSize * partition->mLimit / 100;
        }

        mPartition = partition;

        return ErrorEnum::eNone;
    }

    /**
     * Closes space allocator.
     *
     * @return Error.
     */
    Error Close()
    {
        LockGuard lock {mPartitionsMutex};

        LOG_DBG() << "Close space allocator";

        Error err;

        if (auto errRemovePartLimit = mPartition->RemoveLimit(mPartition->mLimit); !errRemovePartLimit.IsNone()) {
            err = errRemovePartLimit;
        }

        mPartition->mAllocatorCount--;

        if (mPartition->mAllocatorCount != 0) {
            return err;
        }

        if (auto errRemovePart = mPartitions.Remove(mPartition->mMountPoint);
            !err.IsNone() && !errRemovePart.IsNone()) {
            err = errRemovePart;
        }

        return err;
    }

    /**
     * Allocates space.
     *
     * @param size size to allocate.
     * @return RetWithError<UniquePtr<SpaceItf>>.
     */
    RetWithError<UniquePtr<SpaceItf>> AllocateSpace(uint64_t size) override
    {
        LOG_DBG() << "Allocate space: path=" << mPath << ", size=" << size;

        auto err = Allocate(size);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        if (err = mPartition->Allocate(size); !err.IsNone()) {
            return {nullptr, err};
        }

        return UniquePtr<SpaceItf>(new (&mAllocator) Space(size, this));
    };

    /**
     * Frees space.
     *
     * @param size size to free.
     * @return void.
     */
    void FreeSpace(uint64_t size) override
    {
        LOG_DBG() << "Free space: path=" << mPath << ", size=" << size;

        Free(size);

        mPartition->Free(size);
    }

    /**
     * Allocates done.
     *
     * @return Error.
     */
    Error AllocateDone() override
    {
        LOG_DBG() << "Allocate done: path=" << mPath;

        if (auto err = Done(); !err.IsNone()) {
            return err;
        }

        return mPartition->Done();
    }

    /**
     * Adds outdated item.
     *
     * @param id item id.
     * @param size item size.
     * @param timestamp item timestamp.
     * @return Error.
     */
    Error AddOutdatedItem(const String& id, uint64_t size, const Time& timestamp) override
    {
        if (mRemover == nullptr) {
            return Error(ErrorEnum::eNotFound, "no item remover");
        }

        LOG_DBG() << "Add outdated item: path=" << mPath << ", id=" << id << ", size=" << size
                  << ", timestamp=" << timestamp;

        mPartition->AddOutdatedItem(OutdatedItem {id, size, this, mPartition, mRemover, timestamp});

        return ErrorEnum::eNone;
    }

    /**
     * Restores outdated item.
     *
     * @param id item id.
     * @return Error.
     */
    Error RestoreOutdatedItem(const String& id) override
    {
        LOG_DBG() << "Restore outdated item: path=" << mPath << ", id=" << id;

        mPartition->RestoreOutdatedItem(id);

        return ErrorEnum::eNone;
    }

private:
    RetWithError<SharedPtr<Partition>> NewPartition(const String& path)
    {
        auto [totalSize, err] = mHostFS->GetTotalSize(path);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        SharedPtr<Partition> partition = MakeShared<Partition>(&mPartitionAllocator);

        partition->mMountPoint = path;
        partition->mTotalSize  = totalSize;
        partition->mHostFS     = mHostFS;

        return {partition, ErrorEnum::eNone};
    }

    Error Allocate(u_int64_t size)
    {
        LockGuard lock {mMutex};

        if (mSizeLimit == 0) {
            return ErrorEnum::eNone;
        }

        if (mAllocationCount == 0) {
            auto [allocatedSize, err] = mHostFS->GetDirSize(mPath);
            if (!err.IsNone()) {
                return err;
            }

            mAllocatedSize = allocatedSize;

            LOG_DBG() << "Initial allocated space: path= " << mPath << ", size= " << mAllocatedSize;
        }

        if (mSizeLimit > 0 && mAllocatedSize + size > mSizeLimit) {
            auto [freedSize, err] = RemoveOutdatedItems(mAllocatedSize + size - mSizeLimit);
            if (!err.IsNone()) {
                return err;
            }

            if (freedSize > mAllocatedSize) {
                mAllocatedSize = 0;
            } else {
                mAllocatedSize -= freedSize;
            }
        }

        mAllocatedSize += size;
        mAllocationCount++;

        LOG_DBG() << "Total allocated space: path= " << mPath << ", size= " << mAllocatedSize;

        return ErrorEnum::eNone;
    }

    RetWithError<u_int64_t> RemoveOutdatedItems(uint64_t size)
    {
        u_int64_t totalSize = 0;

        for (const auto& item : mPartition->mOutdatedItems) {
            if (item.mAllocator != this) {
                continue;
            }

            totalSize += item.mSize;
        }

        if (size > totalSize) {
            return {0, Error(ErrorEnum::eNoMemory, "partition limit exceeded")};
        }

        LOG_DBG() << "Remove outdated items: mountPoint=" << mPartition->mMountPoint << ", requiredSize=" << size;

        mPartition->mOutdatedItems.Sort(
            [](const OutdatedItem& a, const OutdatedItem& b) { return a.mTimestamp < b.mTimestamp; });

        u_int64_t freedSize = 0;
        size_t    i         = 0;

        for (auto& item : mPartition->mOutdatedItems) {
            if (item.mAllocator != this || freedSize >= size) {
                mPartition->mOutdatedItems[i] = item;
                i++;

                continue;
            }

            LOG_DBG() << "Remove outdated item: mountPoint=" << mPartition->mMountPoint << ", id=" << item.mID
                      << ", size=" << item.mSize;

            if (auto err = item.mRemover->RemoveItem(item.mID); !err.IsNone()) {
                return {freedSize, err};
            }

            item.mPartition->Free(item.mSize);

            freedSize += item.mSize;
        }

        mPartition->mOutdatedItems.Erase(mPartition->mOutdatedItems.begin() + i, mPartition->mOutdatedItems.end());

        return {freedSize, ErrorEnum::eNone};
    }

    void Free(u_int64_t size) override
    {
        LockGuard lock {mMutex};

        if (mSizeLimit == 0) {
            return;
        }

        if (mAllocationCount > 0) {
            if (size < mAllocatedSize) {
                mAllocatedSize -= size;
            } else {
                mAllocatedSize = 0;
            }

            LOG_DBG() << "Total allocated space: path= " << mPath << ", size= " << mAllocatedSize;
        }
    }

    Error Done()
    {
        LockGuard lock {mMutex};

        if (mSizeLimit == 0) {
            return ErrorEnum::eNone;
        }

        if (mAllocationCount == 0) {
            return Error(ErrorEnum::eNotFound, "no allocation");
        }

        mAllocationCount--;

        return ErrorEnum::eNone;
    }

    friend class Partition;

    StaticAllocator<sizeof(Space) * cNumAllocations>       mAllocator;
    StaticAllocator<sizeof(Partition) * cMaxNumPartitions> mPartitionAllocator;
    u_int64_t                                              mSizeLimit {};
    u_int64_t                                              mAllocationCount {};
    u_int64_t                                              mAllocatedSize {};
    StaticString<cFilePathLen>                             mPath;
    ItemRemoverItf*                                        mRemover;
    HostFSItf*                                             mHostFS;
    SharedPtr<Partition>                                   mPartition;
    Mutex                                                  mMutex;
};

} // namespace aos::spaceallocator

#endif
