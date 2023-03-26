/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SERVICEMANAGER_HPP_
#define AOS_SERVICEMANAGER_HPP_

#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/types.hpp"

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
    virtual RetWithError<ServiceData> GetService(const String serviceID) = 0;

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

/** @}*/

} // namespace servicemanager
} // namespace sm
} // namespace aos

#endif
