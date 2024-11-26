/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LAYERMANAGER_HPP_
#define AOS_LAYERMANAGER_HPP_

#include "aos/common/types.hpp"

namespace aos {
namespace sm {
namespace layermanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Layer info.
 */
struct LayerInfo {
    StaticString<cLayerDigestLen> mLayerDigest;
    StaticString<cLayerIDLen>     mLayerID;
    StaticString<cVersionLen>     mVersion;
    StaticString<cFilePathLen>    mPath;
    StaticString<cVersionLen>     mOSVersion;
    Time                          mTimestamp;
    bool                          mCached;
    size_t                        mSize;

    /**
     * Compares layer info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator==(const LayerInfo& info) const
    {
        return mLayerDigest == info.mLayerDigest && mLayerID == info.mLayerID && mVersion == info.mVersion
            && mPath == info.mPath && mOSVersion == info.mOSVersion && mTimestamp == info.mTimestamp
            && mCached == info.mCached && mSize == info.mSize;
    }

    /**
     * Compares layer info.
     *
     * @param info info to compare.
     * @return bool.
     */
    bool operator!=(const LayerInfo& info) const { return !operator==(info); }
};

/**
 * Layer manager storage interface.
 */
class StorageItf {
public:
    /**
     * Adds layer to storage.
     *
     * @param layer layer info to add.
     * @return Error.
     */
    virtual Error AddLayer(const LayerInfo& layer) = 0;

    /**
     * Removes layer from storage by digest.
     *
     * @param digest layer digest.
     * @return Error.
     */
    virtual Error DeleteLayerByDigest(const String& digest) = 0;

    /**
     * Returns all stored layers.
     *
     * @param layers[out] array to return stored layers.
     * @return Error.
     */
    virtual Error GetLayersInfo(Array<LayerInfo>& layers) const = 0;

    /**
     * Returns layer info by digest.
     *
     * @param digest layer digest.
     * @param[out] layer layer info.
     * @return Error.
     */
    virtual Error GetLayerInfoByDigest(const String& digest, LayerInfo& layer) const = 0;

    /**
     * Updates layer.
     *
     * @param layer layer info to update.
     * @return Error.
     */
    virtual Error UpdateLayer(const LayerInfo& layer) = 0;

    /**
     * Destroys storage interface.
     */
    virtual ~StorageItf() = default;
};

/** @}*/

} // namespace layermanager
} // namespace sm
} // namespace aos

#endif
