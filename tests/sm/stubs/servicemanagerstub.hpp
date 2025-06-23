/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SERVICEMANAGER_STUB_HPP_
#define AOS_SERVICEMANAGER_STUB_HPP_

#include <algorithm>
#include <mutex>

#include "aos/sm/servicemanager.hpp"

namespace aos::sm::servicemanager {

/**
 * Storage stub.
 */
class StorageStub : public StorageItf {
public:
    /**
     * Adds new service to storage.
     *
     * @param service service to add.
     * @return Error.
     */
    Error AddService(const ServiceData& service) override
    {
        std::lock_guard lock {mMutex};

        if (std::find_if(mServices.begin(), mServices.end(),
                [&service](const ServiceData& data) {
                    return service.mServiceID == data.mServiceID && service.mVersion == data.mVersion;
                })
            != mServices.end()) {
            return ErrorEnum::eAlreadyExist;
        }

        mServices.push_back(service);

        return ErrorEnum::eNone;
    }

    /**
     * Returns service versions by service ID.
     *
     * @param serviceID service ID.
     * @param services[out] service version for the given id.
     * @return Error.
     */
    Error GetServiceVersions(const String& serviceID, Array<sm::servicemanager::ServiceData>& services) override
    {
        std::lock_guard lock {mMutex};

        for (const auto& service : mServices) {
            if (service.mServiceID == serviceID) {
                if (auto err = services.PushBack(service); !err.IsNone()) {
                    return AOS_ERROR_WRAP(err);
                }
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Updates previously stored service.
     *
     * @param service service to update.
     * @return Error.
     */
    Error UpdateService(const ServiceData& service) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mServices.begin(), mServices.end(), [&service](const ServiceData& data) {
            return service.mServiceID == data.mServiceID && service.mVersion == data.mVersion;
        });

        if (it == mServices.end()) {
            return ErrorEnum::eNotFound;
        }

        *it = service;

        return ErrorEnum::eNone;
    }

    /**
     * Removes previously stored service.
     *
     * @param serviceID service ID to remove.
     * @param version Aos service version.
     * @return Error.
     */
    Error RemoveService(const String& serviceID, const String& version) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mServices.begin(), mServices.end(), [&serviceID, &version](const ServiceData& data) {
            return serviceID == data.mServiceID && version == data.mVersion;
        });
        if (it == mServices.end()) {
            return ErrorEnum::eNotFound;
        }

        mServices.erase(it);

        return ErrorEnum::eNone;
    }

    /**
     * Returns all stored services.
     *
     * @param services array to return stored services.
     * @return Error.
     */
    Error GetAllServices(Array<ServiceData>& services) override
    {
        std::lock_guard lock {mMutex};

        for (const auto& service : mServices) {
            auto err = services.PushBack(service);
            if (!err.IsNone()) {
                return err;
            }
        }

        return ErrorEnum::eNone;
    }

    /**
     * Returns num entries.
     *
     * @return size_t.
     */
    size_t Size() const
    {
        std::lock_guard lock {mMutex};

        return mServices.size();
    }

private:
    std::vector<ServiceData> mServices;
    mutable std::mutex       mMutex;
};

/**
 * Service manager stub.
 */
class ServiceManagerStub : public ServiceManagerItf {
public:
    /**
     * Processes desired services.
     *
     * @param services desired services.
     * @param serviceStatuses[out] service statuses.
     * @return Error.
     */
    Error ProcessDesiredServices(const Array<ServiceInfo>& services, Array<ServiceStatus>& serviceStatuses) override
    {
        (void)serviceStatuses;

        std::lock_guard lock {mMutex};

        mServicesData.clear();

        std::transform(
            services.begin(), services.end(), std::back_inserter(mServicesData), [](const ServiceInfo& service) {
                return ServiceData {service.mServiceID, service.mProviderID, service.mVersion,
                    fs::JoinPath("/aos/services/", service.mServiceID), "", Time::Now(), ServiceStateEnum::eActive, 0,
                    0};
            });

        return ErrorEnum::eNone;
    }

    /**
     * Returns service item by service ID.
     *
     * @param serviceID service ID.
     * @param service[out] service item.
     * @return Error.
     */
    Error GetService(const String& serviceID, servicemanager::ServiceData& service) override
    {
        std::lock_guard lock {mMutex};

        auto it = std::find_if(mServicesData.begin(), mServicesData.end(),
            [&serviceID](const ServiceData& storageService) { return storageService.mServiceID == serviceID; });
        if (it == mServicesData.end()) {
            return ErrorEnum::eNotFound;
        }

        service = *it;

        return ErrorEnum::eNone;
    }

    /**
     * Returns all installed services.
     *
     * @param services array to return installed services.
     * @return Error.
     */
    Error GetAllServices(Array<ServiceData>& services) override
    {
        std::lock_guard lock {mMutex};

        for (const auto& service : mServicesData) {
            services.PushBack(service);
        }

        return ErrorEnum::eNone;
    }

    /**
     * Returns service image parts.
     *
     * @param service service item.
     * @param imageParts[out] image parts.
     * @return Error.
     */
    Error GetImageParts(const ServiceData& service, image::ImageParts& imageParts) override
    {
        imageParts.mImageConfigPath   = fs::JoinPath(service.mImagePath, "image.json");
        imageParts.mServiceConfigPath = fs::JoinPath(service.mImagePath, "service.json");
        imageParts.mServiceFSPath     = service.mImagePath;

        return ErrorEnum::eNone;
    }

    /**
     * Validates service.
     *
     * @param service service to validate.
     * @return Error.
     */
    Error ValidateService(const ServiceData& service) override
    {
        (void)service;

        return ErrorEnum::eNone;
    }

    /**
     * Removes service.
     *
     * @param service service to remove.
     * @return Error.
     */
    Error RemoveService(const ServiceData& service) override
    {
        (void)service;

        return ErrorEnum::eNone;
    }

private:
    std::mutex               mMutex;
    std::vector<ServiceData> mServicesData;
};

} // namespace aos::sm::servicemanager

#endif
