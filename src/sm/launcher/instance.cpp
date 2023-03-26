/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/instance.hpp"
#include "log.hpp"

namespace aos {
namespace sm {
namespace launcher {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

size_t Instance::sInstanceID = 0;

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Instance::Instance(const InstanceInfo& info)
    : mInstanceID("instance-")
    , mInfo(info)
{
    StaticString<cInstanceIDLen> tmp;

    mInstanceID.Append(tmp.Convert(sInstanceID++));

    LOG_INF() << "Create instance: " << mInfo.mInstanceIdent << ", ID: " << *this;
}

void Instance::SetService(const Service* service, const Error& err)
{
    mService = service;
    mRunError = err;

    if (mService) {
        mAosVersion = mService->Data().mVersionInfo.mAosVersion;

        LOG_DBG() << "Set service " << *service << " for instance " << *this << ", Aos version: " << mAosVersion;
    }
}

Error Instance::Start()
{
    LOG_DBG() << "Start instance: " << *this;

    return ErrorEnum::eNone;
}

Error Instance::Stop()
{
    LOG_DBG() << "Stop instance: " << *this;

    return ErrorEnum::eNone;
}

} // namespace launcher
} // namespace sm
} // namespace aos
