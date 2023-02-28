/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/error.hpp"
#include "aos/sm/launcher.hpp"

using namespace aos;
using namespace aos::sm::launcher;
using namespace aos::sm::runner;

class TestStatusReceiver : public InstanceStatusReceiverItf {
public:
    Error InstancesRunStatus(const Array<InstanceStatus>& instances) override
    {
        (void)instances;

        return ErrorEnum::eNone;
    }
    Error InstancesUpdateStatus(const Array<InstanceStatus>& instances) override
    {
        (void)instances;

        return ErrorEnum::eNone;
    }
};

class TestRunner : public RunnerItf {
public:
    RunStatus StartInstance(const char* instanceID, const char* runtimeDir)
    {
        (void)instanceID;
        (void)runtimeDir;

        return RunStatus {};
    }

    Error StopInstance(const char* instanceID)
    {
        (void)instanceID;

        return ErrorEnum::eNone;
    }
};

class TestStorage : public StorageItf { };

TEST(launcher, RunInstances)
{
    Launcher           launcher;
    TestStatusReceiver statusReceiver;
    TestRunner         runner;
    TestStorage        storage;

    EXPECT_TRUE(launcher.Init(statusReceiver, runner, storage).IsNone());
}
