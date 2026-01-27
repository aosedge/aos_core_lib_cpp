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

#include "itf/spaceallocator.hpp"

namespace aos::spaceallocator {

/**
 * Space instance.
 */
class Space : public SpaceItf {
public:
    /**
     * Crates space instance.
     */
    Space(size_t size, SpaceAllocatorItf* allocator)
        : mSize(size)
        , mAllocator(allocator)
    {
    }

    /**
     * Accepts space.
     *
     * @return Error.
     */
    Error Accept() override { return mAllocator->AllocateDone(); }

    /**
     * Releases space.
     *
     * @return Error.
     */
    Error Release() override
    {
        mAllocator->FreeSpace(mSize);

        return mAllocator->AllocateDone();
    }

    /**
     * Resizes space.
     *
     * @param size new size.
     * @return Error.
     */
    Error Resize(size_t size) override
    {
        mSize = size;

        return ErrorEnum::eNone;
    }

    /**
     * Returns space size.
     *
     * @return size_t.
     */
    size_t Size() const override { return mSize; }

private:
    size_t             mSize;
    SpaceAllocatorItf* mAllocator {};
};

struct Partition;

/**
 * Outdated item.
 */
struct OutdatedItem {
    StaticString<cIDLen>                    mID;
    StaticString<cVersionLen>               mVersion;
    SpaceAllocatorItf*                      mAllocator {};
    StaticFunction<cDefaultFunctionMaxSize> mFreeCallback;
    Partition*                              mPartition {};
    ItemRemoverItf*                         mRemover {};
    Time                                    mTimestamp;
};

/**
 * Partition.
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
    Error RemoveLimit(size_t limit)
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
    Error Allocate(size_t size)
    {
        LockGuard lock {mMutex};

        if (mAllocationCount == 0) {
            auto [availableSize, err] = mPlatformFS->GetAvailableSize(mMountPoint);
            if (!err.IsNone()) {
                return err;
            }

            mAvailableSize = availableSize;
        }

        if (size > mAvailableSize) {
            if (mOutdatedItems.Size() == 0) {
                return Error(ErrorEnum::eNoMemory, "not enough space");
            }

            auto [freedSize, err] = RemoveOutdatedItems(size - mAvailableSize);
            if (!err.IsNone()) {
                return err;
            }

            mAvailableSize += freedSize;

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
    void Free(size_t size)
    {
        LockGuard lock {mMutex};

        if (mAllocationCount == 0) {
            return;
        }

        mAvailableSize += size;
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
     * @return Error.
     */
    Error AddOutdatedItem(const OutdatedItem& item)
    {
        LockGuard lock {mMutex};

        for (auto& outdatedItem : mOutdatedItems) {
            if (outdatedItem.mID == item.mID) {
                outdatedItem = item;

                return ErrorEnum::eNone;
            }
        }

        Error err = mOutdatedItems.PushBack(item);
        if (!err.IsNone()) {
            mOutdatedItems.Sort(
                [](const OutdatedItem& a, const OutdatedItem& b) { return a.mTimestamp < b.mTimestamp; });

            auto& oldestItem = mOutdatedItems[0];

            auto [size, removeErr] = oldestItem.mRemover->RemoveItem(oldestItem.mID, oldestItem.mVersion);
            if (!removeErr.IsNone()) {
                return removeErr;
            }

            oldestItem.mFreeCallback(reinterpret_cast<void*>(size));
            mAvailableSize += size;

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
        LockGuard lock {mMutex};

        for (size_t i = 0; i < mOutdatedItems.Size(); i++) {
            if (mOutdatedItems[i].mID == id && mOutdatedItems[i].mVersion == version) {
                mOutdatedItems.Erase(mOutdatedItems.begin() + i);

                break;
            }
        }
    }

    static constexpr size_t cMaxNumOutdatedItems = AOS_CONFIG_SPACEALLOCATOR_MAX_OUTDATED_ITEMS;

    StaticString<cFilePathLen>                      mMountPoint;
    size_t                                          mLimit {};
    size_t                                          mTotalSize {};
    size_t                                          mAllocatorCount {};
    fs::FSPlatformItf*                              mPlatformFS {};
    StaticArray<OutdatedItem, cMaxNumOutdatedItems> mOutdatedItems;

private:
    static constexpr size_t cMaxLimit = 100;

    RetWithError<size_t> RemoveOutdatedItems(size_t size)
    {
        mOutdatedItems.Sort([](const OutdatedItem& a, const OutdatedItem& b) { return a.mTimestamp < b.mTimestamp; });

        size_t freedSize = 0;
        size_t i         = 0;

        for (; freedSize < size && i < mOutdatedItems.Size(); i++) {
            auto& item = mOutdatedItems[i];

            auto [itemSize, err] = item.mRemover->RemoveItem(item.mID, item.mVersion);
            if (!err.IsNone()) {
                return {freedSize, err};
            }

            item.mFreeCallback(reinterpret_cast<void*>(itemSize));
            freedSize += itemSize;
        }

        if (i > 0) {
            mOutdatedItems.Erase(mOutdatedItems.begin(), mOutdatedItems.begin() + i);
        }

        return {freedSize, ErrorEnum::eNone};
    }

    size_t mAllocationCount {};
    size_t mAvailableSize {};
    Mutex  mMutex {};
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
public:
    /**
     * Constructor.
     */
    SpaceAllocator() = default;

    /**
     * Initializes space allocator.
     *
     * @param path path to allocate space.
     * @param platformFS platform file system.
     * @param limit limit in percents.
     * @param remover item remover.
     * @return Error.
     */
    Error Init(const String& path, fs::FSPlatformItf& platformFS, size_t limit = 0, ItemRemoverItf* remover = nullptr)
    {
        LockGuard lock {mPartitionsMutex};

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

            if (auto err = NewPartition(mountPoint, partitionIt->mSecond); !err.IsNone()) {
                return err;
            }
        }

        mPartition = &partitionIt->mSecond;
        mPartition->mAllocatorCount++;

        if (limit != 0) {
            if (auto err = mPartition->AddLimit(limit); !err.IsNone()) {
                return err;
            }

            mSizeLimit = mPartition->mTotalSize * mPartition->mLimit / 100;
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
    RetWithError<UniquePtr<SpaceItf>> AllocateSpace(size_t size) override
    {
        auto err = Allocate(size);
        if (!err.IsNone()) {
            return {nullptr, err};
        }

        if (err = mPartition->Allocate(size); !err.IsNone()) {
            return {nullptr, err};
        }

        return UniquePtr<SpaceItf>(MakeUnique<Space>(&mAllocator, size, this));
    };

    /**
     * Frees space.
     *
     * @param size size to free.
     * @return void.
     */
    void FreeSpace(size_t size) override
    {
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

        item.mID        = id;
        item.mVersion   = version;
        item.mPartition = mPartition;
        item.mRemover   = mRemover;
        item.mTimestamp = timestamp;
        item.mAllocator = this;

        item.mFreeCallback
            = aos::StaticFunction<> {[this](void* sizePtr) { this->Free(reinterpret_cast<size_t>(sizePtr)); }};

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
        mPartition->RestoreOutdatedItem(id, version);

        return ErrorEnum::eNone;
    }

private:
    Error NewPartition(const String& path, Partition& partition)
    {
        auto [totalSize, err] = mPlatformFS->GetTotalSize(path);
        if (!err.IsNone()) {
            return err;
        }

        partition.mMountPoint = path;
        partition.mTotalSize  = totalSize;
        partition.mPlatformFS = mPlatformFS;

        return ErrorEnum::eNone;
    }

    Error Allocate(size_t size)
    {
        LockGuard lock {mMutex};

        if (mSizeLimit == 0) {
            return ErrorEnum::eNone;
        }

        if (mAllocationCount == 0) {
            auto [allocatedSize, err] = mPlatformFS->GetDirSize(mPath);
            if (!err.IsNone()) {
                return err;
            }

            mAllocatedSize = allocatedSize;
        }

        if (mAllocatedSize + size > mSizeLimit) {
            size_t outdatedCount = 0;
            for (const auto& item : mPartition->mOutdatedItems) {
                if (item.mAllocator == this) {
                    outdatedCount++;
                }
            }

            if (outdatedCount == 0) {
                return Error(ErrorEnum::eNoMemory, "allocator limit exceeded");
            }

            auto [freedSize, err] = RemoveOutdatedItems(mAllocatedSize + size - mSizeLimit);
            if (!err.IsNone()) {
                return err;
            }

            if (freedSize > mAllocatedSize) {
                mAllocatedSize = 0;
            } else {
                mAllocatedSize -= freedSize;
            }

            if (mAllocatedSize + size > mSizeLimit) {
                return Error(ErrorEnum::eNoMemory, "allocator limit exceeded");
            }
        }

        mAllocatedSize += size;
        mAllocationCount++;

        return ErrorEnum::eNone;
    }

    RetWithError<size_t> RemoveOutdatedItems(size_t size)
    {
        mPartition->mOutdatedItems.Sort(
            [](const OutdatedItem& a, const OutdatedItem& b) { return a.mTimestamp < b.mTimestamp; });

        size_t freedSize = 0;
        size_t i         = 0;

        for (auto& item : mPartition->mOutdatedItems) {
            if (item.mAllocator != this || freedSize >= size) {
                mPartition->mOutdatedItems[i] = item;
                i++;

                continue;
            }

            auto [itemSize, err] = item.mRemover->RemoveItem(item.mID, item.mVersion);
            if (!err.IsNone()) {
                return {freedSize, err};
            }

            item.mPartition->Free(itemSize);
            freedSize += itemSize;
        }

        if (i < mPartition->mOutdatedItems.Size()) {
            mPartition->mOutdatedItems.Erase(mPartition->mOutdatedItems.begin() + i, mPartition->mOutdatedItems.end());
        }

        return {freedSize, ErrorEnum::eNone};
    }

    void Free(size_t size)
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

    StaticAllocator<sizeof(Space) * cNumAllocations> mAllocator;
    size_t                                           mSizeLimit {};
    size_t                                           mAllocationCount {};
    size_t                                           mAllocatedSize {};
    StaticString<cFilePathLen>                       mPath;
    ItemRemoverItf*                                  mRemover {};
    fs::FSPlatformItf*                               mPlatformFS {};
    Partition*                                       mPartition {};
    Mutex                                            mMutex;
};

} // namespace aos::spaceallocator

#endif
