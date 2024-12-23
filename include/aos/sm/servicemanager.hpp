/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SERVICEMANAGER_HPP_
#define AOS_SERVICEMANAGER_HPP_

#include "aos/common/downloader/downloader.hpp"
#include "aos/common/ocispec/ocispec.hpp"
#include "aos/common/spaceallocator/spaceallocator.hpp"
#include "aos/common/tools/allocator.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/tools/thread.hpp"
#include "aos/common/tools/timer.hpp"
#include "aos/common/types.hpp"
#include "aos/sm/config.hpp"
#include "aos/sm/image/imagehandler.hpp"
#include "aos/sm/image/imageparts.hpp"

namespace aos::sm::servicemanager {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Service state type.
 */
class ServiceStateType {
public:
    enum class Enum {
        eActive,
        eCached,
        ePending,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStateStrings[] = {
            "active",
            "cached",
            "pending",
        };

        return Array<const char* const>(sStateStrings, ArraySize(sStateStrings));
    };
};

using ServiceStateEnum = ServiceStateType::Enum;
using ServiceState     = EnumStringer<ServiceStateType>;

/**
 * Service manager service data.
 */
struct ServiceData {
    /**
     * Service ID.
     */
    StaticString<cServiceIDLen> mServiceID;

    /**
     * Provider ID.
     */
    StaticString<cProviderIDLen> mProviderID;

    /**
     * Service version.
     */
    StaticString<cVersionLen> mVersion;

    /**
     * Image path.
     */
    StaticString<cFilePathLen> mImagePath;

    /**
     * Manifest digest.
     */
    StaticString<oci::cMaxDigestLen> mManifestDigest;

    /**
     * Timestamp.
     */
    Time mTimestamp;

    /**
     * State.
     */
    ServiceState mState;

    /**
     * Service size.
     */
    uint64_t mSize;

    /**
     * Service group identifier.
     */
    uint32_t mGID;

    /**
     * Compares service data.
     *
     * @param data data to compare.
     * @return bool.
     */
    bool operator==(const ServiceData& data) const
    {
        return mServiceID == data.mServiceID && mProviderID == data.mProviderID && mVersion == data.mVersion
            && mImagePath == data.mImagePath && data.mManifestDigest == mManifestDigest && mState == data.mState
            && mSize == data.mSize && mGID == data.mGID;
    }

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
     * Returns service versions by service ID.
     *
     * @param serviceID service ID.
     * @param services[out] service version for the given id.
     * @return Error.
     */
    virtual Error GetServiceVersions(const String& serviceID, Array<sm::servicemanager::ServiceData>& services) = 0;

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
     * @param version Aos service version.
     * @return Error.
     */
    virtual Error RemoveService(const String& serviceID, const String& version) = 0;

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
     * Processes desired services.
     *
     * @param services desired services.
     * @return Error.
     */
    virtual Error ProcessDesiredServices(const Array<ServiceInfo>& services) = 0;

    /**
     * Returns service item by service ID.
     *
     * @param serviceID service ID.
     * @param service[out] service item.
     * @return Error.
     */
    virtual Error GetService(const String& serviceID, ServiceData& service) = 0;

    /**
     * Returns all installed services.
     *
     * @param services array to return installed services.
     * @return Error.
     */
    virtual Error GetAllServices(Array<ServiceData>& services) = 0;

    /**
     * Returns service image parts.
     *
     * @param service service item.
     * @return RetWithError<image::ImageParts>.
     */
    virtual RetWithError<image::ImageParts> GetImageParts(const ServiceData& service) = 0;

    /**
     * Validates service.
     *
     * @param service service to validate.
     * @return Error.
     */
    virtual Error ValidateService(const ServiceData& service) = 0;

    /**
     * Destroys storage interface.
     */
    virtual ~ServiceManagerItf() = default;
};

/**
 * Service manager configuration.
 */
struct Config {
    StaticString<cFilePathLen> mServicesDir;
    StaticString<cFilePathLen> mDownloadDir;
    Duration                   mTTL;
};

class ServiceManager : public ServiceManagerItf, public spaceallocator::ItemRemoverItf, private NonCopyable {
public:
    /**
     * Creates service manager.
     */
    ServiceManager() = default;

