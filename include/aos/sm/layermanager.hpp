/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LAYERMANAGER_HPP_
#define AOS_LAYERMANAGER_HPP_

#include "aos/common/downloader/downloader.hpp"
#include "aos/common/spaceallocator/spaceallocator.hpp"
#include "aos/common/tools/timer.hpp"
#include "aos/common/types.hpp"
#include "aos/sm/config.hpp"
#include "aos/sm/image/imagehandler.hpp"

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
    StaticString<cLayerDigestLen> mUnpackedLayerDigest;
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
        return mLayerDigest == layer.mLayerDigest && mUnpackedLayerDigest == layer.mUnpackedLayerDigest
            && mLayerID == layer.mLayerID && mVersion == layer.mVersion && mPath == layer.mPath
            && mOSVersion == layer.mOSVersion && mState == layer.mState && mSize == layer.mSize;
    }

    /**
     * Compares layer data.
     *
     * @param layer layer data to compare.
     * @return bool.
     */
    bool operator!=(const LayerData& info) const { return !operator==(info); }
};

using LayerDataStaticArray = StaticArray<LayerData, cMaxNumLayers>;

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

/**
 * Layer manager interface.
 */
class LayerManagerItf {
public:
    /**
     * Returns layer data by digest.
     *
     * @param digest layer digest.
     * @param layer[out] layer data.
     * @return Error.
     */
    virtual Error GetLayer(const String& digest, LayerData& layer) const = 0;

    /**
     * Processes desired layers.
     *
     * @param desiredLayers desired layers.
     * @param layerStatuses[out] layer statuses.
     * @return Error.
     */
    virtual Error ProcessDesiredLayers(const Array<LayerInfo>& desiredLayers, Array<LayerStatus>& layerStatuses) = 0;

    /**
     * Validates layer.
     *
     * @param layer layer data.
     * @return Error.
     */
    virtual Error ValidateLayer(const LayerData& layer) = 0;

    /**
     * Removes layer.
     *
     * @param layer layer to remove.
     * @return Error.
     */
    virtual Error RemoveLayer(const LayerData& layer) = 0;

    /**
     *  Destructor.
     */
    virtual ~LayerManagerItf() = default;
};

/**
 * Layer manager configuration.
 */
struct Config {
    StaticString<cFilePathLen> mLayersDir;
    StaticString<cFilePathLen> mDownloadDir;
    size_t                     mPartLimit;
    Duration                   mTTL;
    Duration                   mRemoveOutdatedPeriod = 24 * Time::cHours;
};

/**
 * Layer manager interface implementation.
 */
class LayerManager : public LayerManagerItf, public spaceallocator::ItemRemoverItf {
public:
    /**
     * Initializes layer manager.
     *
     * @param config layer manager configuration.
     * @param layerSpaceAllocator layer space allocator.
     * @param downloadSpaceAllocator download space allocator.
     * @param storage layer storage.
     * @param downloader layer downloader.
     * @param imageHandler image handler.
     * @return Error.
     */
    Error Init(const Config& config, spaceallocator::SpaceAllocatorItf& layerSpaceAllocator,
        spaceallocator::SpaceAllocatorItf& downloadSpaceAllocator, StorageItf& storage,
        downloader::DownloaderItf& downloader, image::ImageHandlerItf& imageHandler);

    /**
     * Starts layer manager.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops layer manager.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Returns layer data by digest.
     *
     * @param digest layer digest.
     * @param layer[out] layer data.
     * @return Error.
     */
    Error GetLayer(const String& digest, LayerData& layer) const override;

    /**
     * Processes desired layers.
     *
     * @param desiredLayers desired layers.
     * @param layerStatuses[out] layer statuses.
     * @return Error.
     */
    Error ProcessDesiredLayers(const Array<LayerInfo>& desiredLayers, Array<LayerStatus>& layerStatuses) override;

    /**
     * Validates layer.
     *
     * @param layer layer data.
     * @return Error.
     */
    Error ValidateLayer(const LayerData& layer) override;

    /**
     * Removes layer.
     *
     * @param layer layer to remove.
     * @return Error.
     */
    Error RemoveLayer(const LayerData& layer) override;

    /**
     * Removes item.
     *
     * @param id item id.
     * @return Error.
     */
    Error RemoveItem(const String& id) override;

private:
    static constexpr auto cLayerOCIDescriptor = "layer.json";
    static constexpr auto cNumInstallThreads  = AOS_CONFIG_SERVICEMANAGER_NUM_COOPERATE_INSTALLS;
    static constexpr auto cAllocatorSize = Max(cNumInstallThreads * (sizeof(oci::ImageManifest) + sizeof(LayerData))
            + sizeof(LayerDataStaticArray) + sizeof(LayerInfoStaticArray),
        sizeof(LayerDataStaticArray) + sizeof(fs::DirIterator) * 2);

    Error PrepareSpaceForLayers(size_t desiredLayersNum);
    Error RemoveDamagedLayerFolders();
    Error SetOutdatedLayers();
    Error SetLayerState(const LayerData& layer, LayerState state);
    Error RemoveOutdatedLayers();
    Error RemoveLayerFromSystem(const LayerData& layer);
    Error UpdateCachedLayers(Array<LayerStatus>& statuses, Array<aos::LayerInfo>& layersToInstall);
    Error InstallLayer(const aos::LayerInfo& layer);

    Config                             mConfig                 = {};
    spaceallocator::SpaceAllocatorItf* mLayerSpaceAllocator    = nullptr;
    spaceallocator::SpaceAllocatorItf* mDownloadSpaceAllocator = nullptr;
    StorageItf*                        mStorage                = nullptr;
    downloader::DownloaderItf*         mDownloader             = nullptr;
    image::ImageHandlerItf*            mImageHandler           = nullptr;
    Mutex                              mMutex;
    Timer                              mTimer;

    StaticAllocator<cAllocatorSize> mAllocator;

    ThreadPool<cNumInstallThreads, cMaxNumLayers> mInstallPool;
};

/** @}*/

} // namespace aos::sm::layermanager

#endif
