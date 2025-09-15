/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_IMAGEMANAGER_ITF_STORAGE_HPP_
#define AOS_CORE_CM_IMAGEMANAGER_ITF_STORAGE_HPP_

#include <core/cm/imagemanager/itf/imagemanager.hpp>

namespace aos::cm::imagemanager::storage {

/**
 * Image state type.
 */
class ItemStateType {
public:
    enum class Enum {
        eActive,
        eCached,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStateStrings[] = {
            "active",
            "cached",
        };

        return Array<const char* const>(sStateStrings, ArraySize(sStateStrings));
    };
};

using ItemStateEnum = ItemStateType::Enum;
using ItemState     = EnumStringer<ItemStateType>;

/**
 * Image info.
 */
struct ImageInfo : public aos::ImageInfo {
    /**
     * Constructor.
     */
    ImageInfo() = default;

    StaticString<cURLLen>             mURL;
    StaticArray<uint8_t, cSHA256Size> mSHA256;
    size_t                            mSize {};
    StaticString<cFilePathLen>        mPath;

    /**
     * Compares image info.
     *
     * @param other image info to compare with.
     * @return bool.
     */
    // cppcheck-suppress duplInheritedMember
    bool operator==(const ImageInfo& other) const
    {
        return mURL == other.mURL && mSHA256 == other.mSHA256 && mSize == other.mSize && mPath == other.mPath
            && static_cast<const aos::ImageInfo&>(*this) == static_cast<const aos::ImageInfo&>(other);
    }

    /**
     * Compares image info.
     *
     * @param other image info to compare with.
     * @return bool.
     */
    // cppcheck-suppress duplInheritedMember
    bool operator!=(const ImageInfo& other) const { return !operator==(other); }
};

/**
 * Item info.
 */
struct ItemInfo {
    StaticString<cIDLen>                        mID;
    StaticString<cVersionLen>                   mVersion;
    storage::ItemState                          mState;
    StaticString<cFilePathLen>                  mPath;
    size_t                                      mTotalSize {};
    Time                                        mTimestamp;
    StaticArray<ImageInfo, cMaxNumUpdateImages> mImages;

    /**
     * Compares item info.
     *
     * @param other item info to compare with.
     * @return bool.
     */
    bool operator==(const ItemInfo& other) const
    {
        return mID == other.mID && mVersion == other.mVersion && mState == other.mState && mPath == other.mPath
            && mTotalSize == other.mTotalSize && mTimestamp == other.mTimestamp && mImages == other.mImages;
    }

    /**
     * Compares item info.
     *
     * @param other item info to compare with.
     * @return bool.
     */
    bool operator!=(const ItemInfo& other) const { return !operator==(other); }
};

/**
 * Storage interface.
 */
class StorageItf {
public:
    /**
     * Destructor.
     */
    virtual ~StorageItf() = default;

    /**
     * Sets item state.
     *
     * @param id ID.
     * @param Version Version.
     * @param state Item state.
     * @return Error.
     */
    virtual Error SetItemState(const String& id, const String& Version, ItemState state) = 0;

    /**
     * Removes item.
     *
     * @param id ID.
     * @param version Version.
     * @return Error.
     */
    virtual Error RemoveItem(const String& id, const String& version) = 0;

    /**
     * Gets items info.
     *
     * @param services Items info.
     * @return Error.
     */
    virtual Error GetItemsInfo(Array<ItemInfo>& services) = 0;

    /**
     * Gets item versions by ID.
     *
     * @param id ID.
     * @param services Items info.
     * @return Error.
     */
    virtual Error GetItemVersionsByID(const String& id, Array<ItemInfo>& services) = 0;

    /**
     * Adds item.
     *
     * @param serviceItem Item info.
     * @return Error.
     */
    virtual Error AddItem(const ItemInfo& serviceItem) = 0;
};

} // namespace aos::cm::imagemanager::storage

#endif // AOS_CORE_CM_IMAGEMANAGER_ITF_STORAGE_HPP_
