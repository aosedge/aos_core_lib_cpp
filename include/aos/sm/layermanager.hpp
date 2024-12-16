/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LAYERMANAGER_HPP_
#define AOS_LAYERMANAGER_HPP_

#include "aos/common/types.hpp"

namespace aos::sm::layermanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Layer state type.
 */
class LayerStateType {
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

using LayerStateEnum = LayerStateType::Enum;
using LayerState     = EnumStringer<LayerStateType>;

/**
 * Layer data.
 */
struct LayerData {
    StaticString<cLayerDigestLen> mLayerDigest;
    StaticString<cLayerIDLen>     mLayerID;
    StaticString<cVersionLen>     mVersion;
    StaticString<cFilePathLen>    mPath;
    StaticString<cVersionLen>     mOSVersion;
    Time                          mTimestamp;
    LayerState                    mState;
    size_t                        mSize;

    /**
     * Compares layer data.
     *
     * @param layer layer data to compare.
     * @return bool.
     */
    bool operator==(const LayerData& layer) const
    {
        return mLayerDigest == layer.mLayerDigest && mLayerID == layer.mLayerID && mVersion == layer.mVersion
            && mPath == layer.mPath && mOSVersion == layer.mOSVersion && mState == layer.mState && mSize == layer.mSize;
    }

    /**
     * Compares layer data.
     *
     * @param layer layer data to compare.
     * @return bool.
     */
    bool operator!=(const LayerData& info) const { return !operator==(info); }
};

/**
 * Layer manager storage interface.
 */
class StorageItf {
public:
    /**
     * Adds layer to storage.
     *
     * @param layer layer data to add.
     * @return Error.
     */
    virtual Error AddLayer(const LayerData& layer) = 0;

    /**
     * Removes layer from storage.
     *
     * @param digest layer digest.
     * @return Error.
     */
    virtual Error RemoveLayer(const String& digest) = 0;

    /**
     * Returns all stored layers.
     *
     * @param layers[out] array to return stored layers.
     * @return Error.
     */
    virtual Error GetAllLayers(Array<LayerData>& layers) const = 0;

    /**
     * Returns layer data.
     *
     * @param digest layer digest.
     * @param[out] layer layer data.
     * @return Error.
     */
    virtual Error GetLayer(const String& digest, LayerData& layer) const = 0;

    /**
     * Updates layer.
     *
     * @param layer layer data to update.
     * @return Error.
     */
    virtual Error UpdateLayer(const LayerData& layer) = 0;

    /**
     * Destroys storage interface.
     */
    virtual ~StorageItf() = default;
};

/** @}*/

} // namespace aos::sm::layermanager

#endif
