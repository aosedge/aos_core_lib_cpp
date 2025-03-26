/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LAUNCHER_MOCK_HPP_
#define AOS_LAUNCHER_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/sm/launcher.hpp"

namespace aos::sm::launcher {

/**
 * Launcher mock.
 */
class LauncherMock : public LauncherItf {
public:
    MOCK_METHOD(Error, RunInstances,
        (const Array<ServiceInfo>&, const Array<LayerInfo>&, const Array<InstanceInfo>&, bool), (override));
    MOCK_METHOD(Error, OverrideEnvVars,
        (const Array<cloudprotocol::EnvVarsInstanceInfo>&, cloudprotocol::EnvVarsInstanceStatusArray&), (override));
};

/**
 * Instance status receiver mock.
 */
class InstanceStatusReceiverMock : public InstanceStatusReceiverItf {
public:
    MOCK_METHOD(Error, InstancesRunStatus, (const Array<InstanceStatus>&), (override));
    MOCK_METHOD(Error, InstancesUpdateStatus, (const Array<InstanceStatus>&), (override));
};

/**
 * Runtime mock.
 */
class RuntimeMock : public RuntimeItf {
public:
    MOCK_METHOD(Error, CreateHostFSWhiteouts, (const String&, const Array<StaticString<cFilePathLen>>&), (override));
    MOCK_METHOD(Error, PrepareServiceRootFS,
        (const String&, const String&, const Array<oci::Mount>&, const Array<StaticString<cFilePathLen>>&), (override));
    MOCK_METHOD(Error, ReleaseServiceRootFS, (const String& runtimeDir), (override));
    MOCK_METHOD(Error, PrepareServiceStorage, (const String& path, uint32_t uid, uint32_t gid), (override));
    MOCK_METHOD(Error, PrepareServiceState, (const String& path, uint32_t uid, uint32_t gid), (override));
};

} // namespace aos::sm::launcher

#endif
