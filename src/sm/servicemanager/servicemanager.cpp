/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/servicemanager.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/common/tools/semver.hpp"
#include "aos/common/tools/uuid.hpp"

#include "log.hpp"

namespace aos::sm::servicemanager {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

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

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

ServiceManager::~ServiceManager()
{
    mInstallPool.Shutdown();
}

Error ServiceManager::Init(const Config& config, oci::OCISpecItf& ociManager, downloader::DownloaderItf& downloader,
    StorageItf& storage, spaceallocator::SpaceAllocatorItf& serviceSpaceAllocator,
    spaceallocator::SpaceAllocatorItf& downloadSpaceAllocator, image::ImageHandlerItf& imageHandler)
{
    LOG_DBG() << "Init service manager";

    mConfig                 = config;
    mOCIManager             = &ociManager;
    mDownloader             = &downloader;
    mStorage                = &storage;
    mServiceSpaceAllocator  = &serviceSpaceAllocator;
    mDownloadSpaceAllocator = &downloadSpaceAllocator;
    mImageHandler           = &imageHandler;

    if (auto err = FS::MakeDirAll(mConfig.mServicesDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = FS::ClearDir(mConfig.mDownloadDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto services = MakeUnique<ServiceDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllServices(*services); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    for (const auto& service : *services) {
        if (service.mState != ServiceStateEnum::eCached) {
            continue;
        }

        auto [allocationId, err] = FormatAllocatorItemID(service);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (err = mServiceSpaceAllocator->AddOutdatedItem(allocationId, service.mSize, service.mTimestamp);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    if (auto err = RemoveDamagedServiceFolders(*services); !err.IsNone()) {
        LOG_ERR() << "Can't remove damaged service folders: err=" << err;
    }

    if (auto err = RemoveOutdatedServices(*services); !err.IsNone()) {
        LOG_ERR() << "Can't remove outdated services: err=" << err;
    }

    return ErrorEnum::eNone;
}
Error ServiceManager::Start()
{
    LOG_DBG() << "Start service manager";

    auto err = mTimer.Create(
        mConfig.mRemoveOutdatedPeriod,
        [this](void*) {
            auto services = MakeUnique<ServiceDataStaticArray>(&mAllocator);

            if (auto err = mStorage->GetAllServices(*services); !err.IsNone()) {
                LOG_ERR() << "Can't get services: err=" << err;

                return;
            }

            if (auto err = RemoveOutdatedServices(*services); !err.IsNone()) {
                LOG_ERR() << "Failed to remove outdated services: err=" << err;
            }
        },
        false);

    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ServiceManager::Stop()
{
    LOG_DBG() << "Stop service manager";

    if (auto err = mTimer.Stop(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ServiceManager::ProcessDesiredServices(const Array<ServiceInfo>& services)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Process desired services";

    assert(mAllocator.FreeSize() == mAllocator.MaxSize());

    if (auto err = mInstallPool.Run(); !err.IsNone()) {
        return err;
    }

    auto desiredServices = MakeUnique<ServiceInfoStaticArray>(&mAllocator, services);

    {
        auto installedServices = MakeUnique<ServiceDataStaticArray>(&mAllocator);

        if (auto err = mStorage->GetAllServices(*installedServices); !err.IsNone()) {
            return err;
        }

        for (const auto& storageService : *installedServices) {
            auto it = desiredServices->FindIf([&storageService](const ServiceInfo& info) {
                return storageService.mServiceID == info.mServiceID && storageService.mVersion == info.mVersion;
            });

            if (it == desiredServices->end()) {
                if (storageService.mState != ServiceStateEnum::eCached) {
                    if (auto err = SetServiceState(storageService, ServiceStateEnum::eCached); !err.IsNone()) {
                        return err;
                    }
                }

                continue;
            }

            if (auto err = SetServiceState(storageService, ServiceStateEnum::eActive); !err.IsNone()) {
                return err;
            }

            desiredServices->Erase(it);
        }
    }

    // Install new services

    for (const auto& service : *desiredServices) {
        auto err = mInstallPool.AddTask([this, &service](void*) {
            if (auto err = InstallService(service); !err.IsNone()) {
                LOG_ERR() << "Can't install service: serviceID=" << service.mServiceID
                          << ", version=" << service.mVersion << ", err=" << err;
            }
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't install service: serviceID=" << service.mServiceID << ", version=" << service.mVersion
                      << ", err=" << err;
        }
    }

    mInstallPool.Wait();
    mInstallPool.Shutdown();

    LOG_DBG() << "Allocator: allocated=" << mAllocator.MaxAllocatedSize() << ", maxSize=" << mAllocator.MaxSize();
#if AOS_CONFIG_THREAD_STACK_USAGE
    LOG_DBG() << "Stack usage: size=" << mInstallPool.GetStackUsage();
#endif

    return ErrorEnum::eNone;
}

Error ServiceManager::GetService(const String& serviceID, ServiceData& service)
{
    auto services = MakeUnique<ServiceDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllServices(*services); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    services->Sort([](const ServiceData& lhs, const ServiceData& rhs) {
        return semver::CompareSemver(lhs.mVersion, rhs.mVersion).mValue == -1;
    });

    for (const auto& storageService : *services) {
        if (storageService.mServiceID == serviceID && storageService.mState != ServiceStateEnum::eCached) {
            service = storageService;

            return ErrorEnum::eNone;
        }
    }

    return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
}

Error ServiceManager::GetAllServices(Array<ServiceData>& services)
{
    return mStorage->GetAllServices(services);
}

Error ServiceManager::GetImageParts(const ServiceData& service, image::ImageParts& imageParts)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get image parts: " << service.mServiceID;

    assert(mAllocator.FreeSize() == mAllocator.MaxSize());

    auto manifest = MakeUnique<oci::ImageManifest>(&mAllocator);

    if (auto err = mOCIManager->LoadImageManifest(FS::JoinPath(service.mImagePath, cImageManifestFile), *manifest);
        !err.IsNone()) {
        return err;
    }

    if (auto err = image::GetImagePartsFromManifest(*manifest, imageParts); !err.IsNone()) {
        return err;
    }

    imageParts.mImageConfigPath   = FS::JoinPath(service.mImagePath, cImageBlobsFolder, imageParts.mImageConfigPath);
    imageParts.mServiceConfigPath = FS::JoinPath(service.mImagePath, cImageBlobsFolder, imageParts.mServiceConfigPath);
    imageParts.mServiceFSPath     = FS::JoinPath(service.mImagePath, cImageBlobsFolder, imageParts.mServiceFSPath);

    return ErrorEnum::eNone;
}

Error ServiceManager::ValidateService(const ServiceData& service)
{
    LOG_DBG() << "Validate service: serviceID=" << service.mServiceID << ", version=" << service.mVersion;

    Error                            err = ErrorEnum::eNone;
    StaticString<oci::cMaxDigestLen> manifestDigest;

    if (Tie(manifestDigest, err) = GetManifestChecksum(service.mImagePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (manifestDigest != service.mManifestDigest) {
        return AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "manifest checksum mismatch"));
    }

    if (err = mImageHandler->ValidateService(service.mImagePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ServiceManager::RemoveItem(const String& id)
{
    StaticArray<StaticString<Max(cServiceIDLen, cVersionLen)>, 2> splitted;

    if (auto err = id.Split(splitted, '_'); !err.IsNone() || splitted.Size() != 2) {
        return AOS_ERROR_WRAP(Error(err, "unexpected service id format"));
    }

    const auto serviceID = splitted[0];
    const auto version   = splitted[1];

    auto services = MakeUnique<ServiceDataStaticArray>(&mAllocator);

    mStorage->GetServiceVersions(serviceID, *services);

    for (const auto& service : *services) {
        if (service.mVersion == version) {
            if (auto err = FS::RemoveAll(service.mImagePath); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }

            if (auto err = RemoveService(service); !err.IsNone()) {
                return AOS_ERROR_WRAP(err);
            }
        }
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error ServiceManager::RemoveDamagedServiceFolders(const Array<ServiceData>& services)
{
    LOG_DBG() << "Remove damaged service folders";

    for (const auto& service : services) {
        if (auto [exists, err] = FS::DirExist(service.mImagePath); err.IsNone() && exists) {
            continue;
        }

        LOG_WRN() << "Service missing: imagePath=" << service.mImagePath;

        if (auto err = RemoveService(service); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (auto serviceDirIterator = FS::DirIterator(mConfig.mServicesDir); serviceDirIterator.Next();) {
        const auto fullPath = FS::JoinPath(mConfig.mServicesDir, serviceDirIterator->mPath);

        if (services.FindIf([&fullPath](const ServiceData& service) { return service.mImagePath == fullPath; })
            != services.end()) {
            continue;
        }

        LOG_WRN() << "Service missing in storage: imagePath=" << fullPath;

        if (auto err = FS::RemoveAll(fullPath); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ServiceManager::RemoveOutdatedServices(const Array<ServiceData>& services)
{
    LOG_DBG() << "Remove outdated services";

    for (const auto& service : services) {
        if (service.mState != ServiceStateEnum::eCached) {
            continue;
        }

        const auto endDate = service.mTimestamp.Add(mConfig.mTTL);

        if (Time::Now() < endDate) {
            continue;
        }

        LOG_DBG() << "Service outdated: serviceID=" << service.mServiceID << ", version=" << service.mVersion;

        if (auto err = RemoveService(service); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto [allocationId, err] = FormatAllocatorItemID(service);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (err = mServiceSpaceAllocator->RestoreOutdatedItem(allocationId); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ServiceManager::RemoveService(const ServiceData& service)
{
    LOG_INF() << "Remove service: serviceID=" << service.mServiceID << ", providerID=" << service.mProviderID
              << ", version=" << service.mVersion << ", path=" << service.mImagePath;

    if (auto err = FS::RemoveAll(service.mImagePath); !err.IsNone()) {
        err = AOS_ERROR_WRAP(err);
    }

    if (service.mState == ServiceStateEnum::eCached) {
        auto [allocationId, err] = FormatAllocatorItemID(service);

        if (!err.IsNone()) {
            LOG_ERR() << "Can't format allocator item ID: serviceID=" << service.mServiceID
                      << ", version=" << service.mVersion << ", err=" << err;
        } else {
            mServiceSpaceAllocator->RestoreOutdatedItem(allocationId);
        }
    }

    mServiceSpaceAllocator->FreeSpace(service.mSize);

    if (auto err = mStorage->RemoveService(service.mServiceID, service.mVersion); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_DBG() << "Service successfully removed: serviceID=" << service.mServiceID << ", version=" << service.mVersion;

    return ErrorEnum::eNone;
}

Error ServiceManager::InstallService(const ServiceInfo& service)
{
    LOG_INF() << "Install service: serviceID=" << service.mServiceID << ", version=" << service.mVersion;

    Error                               err = ErrorEnum::eNone;
    UniquePtr<spaceallocator::SpaceItf> downloadSpace;
    UniquePtr<spaceallocator::SpaceItf> serviceSpace;
    StaticString<cFilePathLen>          archivePath;
    StaticString<cFilePathLen>          servicePath;

    auto cleanupDownload = DeferRelease(&err, [&](Error*) {
        LOG_DBG() << "Cleanup download space";

        ReleaseAllocatedSpace(archivePath, downloadSpace.Get());
    });

    auto releaseServiceSpace = DeferRelease(&err, [&](Error* err) {
        if (!err->IsNone()) {
            ReleaseAllocatedSpace(servicePath, serviceSpace.Get());

            LOG_ERR() << "Can't install service: serviceID=" << service.mServiceID << ", version=" << service.mVersion
                      << ", imagePath=" << servicePath << ", err=" << *err;

            return;
        }

        AcceptAllocatedSpace(serviceSpace.Get());
    });

    Tie(downloadSpace, err) = mDownloadSpaceAllocator->AllocateSpace(service.mSize);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    archivePath = FS::JoinPath(mConfig.mDownloadDir, service.mServiceID);

    if (err = mDownloader->Download(service.mURL, archivePath, downloader::DownloadContentEnum::eService);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (Tie(servicePath, err) = mImageHandler->InstallService(archivePath, mConfig.mServicesDir, service, serviceSpace);
        !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto data = MakeUnique<ServiceData>(&mAllocator);

    data->mServiceID  = service.mServiceID;
    data->mProviderID = service.mProviderID;
    data->mVersion    = service.mVersion;
    data->mImagePath  = servicePath;
    data->mTimestamp  = Time::Now();
    data->mState      = ServiceStateEnum::eActive;
    data->mSize       = serviceSpace->Size();
    data->mGID        = service.mGID;

    if (Tie(data->mManifestDigest, err) = GetManifestChecksum(servicePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mStorage->AddService(*data);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LOG_INF() << "Service successfully installed: serviceID=" << data->mServiceID << ", version=" << data->mVersion
              << ", path=" << data->mImagePath;

    return ErrorEnum::eNone;
}

Error ServiceManager::SetServiceState(const ServiceData& service, ServiceState state)
{
    LOG_DBG() << "Set service state: serviceID=" << service.mServiceID << ", version=" << service.mVersion
              << ", state=" << state;

    auto updatedService = service;

    updatedService.mState     = state;
    updatedService.mTimestamp = Time::Now();

    if (auto err = mStorage->UpdateService(updatedService); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    auto [allocationId, err] = FormatAllocatorItemID(service);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (state == ServiceStateEnum::eCached) {
        if (err = mServiceSpaceAllocator->AddOutdatedItem(allocationId, service.mSize, service.mTimestamp);
            !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        return ErrorEnum::eNone;
    }

    if (err = mServiceSpaceAllocator->RestoreOutdatedItem(allocationId); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<StaticString<ServiceManager::cAllocatorItemLen>> ServiceManager::FormatAllocatorItemID(
    const ServiceData& service)
{
    StaticString<cAllocatorItemLen> id = service.mServiceID;
    id.Append("_").Append(service.mVersion);

    return {id, {}};
}

RetWithError<StaticString<oci::cMaxDigestLen>> ServiceManager::GetManifestChecksum(const String& servicePath)
{
    return mImageHandler->CalculateDigest(FS::JoinPath(servicePath, cImageManifestFile));
}

} // namespace aos::sm::servicemanager
