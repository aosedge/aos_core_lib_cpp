/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/servicemanager.hpp"
#include "aos/common/tools/memory.hpp"

#include "log.hpp"

namespace aos {
namespace sm {
namespace servicemanager {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ServiceManager::Init(DownloaderItf& downloader, StorageItf& storage)
{
    LOG_DBG() << "Initialize service manager";

    mDownloader = &downloader;
    mStorage = &storage;

    return ErrorEnum::eNone;
}

Error ServiceManager::InstallServices(const Array<ServiceInfo>& services)
{
    mInstallPool.Wait();

    LOG_DBG() << "Install services";

    assert(mAllocator.FreeSize() == mAllocator.MaxSize());

    auto err = mInstallPool.Run();
    if (!err.IsNone()) {
        return err;
    }

    auto installedServices = UniquePtr<Array<ServiceData>>(&mAllocator, new (&mAllocator) ServiceDataStaticArray());

    err = mStorage->GetAllServices(*installedServices);
    if (!err.IsNone()) {
        return err;
    }

    // Remove unneeded or changed services

    for (const auto& service : *installedServices) {
        if (!services
                 .Find([&service](const ServiceInfo& info) {
                     return service.mServiceID == info.mServiceID && service.mVersionInfo == info.mVersionInfo;
                 })
                 .mError.IsNone()) {
            err = mInstallPool.AddTask([this, &service](void*) {
                auto err = RemoveService(service);
                if (!err.IsNone()) {
                    LOG_ERR() << "Can't remove service " << service.mServiceID << ": " << err;
                }
            });
            if (!err.IsNone()) {
                LOG_ERR() << "Can't remove service " << service.mServiceID << ": " << err;
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
                 ->Find([&info](const ServiceData& service) { return info.mServiceID == service.mServiceID; })
                 .mError.IsNone()) {
            err = mInstallPool.AddTask([this, &info](void*) {
                auto err = InstallService(info);
                if (!err.IsNone()) {
                    LOG_ERR() << "Can't install service " << info.mServiceID << ": " << err;
                }
            });
            if (!err.IsNone()) {
                LOG_ERR() << "Can't install service " << info.mServiceID << ": " << err;
            }
        }
    }

    mInstallPool.Wait();
    mInstallPool.Shutdown();

    return ErrorEnum::eNone;
}

RetWithError<ServiceData> ServiceManager::GetService(const String& serviceID)
{
    return mStorage->GetService(serviceID);
}

RetWithError<ImageParts> ServiceManager::GetImageParts(const ServiceData& service)
{
    return ImageParts {FS::JoinPath(service.mImagePath, cImageConfigFile),
        FS::JoinPath(service.mImagePath, cServiceConfigFile), FS::JoinPath(service.mImagePath, cServiceRootFS)};
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error ServiceManager::RemoveService(const ServiceData& service)
{
    LOG_INF() << "Remove service " << service.mServiceID << ", path: " << service.mImagePath;

    Error removeErr;

    auto err = FS::RemoveDir(service.mImagePath, true);
    if (!err.IsNone() && removeErr.IsNone()) {
        removeErr = err;
    }

    err = mStorage->RemoveService(service.mServiceID, service.mVersionInfo.mAosVersion);
    if (!err.IsNone() && removeErr.IsNone()) {
        removeErr = err;
    }

    return removeErr;
}

Error ServiceManager::InstallService(const ServiceInfo& service)
{
    ServiceData data {{service.mVersionInfo}, service.mServiceID, service.mProviderID,
        FS::JoinPath(cServicesDir, service.mServiceID)};

    LOG_INF() << "Install service " << service.mServiceID << ", path: " << data.mImagePath;

    auto err = FS::ClearDir(data.mImagePath);
    if (!err.IsNone()) {
        return err;
    }

    err = mDownloader->Download(service.mURL, data.mImagePath, DownloadContentEnum::eService);
    if (!err.IsNone()) {
        return err;
    }

    err = mStorage->AddService(data);
    if (!err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

} // namespace servicemanager
} // namespace sm
} // namespace aos
