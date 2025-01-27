/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/layermanager.hpp"
#include "aos/common/tools/fs.hpp"
#include "log.hpp"

namespace aos::sm::layermanager {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

LayerData CreateLayerData(const LayerInfo& layer, const size_t size, const String& path)
{
    LayerData layerData = {};

    layerData.mLayerID     = layer.mLayerID;
    layerData.mLayerDigest = layer.mLayerDigest;
    layerData.mVersion     = layer.mVersion;
    layerData.mPath        = path;
    layerData.mSize        = size;
    layerData.mState       = LayerStateEnum::eActive;
    layerData.mTimestamp   = Time::Now();

    return layerData;
}

void AcceptAllocatedSpace(spaceallocator::SpaceItf* space)
{
    if (!space) {
        return;
    }

    if (auto err = space->Accept(); !err.IsNone()) {
        LOG_ERR() << "Can't accept space: err=" << err;
    }
}

void ReleaseAllocatedSpace(const String& path, spaceallocator::SpaceItf* space)
{
    if (!path.IsEmpty()) {
        FS::RemoveAll(path);
    }

    if (!space) {
        return;
    }

    if (auto err = space->Release(); !err.IsNone()) {
        LOG_ERR() << "Can't release space: err=" << err;
    }
}

}; // namespace

