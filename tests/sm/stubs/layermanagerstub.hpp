/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LAYERMANAGER_STUB_HPP_
#define AOS_LAYERMANAGER_STUB_HPP_

#include <algorithm>
#include <mutex>

#include "aos/sm/layermanager.hpp"

namespace aos::sm::layermanager {

/**
 * Storage stub.
 */
class StorageStub : public StorageItf {
public:
    /**
     * Adds layer to storage.
     *
     * @param layer layer data to add.
     * @return Error.
     */
    Error AddLayer(const LayerData& layer) override
    {
        LockGuard lock {mMutex};

        if (mLayers.Contains(layer.mLayerDigest)) {
            return ErrorEnum::eAlreadyExist;
        }

        if (auto err = mLayers.Set(layer.mLayerDigest, layer); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    /**
     * Removes layer from storage.
     *
     * @param digest layer digest.
     * @return Error.
     */
    Error RemoveLayer(const String& digest) override
    {
        LockGuard lock {mMutex};

        if (auto err = mLayers.Remove(digest); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    /**
     * Returns all stored layers.
     *
     * @param layers[out] array to return stored layers.
     * @return Error.
     */
    Error GetAllLayers(Array<LayerData>& layers) const override
    {
        LockGuard lock {mMutex};

        for (const auto& [_, layer] : mLayers) {
            if (auto err = layers.PushBack(layer); !err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Returns layer data.
     *
     * @param digest layer digest.
     * @param[out] layer layer data.
     * @return Error.
     */
    Error GetLayer(const String& digest, LayerData& layer) const override
    {
        LockGuard lock {mMutex};

        auto it = mLayers.Find(digest);
        if (it == mLayers.end()) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        layer = it->mSecond;

        return ErrorEnum::eNone;
    }

    /**
     * Updates layer.
     *
     * @param layer layer data to update.
     * @return Error.
     */
    Error UpdateLayer(const LayerData& layer) override
    {
        LockGuard lock {mMutex};

        auto it = mLayers.Find(layer.mLayerDigest);
        if (it == mLayers.end()) {
            return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
        }

        it->mSecond = layer;

        return ErrorEnum::eNone;
    }

private:
    mutable Mutex                                                      mMutex;
    StaticMap<StaticString<cLayerDigestLen>, LayerData, cMaxNumLayers> mLayers;
};

/**
 * Layer manager stub.
 */
class LayerManagerStub : public LayerManagerItf {
public:
    /**
     * Returns layer data by digest.
     *
     * @param digest layer digest.
     * @param layer[out] layer data.
     * @return Error.
     */
    Error GetLayer(const String& digest, LayerData& layer) const override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mLayersData.begin(), mLayersData.end(),
            [&digest](const LayerData& layer) { return layer.mLayerDigest == digest; });
        if (it == mLayersData.end()) {
            return ErrorEnum::eNotFound;
        }

        layer = *it;

        return ErrorEnum::eNone;
    }

    /**
     * Processes desired layers.
     *
     * @param desiredLayers desired layers.
     * @return Error.
     */
    Error ProcessDesiredLayers(const Array<aos::LayerInfo>& desiredLayers) override
    {
        std::lock_guard lock {mMutex};

        mLayersData.clear();

        std::transform(
            desiredLayers.begin(), desiredLayers.end(), std::back_inserter(mLayersData), [](const LayerInfo& layer) {
                return LayerData {layer.mLayerDigest, layer.mLayerID, layer.mVersion,
                    FS::JoinPath("/aos/layers", layer.mLayerDigest), "", Time::Now(), LayerStateEnum::eActive, 0};
            });

        return ErrorEnum::eNone;
    }

    /**
     * Validates layer.
     *
     * @param layer layer data.
     * @return Error.
     */
    Error ValidateLayer(const LayerData& layer) override
    {
        (void)layer;

        return ErrorEnum::eNone;
    }

private:
    mutable std::mutex     mMutex;
    std::vector<LayerData> mLayersData;
};

} // namespace aos::sm::layermanager

#endif