    /**
     * Destroys service manager.
     */
    ~ServiceManager();

    /**
     * Initializes service manager.
     *
     * @param config service manager configuration.
     * @param ociManager OCI manager instance.
     * @param downloader downloader instance.
     * @param storage storage instance.
     * @param serviceSpaceAllocator service space allocator.
     * @param downloadSpaceAllocator download space allocator.
     * @param imageHandler image handler.
     * @return Error.
     */
    Error Init(const Config& config, oci::OCISpecItf& ociManager, downloader::DownloaderItf& downloader,
        StorageItf& storage, spaceallocator::SpaceAllocatorItf& serviceSpaceAllocator,
        spaceallocator::SpaceAllocatorItf& downloadSpaceAllocator, imagehandler::ImageHandlerItf& imageHandler);

    /**
     * Starts service manager.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops service manager.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Process desired services.
     *
     * @param services desired services.
     * @return Error.
     */
    Error ProcessDesiredServices(const Array<ServiceInfo>& services) override;

    /**
     * Returns service item by service ID.
     *
     * @param serviceID service ID.
     * @param service[out] service item.
     * @return Error.
     */
    Error GetService(const String& serviceID, ServiceData& service) override;

    /**
     * Returns all installed services.
     *
     * @param services array to return installed services.
     * @return Error.
     */
    Error GetAllServices(Array<ServiceData>& services) override;

    /**
     * Returns service image parts.
     *
     * @param service service item.
     * @return RetWithError<image::ImageParts>.
     */
    RetWithError<image::ImageParts> GetImageParts(const ServiceData& service) override;

    /**
     * Validates service.
     *
     * @param service service to validate.
     * @return Error.
     */
    Error ValidateService(const ServiceData& service) override;

    /**
     * Removes item.
     *
     * @param id item id.
     * @return Error.
     */
    Error RemoveItem(const String& id) override;

private:
    static constexpr auto cNumInstallThreads = AOS_CONFIG_SERVICEMANAGER_NUM_COOPERATE_INSTALLS;
    static constexpr auto cImageManifestFile = "manifest.json";
    static constexpr auto cImageBlobsFolder  = "blobs";
    static constexpr auto cAllocatorItemLen  = cServiceIDLen + cVersionLen + 1;

    Error                                          RemoveDamagedServiceFolders(const Array<ServiceData>& services);
    Error                                          RemoveOutdatedServices(const Array<ServiceData>& services);
    Error                                          RemoveService(const ServiceData& service);
    Error                                          InstallService(const ServiceInfo& service);
    Error                                          SetServiceState(const ServiceData& service, ServiceState state);
    RetWithError<StaticString<cFilePathLen>>       DigestToPath(const String& imagePath, const String& digest);
    RetWithError<StaticString<cAllocatorItemLen>>  FormatAllocatorItemID(const ServiceData& service);
    RetWithError<StaticString<oci::cMaxDigestLen>> GetManifestChecksum(const String& servicePath);

    Config                             mConfig {};
    oci::OCISpecItf*                   mOCIManager             = nullptr;
    downloader::DownloaderItf*         mDownloader             = nullptr;
    StorageItf*                        mStorage                = nullptr;
    spaceallocator::SpaceAllocatorItf* mServiceSpaceAllocator  = nullptr;
    spaceallocator::SpaceAllocatorItf* mDownloadSpaceAllocator = nullptr;
    imagehandler::ImageHandlerItf*     mImageHandler           = nullptr;
    Timer                              mTimer;
    Mutex                              mMutex;

    StaticAllocator<2 * sizeof(ServiceDataStaticArray) + sizeof(ServiceInfoStaticArray)
            + cNumInstallThreads * sizeof(oci::ImageManifest),
        cNumInstallThreads * 3>
        mAllocator;

    ThreadPool<cNumInstallThreads, cMaxNumServices> mInstallPool;
};

/** @}*/

} // namespace aos::sm::servicemanager

#endif
