/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/servicemanager.hpp"
#include "aos/common/tools/memory.hpp"

#include "log.hpp"

namespace aos::sm::servicemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ServiceManager::Init(oci::OCISpecItf& ociManager, downloader::DownloaderItf& downloader, StorageItf& storage)
{
    LOG_DBG() << "Initialize service manager";

    mOCIManager = &ociManager;
    mDownloader = &downloader;
    mStorage    = &storage;

    return ErrorEnum::eNone;
}

Error ServiceManager::InstallServices(const Array<ServiceInfo>& services)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Install services";

    assert(mAllocator.FreeSize() == mAllocator.MaxSize());

    auto err = mInstallPool.Run();
    if (!err.IsNone()) {
        return err;
    }

    auto installedServices = MakeUnique<ServiceDataStaticArray>(&mAllocator);

    err = mStorage->GetAllServices(*installedServices);
    if (!err.IsNone()) {
        return err;
    }

    // Remove unneeded or changed services

    for (const auto& service : *installedServices) {
        if (!services
                 .FindIf([&service](const ServiceInfo& info) {
                     return service.mServiceID == info.mServiceID && service.mVersion == info.mVersion;
                 })
                 .mError.IsNone()) {
            err = mInstallPool.AddTask([this, &service](void*) {
                auto err = RemoveService(service);
                if (!err.IsNone()) {
                    LOG_ERR() << "Can't remove service: serviceID=" << service.mServiceID << ", err=" << err;
                }
            });
            if (!err.IsNone()) {
                LOG_ERR() << "Can't remove service: serviceID=" << service.mServiceID << ", err=" << err;
            }
        }
    }

    mInstallPool.Wait();

    // Install new services

    installedServices->Clear();

    err = mStorage->GetAllServices(*installedServices);
    if (!err.IsNone()) {
        return err;
    }

    for (const auto& info : services) {
        if (!installedServices
                 ->FindIf([&info](const ServiceData& service) { return info.mServiceID == service.mServiceID; })
                 .mError.IsNone()) {
            err = mInstallPool.AddTask([this, &info](void*) {
                auto err = InstallService(info);
                if (!err.IsNone()) {
                    LOG_ERR() << "Can't install service: serviceID=" << info.mServiceID << ", err=" << err;
                }
            });
            if (!err.IsNone()) {
                LOG_ERR() << "Can't install service: serviceID=" << info.mServiceID << ", err=" << err;
            }
        }
    }

    mInstallPool.Wait();
    mInstallPool.Shutdown();

    return ErrorEnum::eNone;
}

RetWithError<ServiceData> ServiceManager::GetService(const String& serviceID)
{
    auto services = MakeUnique<ServiceDataStaticArray>(&mAllocator);

    if (auto err = mStorage->GetAllServices(*services); !err.IsNone()) {
        return {{}, err};
    }

    services->Sort([](const ServiceData& lhs, const ServiceData& rhs) { return lhs.mVersion < rhs.mVersion; });

    for (const auto& service : *services) {
        if (service.mServiceID == serviceID && service.mState != ServiceStateEnum::eCached) {
            return {service, {}};
        }
    }

    return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotFound)};
}

Error ServiceManager::GetAllServices(Array<ServiceData>& services)
{
    return mStorage->GetAllServices(services);
}

RetWithError<ImageParts> ServiceManager::GetImageParts(const ServiceData& service)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get image parts: " << service.mServiceID;

    assert(mAllocator.FreeSize() == mAllocator.MaxSize());

    auto manifest   = MakeUnique<oci::ImageManifest>(&mAllocator);
    auto aosService = MakeUnique<oci::ContentDescriptor>(&mAllocator);

    manifest->mAosService = aosService.Get();

    auto err = mOCIManager->LoadImageManifest(FS::JoinPath(service.mImagePath, cImageManifestFile), *manifest);
    if (!err.IsNone()) {
        return {{}, err};
    }

    auto imageConfig = DigestToPath(service.mImagePath, manifest->mConfig.mDigest);
    if (!imageConfig.mError.IsNone()) {
        return {{}, imageConfig.mError};
    }

    auto serviceConfig = DigestToPath(service.mImagePath, manifest->mAosService->mDigest);
    if (!serviceConfig.mError.IsNone()) {
        return {{}, serviceConfig.mError};
    }

    if (!manifest->mLayers) {
        return {{}, AOS_ERROR_WRAP(ErrorEnum::eNotFound)};
    }

    auto serviceFS = DigestToPath(service.mImagePath, manifest->mLayers[0].mDigest);
    if (!serviceFS.mError.IsNone()) {
        return {{}, serviceFS.mError};
    }

    return ImageParts {imageConfig.mValue, serviceConfig.mValue, serviceFS.mValue};
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error ServiceManager::RemoveService(const ServiceData& service)
{
    LOG_INF() << "Remove service: serviceID=" << service.mServiceID << ", providerID=" << service.mProviderID
              << ", version=" << service.mVersion << ", path=" << service.mImagePath;

    Error removeErr;

    auto err = FS::RemoveAll(service.mImagePath);
    if (!err.IsNone() && removeErr.IsNone()) {
        removeErr = AOS_ERROR_WRAP(err);
    }

    err = mStorage->RemoveService(service.mServiceID, service.mVersion);
    if (!err.IsNone() && removeErr.IsNone()) {
        removeErr = AOS_ERROR_WRAP(err);
    }

    return removeErr;
}

Error ServiceManager::InstallService(const ServiceInfo& service)
{
    ServiceData data {service.mServiceID, service.mProviderID, service.mVersion,
        FS::JoinPath(cServicesDir, service.mServiceID), "", Time::Now(), ServiceStateEnum::eActive, service.mSize,
        service.mGID};

    LOG_INF() << "Install service: serviceID=" << data.mServiceID << ", providerID=" << data.mProviderID
              << ", version=" << data.mVersion << ", path=" << data.mImagePath;

    auto err = FS::ClearDir(data.mImagePath);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mDownloader->Download(service.mURL, data.mImagePath, downloader::DownloadContentEnum::eService);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    err = mStorage->AddService(data);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<StaticString<cFilePathLen>> ServiceManager::DigestToPath(const String& imagePath, const String& digest)
{
    StaticArray<const StaticString<oci::cMaxDigestLen>, 2> digestList;

    auto err = digest.Split(digestList, ':');
    if (!err.IsNone()) {
        return {"", AOS_ERROR_WRAP(err)};
    }

    return FS::JoinPath(imagePath, cImageBlobsFolder, digestList[0], digestList[1]);
}

} // namespace aos::sm::servicemanager
