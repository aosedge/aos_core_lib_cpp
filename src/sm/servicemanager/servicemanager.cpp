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
        fs::RemoveAll(path);
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

    if (auto err = fs::MakeDirAll(mConfig.mServicesDir); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = fs::ClearDir(mConfig.mDownloadDir); !err.IsNone()) {
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

    auto err = mTimer.Start(
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

Error ServiceManager::ProcessDesiredServices(const Array<ServiceInfo>& services, Array<ServiceStatus>& serviceStatuses)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Process desired services";

    assert(mAllocator.FreeSize() == mAllocator.MaxSize());

    if (auto err = mInstallPool.Run(); !err.IsNone()) {
        return err;
    }

    auto desiredServices = MakeUnique<ServiceInfoStaticArray>(&mAllocator, services);

    if (auto err = ProcessAlreadyInstalledServices(*desiredServices, serviceStatuses); !err.IsNone()) {
        return err;
    }

    if (auto err = PrepareSpaceForServices(desiredServices->Size()); !err.IsNone()) {
        return err;
    }

    if (auto err = InstallServices(*desiredServices, serviceStatuses); !err.IsNone()) {
        return err;
    }

    for (const auto& service : services) {
        if (auto err = TruncServiceVersions(service.mServiceID, cNumServiceVersions); !err.IsNone()) {
            LOG_WRN() << "Can't truncate service versions: serviceID=" << service.mServiceID << ", err=" << err;
        }
    }

    LOG_DBG() << "Allocator: allocated=" << mAllocator.MaxAllocatedSize() << ", maxSize=" << mAllocator.MaxSize();
#if AOS_CONFIG_THREAD_STACK_USAGE
    LOG_DBG() << "Stack usage: size=" << mInstallPool.GetStackUsage();
#endif

    return ErrorEnum::eNone;
}

Error ServiceManager::GetService(const String& serviceID, ServiceData& service)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get service: serviceID=" << serviceID;

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

    if (auto err = mOCIManager->LoadImageManifest(fs::JoinPath(service.mImagePath, cImageManifestFile), *manifest);
        !err.IsNone()) {
        return err;
    }

    if (auto err = image::GetImagePartsFromManifest(*manifest, imageParts); !err.IsNone()) {
        return err;
    }

    imageParts.mImageConfigPath   = fs::JoinPath(service.mImagePath, cImageBlobsFolder, imageParts.mImageConfigPath);
    imageParts.mServiceConfigPath = fs::JoinPath(service.mImagePath, cImageBlobsFolder, imageParts.mServiceConfigPath);
    imageParts.mServiceFSPath     = fs::JoinPath(service.mImagePath, cImageBlobsFolder, imageParts.mServiceFSPath);

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
        return AOS_ERROR_WRAP(Error(ErrorEnum::eInvalidChecksum, "manifest checksum mismatch"));
    }

    if (err = mImageHandler->ValidateService(service.mImagePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error ServiceManager::RemoveService(const ServiceData& service)
{
    if (auto err = RemoveServiceFromSystem(service); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
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

    auto it = services->FindIf([&version](const ServiceData& service) { return service.mVersion == version; });
    if (it == services->end()) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    if (auto err = RemoveServiceFromSystem(*it); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error ServiceManager::ProcessAlreadyInstalledServices(
    Array<ServiceInfo>& desiredServices, Array<ServiceStatus>& serviceStatuses)
{
    auto installedServices = MakeUnique<ServiceDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllServices(*installedServices); !err.IsNone()) {
        return err;
    }

    for (const auto& storageService : *installedServices) {
        auto it = desiredServices.FindIf([&storageService](const ServiceInfo& info) {
            return storageService.mServiceID == info.mServiceID && storageService.mVersion == info.mVersion;
        });

        if (it == desiredServices.end()) {
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

        if (auto err = serviceStatuses.EmplaceBack(
                storageService.mServiceID, storageService.mVersion, ItemStatusEnum::eInstalled);
            !err.IsNone()) {
            return err;
        }

        desiredServices.Erase(it);

        if (auto err = ValidateService(storageService); !err.IsNone()) {
            serviceStatuses.Back().SetError(err);
        }
    }

    return ErrorEnum::eNone;
}

Error ServiceManager::InstallServices(const Array<ServiceInfo>& services, Array<ServiceStatus>& serviceStatuses)
{
    for (const auto& service : services) {
        auto err = serviceStatuses.EmplaceBack(service.mServiceID, service.mVersion, ItemStatusEnum::eInstalling);
        if (!err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        auto& status = serviceStatuses.Back();

        err = mInstallPool.AddTask([this, &service, &status](void*) {
            if (auto err = InstallService(service); !err.IsNone()) {
                LOG_ERR() << "Can't install service: serviceID=" << service.mServiceID
                          << ", version=" << service.mVersion << ", err=" << err;

                status.SetError(err);

                return;
            }

            status.mStatus = ItemStatusEnum::eInstalled;
        });
        if (!err.IsNone()) {
            LOG_ERR() << "Can't install service: serviceID=" << service.mServiceID << ", version=" << service.mVersion
                      << ", err=" << err;

            status.SetError(err);
        }
    }

    mInstallPool.Wait();
    mInstallPool.Shutdown();

    return ErrorEnum::eNone;
}

Error ServiceManager::PrepareSpaceForServices(size_t desiredServicesNum)
{
    auto storedServices = MakeUnique<ServiceDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllServices(*storedServices); !err.IsNone()) {
        return err;
    }

    storedServices->Sort([](const ServiceData& lhs, const ServiceData& rhs) {
        return (lhs.mServiceID < rhs.mServiceID)
            || (lhs.mServiceID == rhs.mServiceID && semver::CompareSemver(lhs.mVersion, rhs.mVersion).mValue == -1);
    });

    while (storedServices->Size() + desiredServicesNum > cMaxNumServices) {
        auto it = storedServices->FindIf(
            [](const ServiceData& service) { return service.mState == ServiceStateEnum::eCached; });

        if (it == storedServices->end()) {
            return AOS_ERROR_WRAP(Error(ErrorEnum::eFailed, "can't find cached service to remove"));
        }

        if (auto err = RemoveServiceFromSystem(*it); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        storedServices->Erase(it);
    }

    return ErrorEnum::eNone;
}

Error ServiceManager::TruncServiceVersions(const String& serviceID, size_t threshold)
{
    auto storageServices = MakeUnique<StaticArray<ServiceData, cNumServiceVersions + 1>>(&mAllocator);

    if (auto err = mStorage->GetServiceVersions(serviceID, *storageServices);
        !err.IsNone() && !err.Is(ErrorEnum::eNotFound)) {
        return AOS_ERROR_WRAP(err);
    }

    if (storageServices->Size() <= threshold) {
        return ErrorEnum::eNone;
    }

    LOG_DBG() << "Truncate service versions: serviceID=" << serviceID << ", versions=" << storageServices->Size()
              << ", threshold=" << threshold;

    storageServices->Sort([](const ServiceData& lhs, const ServiceData& rhs) {
        return semver::CompareSemver(lhs.mVersion, rhs.mVersion).mValue == -1;
    });

    size_t removedVersions = 0;

    for (const auto& service : *storageServices) {
        if (service.mState == ServiceStateEnum::eActive) {
            continue;
        }

        if (auto err = RemoveService(service); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }

        if (++removedVersions == storageServices->Size() - threshold) {
            break;
        }
    }

    return ErrorEnum::eNone;
}

Error ServiceManager::RemoveDamagedServiceFolders(const Array<ServiceData>& services)
{
    LOG_DBG() << "Remove damaged service folders";

    for (const auto& service : services) {
        if (auto [exists, err] = fs::DirExist(service.mImagePath); err.IsNone() && exists) {
            continue;
        }

        LOG_WRN() << "Service missing: imagePath=" << service.mImagePath;

        if (auto err = RemoveServiceFromSystem(service); !err.IsNone()) {
            return AOS_ERROR_WRAP(err);
        }
    }

    for (auto serviceDirIterator = fs::DirIterator(mConfig.mServicesDir); serviceDirIterator.Next();) {
        const auto fullPath = fs::JoinPath(mConfig.mServicesDir, serviceDirIterator->mPath);

        if (services.FindIf([&fullPath](const ServiceData& service) { return service.mImagePath == fullPath; })
            != services.end()) {
            continue;
        }

        LOG_WRN() << "Service missing in storage: imagePath=" << fullPath;

        if (auto err = fs::RemoveAll(fullPath); !err.IsNone()) {
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

        if (auto err = RemoveServiceFromSystem(service); !err.IsNone()) {
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

Error ServiceManager::RemoveServiceFromSystem(const ServiceData& service)
{
    LOG_DBG() << "Remove service: serviceID=" << service.mServiceID << ", version=" << service.mVersion
              << ", path=" << service.mImagePath;

    if (auto err = fs::RemoveAll(service.mImagePath); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

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

    archivePath = fs::JoinPath(mConfig.mDownloadDir, service.mServiceID);

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
    return mImageHandler->CalculateDigest(fs::JoinPath(servicePath, cImageManifestFile));
}

} // namespace aos::sm::servicemanager
