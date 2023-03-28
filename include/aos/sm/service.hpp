
/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SERVICE_HPP_
#define AOS_SERVICE_HPP_

#include "aos/common/ocispec.hpp"
#include "aos/sm/servicemanager.hpp"

namespace aos {
namespace sm {
namespace launcher {

/**
 * Launcher service.
 */
class Service {
public:
    /**
     * Creates service.
     *
     * @param data service data.
     */
    explicit Service(const servicemanager::ServiceData& data, servicemanager::ServiceManagerItf& serviceManager,
        OCISpecItf& ociManager);

    /**
     * Loads specs from loader.
     *
     * @param loader loader.
     * @return Error.
     */
    Error LoadSpecs();

    /**
     * Returns service data.
     *
     * @return const servicemanager::ServiceData&.
     */
    const servicemanager::ServiceData& Data() const { return mData; };

    /**
     * Return image spec.
     *
     * @return RetWithError<const oci::ImageSpec&>.
     */
    RetWithError<const oci::ImageSpec&> ImageSpec() const
    {
        if (!mSpecErr.IsNone()) {
            return {mImageSpec, mSpecErr};
        }

        return mImageSpec;
    };

    /**
     * Returns service FS path.
     *
     * @return RetWithError<const String&>.
     */
    RetWithError<const String&> ServiceFSPath() const
    {
        if (!mSpecErr.IsNone()) {
            return {mServiceFSPath, mSpecErr};
        }

        return mServiceFSPath;
    };

    /**
     * Compares services.
     *
     * @param service service to compare.
     * @return bool.
     */
    bool operator==(const Service& service) const { return mData == service.mData; }

    /**
     * Compares services.
     *
     * @param service service to compare.
     * @return bool.
     */
    bool operator!=(const Service& service) const { return !operator==(service); }

    /**
     * Compares service with service data.
     *
     * @param data data to compare.
     * @return bool.
     */
    bool operator==(const servicemanager::ServiceData& data) const { return mData == data; }

    /**
     * Compares service with service data.
     *
     * @param data data to compare.
     * @return bool.
     */
    bool operator!=(const servicemanager::ServiceData& data) const { return !operator==(data); }

    /**
     * Outputs instance to log.
     *
     * @param log log to output.
     * @param instance instance.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const Service& service) { return log << service.mData.mServiceID; }

private:
    servicemanager::ServiceData        mData;
    servicemanager::ServiceManagerItf& mServiceManager;
    OCISpecItf&                        mOCIManager;

    StaticString<cFilePathLen> mServiceFSPath {};
    oci::ImageSpec             mImageSpec {};
    Error                      mSpecErr {ErrorEnum::eNotFound};
};

} // namespace launcher
} // namespace sm
} // namespace aos

#endif
