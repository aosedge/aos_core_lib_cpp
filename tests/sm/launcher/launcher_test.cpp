/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <future>

#include <gtest/gtest.h>

#include "aos/common/tools/error.hpp"
#include "aos/common/tools/log.hpp"
#include "aos/sm/launcher.hpp"

using namespace aos;
using namespace aos::sm::launcher;
using namespace aos::sm::runner;
using namespace aos::sm::servicemanager;

static constexpr auto cWaitStatusTimeout = std::chrono::seconds(5);

class TestServiceManager : public ServiceManagerItf {
public:
    Error InstallServices(const Array<ServiceInfo>& services) override
    {
        (void)services;

        return ErrorEnum::eNone;
    }

    RetWithError<ServiceData> GetService(const String serviceID) override
    {
        (void)serviceID;

        return ServiceData {};
    }

    RetWithError<ImageParts> GetImageParts(const ServiceData& service) override
    {
        (void)service;

        return ImageParts {};
    }
};

class TestRunner : public RunnerItf {
public:
    RunStatus StartInstance(const String& instanceID, const String& runtimeDir) override
    {
        (void)instanceID;
        (void)runtimeDir;

        return RunStatus {};
    }

    Error StopInstance(const String& instanceID) override
    {
        (void)instanceID;

        return ErrorEnum::eNone;
    }
};

class TestStatusReceiver : public InstanceStatusReceiverItf {
public:
    std::promise<const Array<InstanceStatus>> mPromise;

    Error InstancesRunStatus(const Array<InstanceStatus>& status) override
    {
        mPromise.set_value(status);

        return ErrorEnum::eNone;
    }

    Error InstancesUpdateStatus(const Array<InstanceStatus>& status) override
    {
        mPromise.set_value(status);

        return ErrorEnum::eNone;
    }
};

class TestStorage : public sm::launcher::StorageItf {
public:
    Error AddInstance(const InstanceInfo& instance) override
    {
        (void)instance;

        return ErrorEnum::eNone;
    }

    Error UpdateInstance(const InstanceInfo& instance) override
    {
        (void)instance;

        return ErrorEnum::eNone;
    }

    Error RemoveInstance(const InstanceIdent& instanceIdent) override
    {
        (void)instanceIdent;

        return ErrorEnum::eNone;
    }

    Error GetAllInstances(Array<InstanceInfo>& instances) override
    {
        (void)instances;

        return ErrorEnum::eNone;
    }
};

TEST(launcher, RunInstances)
{
    TestServiceManager serviceManager;
    TestRunner         runner;
    TestStatusReceiver statusReceiver;
    TestStorage        storage;

    Launcher launcher;

    EXPECT_TRUE(launcher.Init(serviceManager, runner, statusReceiver, storage).IsNone());

    InstanceInfo instances[] = {{{"service1", "subject1", 0}, 0, 0, "", ""},
        {{"service1", "subject1", 1}, 0, 0, "", ""}, {{"service1", "subject1", 2}, 0, 0, "", ""}};

    auto feature = statusReceiver.mPromise.get_future();

    EXPECT_TRUE(launcher
                    .RunInstances(
                        Array<ServiceInfo>(), Array<LayerInfo>(), Array<InstanceInfo>(instances, ArraySize(instances)))
                    .IsNone());

    EXPECT_EQ(feature.wait_for(cWaitStatusTimeout), std::future_status::ready);
    EXPECT_EQ(feature.get(), Array<InstanceStatus>());
}
