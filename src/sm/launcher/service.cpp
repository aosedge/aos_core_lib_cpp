/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/launcher/service.hpp"
#include "log.hpp"

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Service::Service(const servicemanager::ServiceData& data, servicemanager::ServiceManagerItf& serviceManager,
    oci::OCISpecItf& ociManager)
    : mData(data)
    , mServiceManager(serviceManager)
    , mOCIManager(ociManager)
{
    LOG_DBG() << "Create service: serviceID=" << *this;
}

Error Service::LoadSpecs()
{
    LOG_DBG() << "Load specs: serviceID=" << *this;

    auto result = mServiceManager.GetImageParts(mData);
    if (!result.mError.IsNone()) {
        mSpecErr = result.mError;
        return mSpecErr;
    }

    mServiceFSPath = result.mValue.mServiceFSPath;

    auto err = mOCIManager.LoadImageSpec(result.mValue.mImageConfigPath, mImageSpec);
    if (!err.IsNone()) {
        mSpecErr = err;
        return mSpecErr;
    }

    mSpecErr = ErrorEnum::eNone;

    return mSpecErr;
}

} // namespace aos::sm::launcher
