/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/servicemanager.hpp"
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
    (void)services;

    return ErrorEnum::eNone;
}

RetWithError<ServiceData> ServiceManager::GetService(const String& serviceID)
{
    (void)serviceID;

    return ServiceData {};
}

RetWithError<ImageParts> ServiceManager::GetImageParts(const ServiceData& service)
{
    (void)service;

    return ImageParts {};
}

} // namespace servicemanager
} // namespace sm
} // namespace aos
