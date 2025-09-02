/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_MOCKS_LAUNCHERMOCK_HPP_
#define AOS_CORE_SM_TESTS_MOCKS_LAUNCHERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/launcher/launcher.hpp>

namespace aos::sm::launcher {

/**
 * Launcher mock.
 */
class LauncherMock : public LauncherItf {
public:
    MOCK_METHOD(Error, RunInstances,
        (const Array<ServiceInfo>&, const Array<LayerInfo>&, const Array<InstanceInfo>&, bool), (override));
    MOCK_METHOD(Error, GetCurrentRunStatus, (Array<InstanceStatusObsolete>&), (const override));
    MOCK_METHOD(Error, OverrideEnvVars,
        (const Array<cloudprotocol::EnvVarsInstanceInfo>&, Array<cloudprotocol::EnvVarsInstanceStatus>&), (override));
};

/**
 * Instance status receiver mock.
 */
class InstanceStatusReceiverMock : public InstanceStatusReceiverItf {
public:
    MOCK_METHOD(Error, InstancesRunStatus, (const Array<InstanceStatusObsolete>&), (override));
    MOCK_METHOD(Error, InstancesUpdateStatus, (const Array<InstanceStatusObsolete>&), (override));
};

/**
 * Runtime mock.
 */
class RuntimeMock : public RuntimeItf {
public:
    MOCK_METHOD(Error, CreateHostFSWhiteouts, (const String&, const Array<StaticString<cFilePathLen>>&), (override));
    MOCK_METHOD(Error, CreateMountPoints, (const String&, const Array<Mount>&), (override));
    MOCK_METHOD(Error, MountServiceRootFS, (const String&, const Array<StaticString<cFilePathLen>>&), (override));
    MOCK_METHOD(Error, UmountServiceRootFS, (const String&), (override));
    MOCK_METHOD(Error, PrepareServiceStorage, (const String& path, uint32_t uid, uint32_t gid), (override));
    MOCK_METHOD(Error, PrepareServiceState, (const String& path, uint32_t uid, uint32_t gid), (override));
    MOCK_METHOD(Error, PrepareNetworkDir, (const String& path), (override));
    MOCK_METHOD(RetWithError<StaticString<cFilePathLen>>, GetAbsPath, (const String& path), (override));
    MOCK_METHOD(RetWithError<uint32_t>, GetGIDByName, (const String& groupName), (override));
    MOCK_METHOD(Error, PopulateHostDevices, (const String& devicePath, Array<oci::LinuxDevice>& devices), (override));
};

} // namespace aos::sm::launcher

#endif
