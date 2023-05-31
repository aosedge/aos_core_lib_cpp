/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SERVICEMANAGER_HPP_
#define AOS_SERVICEMANAGER_HPP_

#include "aos/common/downloader.hpp"
#include "aos/common/ocispec.hpp"
#include "aos/common/tools/allocator.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/tools/thread.hpp"
#include "aos/common/types.hpp"
#include "aos/sm/config.hpp"

namespace aos {
namespace sm {
namespace servicemanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Service manager service data.
 */
struct ServiceData {
    /**
     * Version information.
     */
    VersionInfo mVersionInfo;
    /**
     * Service ID.
     */
    StaticString<cServiceIDLen> mServiceID;
    /**
     * Provider ID.
     */
    StaticString<cProviderIDLen> mProviderID;
    /**
     * Image path.
     */
    StaticString<cFilePathLen> mImagePath;

    /**
     * Compares service data.
     *
     * @param data data to compare.
     * @return bool.
     */
    bool operator==(const ServiceData& data) const { return data.mVersionInfo == mVersionInfo; }

    /**
     * Compares service data.
     *
     * @param data data to compare.
     * @return bool.
     */
    bool operator!=(const ServiceData& data) const { return !operator==(data); }
};

/**
 * Service data static array.
 */
using ServiceDataStaticArray = StaticArray<ServiceData, cMaxNumServices>;

/**
 * Image parts.
 */
struct ImageParts {
    /**
     * Image config path.
     */
    StaticString<cFilePathLen> mImageConfigPath;
    /**
     * Service config path.
     */
    StaticString<cFilePathLen> mServiceConfigPath;
    /**
     * Service root FS path.
     */
    StaticString<cFilePathLen> mServiceFSPath;
};

/**
 * Service manager storage interface.
 */
class StorageItf {
public:
    /**
     * Adds new service to storage.
     *
     * @param service service to add.
     * @return Error.
     */
    virtual Error AddService(const ServiceData& service) = 0;

    /**
     * Returns service data by service ID.
     *
     * @param serviceID service ID.
     * @return  RetWithError<ServiceData>.
     */
    virtual RetWithError<ServiceData> GetService(const String& serviceID) = 0;

    /**
     * Updates previously stored service.
     *
     * @param service service to update.
     * @return Error.
     */
    virtual Error UpdateService(const ServiceData& service) = 0;

    /**
     * Removes previously stored service.
     *
     * @param serviceID service ID to remove.
     * @param aosVersion Aos service version.
     * @return Error.
     */
    virtual Error RemoveService(const String& serviceID, uint64_t aosVersion) = 0;

    /**
     * Returns all stored services.
     *
     * @param services array to return stored services.
     * @return Error.
     */
    virtual Error GetAllServices(Array<ServiceData>& services) = 0;

    /**
     * Destroys storage interface.
     */
    virtual ~StorageItf() = default;
};

/**
 * Service manager interface.
 */
class ServiceManagerItf {
public:
    /**
     * Installs services.
     *
     * @param services to install.
     * @return Error
     */
    virtual Error InstallServices(const Array<ServiceInfo>& services) = 0;

    /**
     * Returns service item by service ID.
     *
     * @param serviceID service ID.
     * @return RetWithError<ServiceItem>.
     */
    virtual RetWithError<ServiceData> GetService(const String& serviceID) = 0;

    /**
     * Returns service image parts.
     *
     * @param service service item.
     * @return RetWithError<ImageParts>.
     */
    virtual RetWithError<ImageParts> GetImageParts(const ServiceData& service) = 0;

    /**
     * Destroys storage interface.
     */
    virtual ~ServiceManagerItf() = default;
};

class ServiceManager : public ServiceManagerItf, private NonCopyable {
public:
    /**
     * Creates service manager.
     */
    ServiceManager() = default;

    /**
     * Destroys service manager.
     */
    ~ServiceManager() { mInstallPool.Shutdown(); }

    /**
     * Initializes service manager.
     *
     * @param ociManager OCI manager instance.
     * @param downloader downloader instance.
     * @param storage storage instance.
     * @return Error.
     */
    Error Init(OCISpecItf& ociManager, DownloaderItf& downloader, StorageItf& storage);

    /**
     * Installs services.
     *
     * @param services to install.
     * @return Error
     */
    Error InstallServices(const Array<ServiceInfo>& services) override;

    /**
     * Returns service item by service ID.
     *
     * @param serviceID service ID.
     * @return RetWithError<ServiceItem>.
     */
    RetWithError<ServiceData> GetService(const String& serviceID) override;

    /**
     * Returns service image parts.
     *
     * @param service service item.
     * @return RetWithError<ImageParts>.
     */
    RetWithError<ImageParts> GetImageParts(const ServiceData& service) override;

private:
    static constexpr auto cNumInstallThreads = AOS_CONFIG_SERVICEMANAGER_NUM_COOPERATE_INSTALLS;
    static constexpr auto cServicesDir = AOS_CONFIG_SERVICEMANAGER_SERVICES_DIR;
    static constexpr auto cImageManifestFile = "manifest.json";
    static constexpr auto cImageBlobsFolder = "blobs";

    Error                                    RemoveService(const ServiceData& service);
    Error                                    InstallService(const ServiceInfo& service);
    RetWithError<StaticString<cFilePathLen>> DigestToPath(const String& imagePath, const String& digest);

    OCISpecItf*    mOCIManager {};
    DownloaderItf* mDownloader {};
    StorageItf*    mStorage {};

    Mutex mMutex;
    StaticAllocator<Max(sizeof(ServiceDataStaticArray), sizeof(oci::ImageManifest) + sizeof(oci::ContentDescriptor))>
                                                    mAllocator;
    ThreadPool<cNumInstallThreads, cMaxNumServices> mInstallPool;
};

/** @}*/

} // namespace servicemanager
} // namespace sm
} // namespace aos

#endif
