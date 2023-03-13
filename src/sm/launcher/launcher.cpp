/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/launcher.hpp"
#include "log.hpp"

namespace aos {
namespace sm {
namespace launcher {

using namespace runner;

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::Init(InstanceStatusReceiverItf& statusReceiver, RunnerItf& runner, StorageItf& storage)
{
    LOG_DBG() << "Initialize launcher";

    mStatusReceiver = &statusReceiver;
    mRunner = &runner;
    mStorage = &storage;

    return ErrorEnum::eNone;
}

Error Launcher::RunInstances(const Array<ServiceInfo>& services, const Array<LayerInfo>& layers,
    const Array<InstanceInfo>& instances, bool forceRestart)
{
    (void)services;
    (void)layers;
    (void)instances;
    (void)forceRestart;

    LOG_DBG() << "Run instances";

    return ErrorEnum::eNone;
}

Error Launcher::UpdateRunStatus(const Array<RunStatus>& instances)
{
    (void)instances;

    LOG_DBG() << "Run instances";

    return ErrorEnum::eNone;
}

} // namespace launcher
} // namespace sm
} // namespace aos