/***********************************************************************************************************************
 * LayerManager
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error LayerManager::Init(const Config& config, spaceallocator::SpaceAllocatorItf& layerSpaceAllocator,
    spaceallocator::SpaceAllocatorItf& downloadSpaceAllocator, StorageItf& storage,
    downloader::DownloaderItf& downloader, image::ImageHandlerItf& imageHandler)
{
    LOG_DBG() << "Init layer manager";

    mConfig = config;

    LOG_DBG() << "Config: layersDir=" << mConfig.mLayersDir << ", downloadDir=" << mConfig.mDownloadDir
              << ", ttl=" << mConfig.mTTL;

    mLayerSpaceAllocator    = &layerSpaceAllocator;
    mDownloadSpaceAllocator = &downloadSpaceAllocator;
    mStorage                = &storage;
    mDownloader             = &downloader;
    mImageHandler           = &imageHandler;

    if (auto err = FS::ClearDir(mConfig.mDownloadDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = FS::MakeDirAll(mConfig.mLayersDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = RemoveDamagedLayerFolders(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = SetOutdatedLayers(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = RemoveOutdatedLayers(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error LayerManager::Start()
{
    LOG_DBG() << "Start layer manager";

    auto err = mTimer.Create(
        mConfig.mRemoveOutdatedPeriod,
        [this](void*) {
            if (auto err = RemoveOutdatedLayers(); !err.IsNone()) {
                LOG_ERR() << "Failed to remove outdated layers: err=" << err;
            }
        },
        false);

    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error LayerManager::Stop()
{
    LOG_DBG() << "Stop layer manager";

    if (auto err = mTimer.Stop(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error LayerManager::GetLayer(const String& digest, LayerData& layer) const
{
    LOG_DBG() << "Get layer info by digest: digest=" << digest;

    return mStorage->GetLayer(digest, layer);
}

Error LayerManager::ProcessDesiredLayers(const Array<LayerInfo>& desiredLayers)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Process desired layers";

    auto storageLayers = MakeUnique<LayerDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllLayers(*storageLayers); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto layersToInstall = MakeUnique<LayerInfoStaticArray>(&mAllocator, desiredLayers);

    if (auto err = UpdateCachedLayers(*storageLayers, *layersToInstall); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mInstallPool.Run(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& layer : *layersToInstall) {
        auto err = mInstallPool.AddTask([this, &layer](void*) {
            if (auto err = InstallLayer(layer); !err.IsNone()) {
                LOG_ERR() << "Failed to install layer: id=" << layer.mLayerID << ", version=" << layer.mVersion
                          << ", digest=" << layer.mLayerDigest << ", err=" << err;
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Failed to add layer install task: id=" << layer.mLayerID << ", version=" << layer.mVersion
                      << ", digest=" << layer.mLayerDigest << ", err=" << err;
        }
    }

    if (auto err = mInstallPool.Wait(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "All desired layers installed";

    if (auto err = mInstallPool.Shutdown(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Allocator: allocated=" << mAllocator.MaxAllocatedSize() << ", maxSize=" << mAllocator.MaxSize();
#if AOS_CONFIG_THREAD_STACK_USAGE
    LOG_DBG() << "Stack usage: size=" << mInstallPool.GetStackUsage();
#endif

    return ErrorEnum::eNone;
}

Error LayerManager::ValidateLayer(const LayerData& layer)
{
    LOG_DBG() << "Validate layer: id=" << layer.mLayerID << ", version=" << layer.mVersion
              << ", digest=" << layer.mLayerDigest;

    const auto [digest, err] = mImageHandler->CalculateDigest(layer.mPath);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (digest != layer.mLayerDigest) {
        LOG_ERR() << "Layer digest mismatch: digest=" << layer.mLayerDigest << ", calculatedDigest=" << digest;

        return ErrorEnum::eInvalidChecksum;
    }

    return ErrorEnum::eNone;
}

Error LayerManager::RemoveItem(const String& id)
{
    LOG_DBG() << "Remove item: id=" << id;

    LayerData layer;

    if (auto err = mStorage->GetLayer(id, layer); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = RemoveLayer(layer); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error LayerManager::RemoveDamagedLayerFolders()
{
    LOG_DBG() << "Remove damaged layer folders";

    auto layers = MakeUnique<LayerDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllLayers(*layers); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& layer : *layers) {
        if (auto [exists, err] = FS::DirExist(layer.mPath); !err.IsNone() || !exists) {
            LOG_WRN() << "Layer folder does not exist: path=" << layer.mPath;

            if (auto removeErr = RemoveLayer(layer); !removeErr.IsNone()) {
                return AOS_ERROR_WRAP(removeErr);
            }
        }
    }

    auto layerDirIterator = MakeUnique<FS::DirIterator>(&mAllocator, mConfig.mLayersDir);

    while (layerDirIterator->Next()) {
        if (!(*layerDirIterator)->mIsDir) {
            continue;
        }

        const auto algorithmPath             = FS::JoinPath(mConfig.mLayersDir, (*layerDirIterator)->mPath);
        auto       layerAlgorithmDirIterator = MakeUnique<FS::DirIterator>(&mAllocator, algorithmPath);

        while (layerAlgorithmDirIterator->Next()) {
            const auto layerPath = FS::JoinPath(algorithmPath, (*layerAlgorithmDirIterator)->mPath);

            const auto it = layers->FindIf([&layerPath](const auto& layer) { return layer.mPath == layerPath; });
            if (it == layers->end()) {
                LOG_WRN() << "Layer missing in storage: path=" << layerPath;

                if (auto removeErr = FS::RemoveAll(layerPath); !removeErr.IsNone()) {
                    return AOS_ERROR_WRAP(removeErr);
                }
            }
        }
    }

    return ErrorEnum::eNone;
}

Error LayerManager::SetOutdatedLayers()
{
    LOG_DBG() << "Set outdated layers";

    auto layers = MakeUnique<LayerDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllLayers(*layers); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& layer : *layers) {
        if (layer.mState != LayerStateEnum::eCached) {
            continue;
        }

        if (auto err = mLayerSpaceAllocator->AddOutdatedItem(layer.mLayerDigest, layer.mSize, layer.mTimestamp);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error LayerManager::SetLayerState(const LayerData& layer, LayerState state)
{
    LOG_DBG() << "Set layer state: id=" << layer.mLayerID << ", version=" << layer.mVersion
              << ", digest=" << layer.mLayerDigest << ", state=" << state;

    auto updatedLayer = layer;

    updatedLayer.mState     = state;
    updatedLayer.mTimestamp = Time::Now();

    if (auto err = mStorage->UpdateLayer(updatedLayer); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (state == LayerStateEnum::eCached) {
        if (auto err = mLayerSpaceAllocator->AddOutdatedItem(layer.mLayerDigest, layer.mSize, layer.mTimestamp);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    if (auto err = mLayerSpaceAllocator->RestoreOutdatedItem(layer.mLayerDigest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error LayerManager::RemoveOutdatedLayers()
{
    LOG_DBG() << "Remove outdated layers";

    auto layers = MakeUnique<LayerDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllLayers(*layers); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& layer : *layers) {
        if (layer.mState != LayerStateEnum::eCached) {
            continue;
        }

        auto expiredTime = layer.mTimestamp.Add(mConfig.mTTL);
        if (Time::Now() < expiredTime) {
            continue;
        }

        if (auto err = RemoveLayer(layer); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (auto err = mLayerSpaceAllocator->RestoreOutdatedItem(layer.mLayerDigest); !err.IsNone()) {
            LOG_WRN() << "Failed to restore outdated item: err=" << err;
        }
    }

    return ErrorEnum::eNone;
}

Error LayerManager::RemoveLayer(const LayerData& layer)
{
    LOG_DBG() << "Remove layer: id=" << layer.mLayerID << ", version=" << layer.mVersion
              << ", digest=" << layer.mLayerDigest << ", path=" << layer.mPath;

    if (auto err = FS::RemoveAll(layer.mPath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mStorage->RemoveLayer(layer.mLayerDigest); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_INF() << "Layer successfully removed: digest=" << layer.mLayerDigest;

    return ErrorEnum::eNone;
}

Error LayerManager::UpdateCachedLayers(const Array<LayerData>& stored, Array<LayerInfo>& result)
{
    LOG_DBG() << "Update cached layers";

    for (const auto& storageLayer : stored) {
        auto layer = result.FindIf([&storageLayer](const auto& desiredLayer) {
            return storageLayer.mLayerDigest == desiredLayer.mLayerDigest;
        });

        if (layer != result.end()) {
            if (storageLayer.mState == LayerStateEnum::eCached) {
                if (auto err = SetLayerState(storageLayer, LayerStateEnum::eActive); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }
            }

            result.Erase(layer);

            continue;
        }

        if (storageLayer.mState != LayerStateEnum::eCached) {
            if (auto err = SetLayerState(storageLayer, LayerStateEnum::eCached); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

Error LayerManager::InstallLayer(const LayerInfo& layer)
{
    LOG_DBG() << "Install layer: id=" << layer.mLayerID << ", version=" << layer.mVersion
              << ", digest=" << layer.mLayerDigest;

    Error                               err = ErrorEnum::eNone;
    UniquePtr<spaceallocator::SpaceItf> downloadSpace;
    UniquePtr<spaceallocator::SpaceItf> unpackedSpace;
    StaticString<cFilePathLen>          archivePath;
    StaticString<cFilePathLen>          storeLayerPath;

    auto cleanupDownload = DeferRelease(&err, [&](Error*) {
        LOG_DBG() << "Cleanup download space";

        ReleaseAllocatedSpace(archivePath, downloadSpace.Get());
    });

    auto cleanupLayer = DeferRelease(&err, [&](Error* err) {
        if (err->IsNone()) {
            LOG_DBG() << "Accept layer space";

            AcceptAllocatedSpace(unpackedSpace.Get());

            return;
        }

        LOG_DBG() << "Cleanup layer space";

        ReleaseAllocatedSpace(storeLayerPath, unpackedSpace.Get());
    });

    Tie(downloadSpace, err) = mDownloadSpaceAllocator->AllocateSpace(layer.mSize);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    archivePath = FS::JoinPath(mConfig.mDownloadDir, layer.mLayerDigest);

    if (err = mDownloader->Download(layer.mURL, archivePath, downloader::DownloadContentEnum::eLayer); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (Tie(storeLayerPath, err) = mImageHandler->InstallLayer(archivePath, mConfig.mLayersDir, layer, unpackedSpace);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto layerData = MakeUnique<LayerData>(&mAllocator);
    *layerData     = CreateLayerData(layer, unpackedSpace->Size(), storeLayerPath);

    if (err = mStorage->AddLayer(*layerData); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_INF() << "Layer successfully installed: id=" << layerData->mLayerID << ", version=" << layerData->mVersion
              << ", digest=" << layerData->mLayerDigest;

    return ErrorEnum::eNone;
}

} // namespace aos::sm::layermanager
