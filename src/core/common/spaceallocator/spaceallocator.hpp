/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_SPACEALLOCATOR_SPACEALLOCATOR_HPP_
#define AOS_CORE_COMMON_SPACEALLOCATOR_SPACEALLOCATOR_HPP_

#include <core/common/ocispec/itf/imagespec.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/function.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>

#include "itf/spaceallocator.hpp"

namespace aos::spaceallocator {

struct Partition;

/**
 * Outdated item.
 */
struct OutdatedItem {
    StaticString<cIDLen>                    mID;
    StaticString<cVersionLen>               mVersion;
    SpaceAllocatorItf*                      mSpaceAllocator {};
    StaticFunction<cDefaultFunctionMaxSize> mFreeCallback;
    Partition*                              mPartition {};
    ItemRemoverItf*                         mRemover {};
    Time                                    mTimestamp;
};

/**
 * Partition.
 *
 * Partition methods must be called with the partition mutex locked, except AddLimit and RemoveLimit which are
 * protected by the partitions mutex. The partition mutex also protects the state of the space allocators sharing the
 * partition.
 */
class Partition {
public:
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
    Error AddLimit(size_t limit)
    {
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
    Error RemoveLimit(size_t limit)
    {
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
    Error Allocate(size_t size)
    {
        if (mAllocationCount == 0) {
            auto [availableSize, err] = mPlatformFS->GetAvailableSize(mMountPoint);
            if (!err.IsNone()) {
                return err;
            }

            mAvailableSize = availableSize;
        }

        if (size > mAvailableSize) {
            if (auto err = RemoveOutdatedItems(size - mAvailableSize).mError; !err.IsNone()) {
                return err;
            }

            if (size > mAvailableSize) {
                return Error(ErrorEnum::eNoMemory, "not enough space");
            }
        }

        mAvailableSize -= size;
        mAllocationCount++;

        return ErrorEnum::eNone;
    }

    /**
     * Free space.
     *
     * @param size size to free.
     * @return void.
     */
    void Free(size_t size) { mAvailableSize += size; }

    /**
     * Allocate done.
     *
     * @return Error.
     */
    Error Done()
    {
        if (mAllocationCount == 0) {
            return Error(ErrorEnum::eNotFound, "no allocation");
        }

        mAllocationCount--;

        return ErrorEnum::eNone;
    }

    /**
     * Resizes allocated space without changing allocation count.
     * Used by resize operations so that mAllocationCount tracks live Space objects only.
     *
     * @param deltaSize size delta: positive means more space is needed, negative means space is released.
     * @return Error.
     */
    Error Resize(int64_t deltaSize)
    {
        if (deltaSize <= 0) {
            mAvailableSize += static_cast<size_t>(-deltaSize);

            return ErrorEnum::eNone;
        }

        const auto extra = static_cast<size_t>(deltaSize);

        if (extra > mAvailableSize) {
            if (auto err = RemoveOutdatedItems(extra - mAvailableSize).mError; !err.IsNone()) {
                return err;
            }

            if (extra > mAvailableSize) {
                return Error(ErrorEnum::eNoMemory, "not enough space");
            }
        }

        mAvailableSize -= extra;

        return ErrorEnum::eNone;
    }

    /**
     * Add outdated item.
     *
     * @param item outdated item.
     * @return Error.
     */
    Error AddOutdatedItem(const OutdatedItem& item)
    {
        for (auto& outdatedItem : mOutdatedItems) {
            if (outdatedItem.mID == item.mID && outdatedItem.mVersion == item.mVersion) {
                outdatedItem = item;

                return ErrorEnum::eNone;
            }
        }

        auto err = mOutdatedItems.PushBack(item);
        if (!err.IsNone()) {
            SortOutdatedItems();

            auto& oldestItem = mOutdatedItems[0];

            if (auto removeErr = RemoveItem(oldestItem).mError; !removeErr.IsNone()) {
                return removeErr;
            }

            oldestItem = item;

            return ErrorEnum::eNone;
        }

        return err;
    }

    /**
     * Remove outdated item.
     *
     * @param id item id.
     * @param version item version.
     * @return void.
     */
    void RestoreOutdatedItem(const String& id, const String& version)
    {
        for (size_t i = 0; i < mOutdatedItems.Size(); i++) {
            if (mOutdatedItems[i].mID == id && mOutdatedItems[i].mVersion == version) {
                mOutdatedItems.Erase(mOutdatedItems.begin() + i);

                break;
            }
        }
    }

    /**
     * Removes the oldest outdated item of the space allocator.
     *
     * The item is removed from the outdated items list whatever the result of the item remover. On success, the freed
     * size is accounted in the partition and in the space allocator the item belongs to.
     *
     * @param spaceAllocator space allocator the item belongs to.
     * @return RetWithError<size_t>: freed size, eNotFound if there is no outdated item to remove.
     */
    RetWithError<size_t> RemoveOutdatedItem(const SpaceAllocatorItf* spaceAllocator)
    {
        SortOutdatedItems();

        auto oldestItem = mOutdatedItems.FindIf(
            [spaceAllocator](const OutdatedItem& item) { return item.mSpaceAllocator == spaceAllocator; });
        if (oldestItem == mOutdatedItems.end()) {
            return {0, Error(ErrorEnum::eNotFound, "no outdated item")};
        }

        auto [size, err] = RemoveItem(*oldestItem);

        mOutdatedItems.Erase(oldestItem);

        if (!err.IsNone()) {
            return {0, err};
        }

        return {size, ErrorEnum::eNone};
    }

    /**
     * Removes outdated items starting from the oldest one until the requested size is freed.
     *
     * The freed size of each removed item is accounted in the partition and in the space allocator the item belongs
     * to. On item remover error, the already removed items stay removed and the failed item stays in the list.
     *
     * @param size size to free.
     * @param spaceAllocator if set, only items of this space allocator are removed.
     * @return RetWithError<size_t>: freed size.
     */
    RetWithError<size_t> RemoveOutdatedItems(size_t size, const SpaceAllocatorItf* spaceAllocator = nullptr)
    {
        SortOutdatedItems();

        size_t freedSize = 0;

        for (size_t i = 0; freedSize < size && i < mOutdatedItems.Size();) {
            auto& item = mOutdatedItems[i];

            if (spaceAllocator != nullptr && item.mSpaceAllocator != spaceAllocator) {
                i++;

                continue;
            }

            auto [itemSize, err] = RemoveItem(item);
            if (!err.IsNone()) {
                return {freedSize, err};
            }

            freedSize += itemSize;

            mOutdatedItems.Erase(mOutdatedItems.begin() + i);
        }

        return {freedSize, ErrorEnum::eNone};
    }

    /**
     * Removes the outdated items of the space allocator from the outdated items list without removing the items.
     *
     * @param spaceAllocator space allocator the items belong to.
     * @return void.
     */
    void ClearOutdatedItems(const SpaceAllocatorItf* spaceAllocator)
    {
        mOutdatedItems.RemoveIf(
            [spaceAllocator](const OutdatedItem& item) { return item.mSpaceAllocator == spaceAllocator; });
    }

    static constexpr size_t cMaxNumOutdatedItems = AOS_CONFIG_SPACEALLOCATOR_MAX_OUTDATED_ITEMS;

    StaticString<cFilePathLen>                      mMountPoint;
    size_t                                          mLimit {};
    size_t                                          mAllocatorCount {};
    fs::FSPlatformItf*                              mPlatformFS {};
    StaticArray<OutdatedItem, cMaxNumOutdatedItems> mOutdatedItems;
    Mutex                                           mMutex {};

private:
    static constexpr size_t cMaxLimit = 100;

    void SortOutdatedItems()
    {
        mOutdatedItems.Sort([](const OutdatedItem& a, const OutdatedItem& b) { return a.mTimestamp < b.mTimestamp; });
    }

    RetWithError<size_t> RemoveItem(OutdatedItem& item)
    {
        auto [size, err] = item.mRemover->RemoveItem(item.mID, item.mVersion);
        if (!err.IsNone()) {
            return {0, err};
        }

        item.mFreeCallback(reinterpret_cast<void*>(size));
        mAvailableSize += size;

        return {size, ErrorEnum::eNone};
    }

    size_t mAllocationCount {};
    size_t mAvailableSize {};
};

/**
 * Space allocator storage.
 */
struct SpaceAllocatorStorage {
    static inline StaticMap<String, Partition, cMaxNumPartitions> mPartitions;
    static inline Mutex                                           mPartitionsMutex;
};

/**
 * Space allocator instance.
 */
template <size_t cNumAllocations>
class SpaceAllocator : public SpaceAllocatorItf, public SpaceAllocatorStorage {
private:
    /**
     * Space instance.
     */
    class Space : public SpaceItf {
    public:
        Space(size_t size, SpaceAllocator* spaceAllocator)
            : mSize(size)
            , mSpaceAllocator(spaceAllocator)
        {
        }

        Error Accept() override { return mSpaceAllocator->AllocateDone(); }

        Error Release() override
        {
            mSpaceAllocator->FreeSpace(mSize);

            return mSpaceAllocator->AllocateDone();
        }

        Error Resize(size_t size) override
        {
            if (auto err = mSpaceAllocator->ResizeSpace(mSize, size); !err.IsNone()) {
                return err;
            }

            mSize = size;

            return ErrorEnum::eNone;
        }

        size_t Size() const override { return mSize; }

    private:
        size_t          mSize;
        SpaceAllocator* mSpaceAllocator;
    };

public:
    /**
     * Constructor.
     */
    SpaceAllocator() = default;

    /**
     * Initializes space allocator.
     *
     * @param allocator allocator to use for space objects.
     * @param path path to allocate space.
     * @param platformFS platform file system.
     * @param limit limit in percents.
     * @param remover item remover.
     * @return Error.
     */
    Error Init(AllocatorItf& allocator, const String& path, fs::FSPlatformItf& platformFS, size_t limit = 0,
        ItemRemoverItf* remover = nullptr)
    {
        LockGuard lock {mPartitionsMutex};

        mAllocator  = &allocator;
        mRemover    = remover;
        mPlatformFS = &platformFS;
        mPath       = path;

        if (auto err = fs::MakeDirAll(path); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto [mountPoint, errMountPoint] = mPlatformFS->GetMountPoint(path);
        if (!errMountPoint.IsNone()) {
            return AOS_ERROR_WRAP(errMountPoint);
        }

        auto partitionIt = mPartitions.Find(mountPoint);
        if (partitionIt == mPartitions.end()) {
            if (auto err = mPartitions.TryEmplace(mountPoint); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            partitionIt = mPartitions.end() - 1;

            NewPartition(mountPoint, partitionIt->mSecond);
        }

        mPartition = &partitionIt->mSecond;
        mPartition->mAllocatorCount++;

        if (limit != 0) {
            if (auto err = mPartition->AddLimit(limit); !err.IsNone()) {
                return err;
            }

            mLimitPercent = limit;
        }

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

        Error err;

        if (auto errRemovePartLimit = mPartition->RemoveLimit(mLimitPercent); !errRemovePartLimit.IsNone()) {
            err = errRemovePartLimit;
        }

        {
            LockGuard partitionLock {mPartition->mMutex};

            mPartition->ClearOutdatedItems(this);
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
    RetWithError<UniquePtr<SpaceItf>> AllocateSpace(size_t size) override
    {
        LockGuard lock {mPartition->mMutex};

        auto err = Allocate(size);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        if (err = mPartition->Allocate(size); !err.IsNone()) {
            Free(size);
            Done();

            return {nullptr, err};
        }

        auto space = MakeUnique<Space>(mAllocator, size, this);
        if (!space) {
            mPartition->Free(size);
            mPartition->Done();
            Free(size);
            Done();

            return {nullptr, ErrorEnum::eNoMemory};
        }

        return UniquePtr<SpaceItf>(Move(space));
    };

    /**
     * Frees space.
     *
     * @param size size to free.
     * @return void.
     */
    void FreeSpace(size_t size) override
    {
        LockGuard lock {mPartition->mMutex};

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
        LockGuard lock {mPartition->mMutex};

        if (auto err = Done(); !err.IsNone()) {
            return err;
        }

        return mPartition->Done();
    }

    /**
     * Adds outdated item.
     *
     * @param id item id.
     * @param version item version.
     * @param timestamp item timestamp.
     * @return Error.
     */
    Error AddOutdatedItem(const String& id, const String& version, const Time& timestamp) override
    {
        if (mRemover == nullptr) {
            return Error(ErrorEnum::eNotFound, "no item remover");
        }

        OutdatedItem item;

        item.mID             = id;
        item.mVersion        = version;
        item.mPartition      = mPartition;
        item.mRemover        = mRemover;
        item.mTimestamp      = timestamp;
        item.mSpaceAllocator = this;

        item.mFreeCallback
            = aos::StaticFunction<> {[this](void* sizePtr) { this->Free(reinterpret_cast<size_t>(sizePtr)); }};

        LockGuard lock {mPartition->mMutex};

        return mPartition->AddOutdatedItem(item);
    }

    /**
     * Restores outdated item.
     *
     * @param id item id.
     * @param version item version.
     * @return Error.
     */
    Error RestoreOutdatedItem(const String& id, const String& version) override
    {
        LockGuard lock {mPartition->mMutex};

        mPartition->RestoreOutdatedItem(id, version);

        return ErrorEnum::eNone;
    }

    /**
     * Removes the oldest outdated item.
     *
     * @return RetWithError<size_t>: freed size, eNotFound if there is no outdated item to remove.
     */
    RetWithError<size_t> RemoveOutdatedItem() override
    {
        LockGuard lock {mPartition->mMutex};

        return mPartition->RemoveOutdatedItem(this);
    }

private:
    Error ResizeSpace(size_t oldSize, size_t newSize)
    {
        if (oldSize == newSize) {
            return ErrorEnum::eNone;
        }

        LockGuard lock {mPartition->mMutex};

        Free(oldSize);

        if (auto err = Allocate(newSize); !err.IsNone()) {
            return err;
        }

        if (auto err = mPartition->Resize(static_cast<int64_t>(newSize) - static_cast<int64_t>(oldSize));
            !err.IsNone()) {
            Free(newSize);

            return err;
        }

        return ErrorEnum::eNone;
    }

    void NewPartition(const String& path, Partition& partition)
    {
        partition.mMountPoint = path;
        partition.mPlatformFS = mPlatformFS;
    }

    Error UpdateSizes()
    {
        auto [totalSize, totalSizeErr] = mPlatformFS->GetTotalSize(mPartition->mMountPoint);
        if (!totalSizeErr.IsNone()) {
            return totalSizeErr;
        }

        mSizeLimit = totalSize * mLimitPercent / 100;

        auto [allocatedSize, err] = mPlatformFS->GetDirSize(mPath);
        if (!err.IsNone()) {
            return err;
        }

        mAllocatedSize = allocatedSize;

        return ErrorEnum::eNone;
    }

    Error Allocate(size_t size)
    {
        if (mLimitPercent == 0) {
            return ErrorEnum::eNone;
        }

        if (mAllocationCount == 0) {
            if (auto err = UpdateSizes(); !err.IsNone()) {
                return err;
            }
        }

        if (mAllocatedSize + size > mSizeLimit) {
            if (auto err = mPartition->RemoveOutdatedItems(mAllocatedSize + size - mSizeLimit, this).mError;
                !err.IsNone()) {
                return err;
            }

            if (mAllocatedSize + size > mSizeLimit) {
                return Error(ErrorEnum::eNoMemory, "allocator limit exceeded");
            }
        }

        mAllocatedSize += size;
        mAllocationCount++;

        return ErrorEnum::eNone;
    }

    void Free(size_t size)
    {
        if (mLimitPercent == 0) {
            return;
        }

        if (size < mAllocatedSize) {
            mAllocatedSize -= size;
        } else {
            mAllocatedSize = 0;
        }
    }

    Error Done()
    {
        if (mLimitPercent == 0) {
            return ErrorEnum::eNone;
        }

        if (mAllocationCount == 0) {
            return Error(ErrorEnum::eNotFound, "no allocation");
        }

        mAllocationCount--;

        return ErrorEnum::eNone;
    }

    AllocatorItf*              mAllocator {};
    size_t                     mLimitPercent {};
    size_t                     mSizeLimit {};
    size_t                     mAllocationCount {};
    size_t                     mAllocatedSize {};
    StaticString<cFilePathLen> mPath;
    ItemRemoverItf*            mRemover {};
    fs::FSPlatformItf*         mPlatformFS {};
    Partition*                 mPartition {};
};

} // namespace aos::spaceallocator

#endif
