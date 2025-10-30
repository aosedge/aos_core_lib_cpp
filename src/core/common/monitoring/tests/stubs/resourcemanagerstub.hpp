/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_TESTS_STUBS_RESOURCEMANAGERSTUB_HPP_
#define AOS_CORE_COMMON_MONITORING_TESTS_STUBS_RESOURCEMANAGERSTUB_HPP_

#include <core/sm/resourcemanager/resourcemanager.hpp>

namespace aos::monitoring {

/**
 * Resource manager stub.
 */
class ResourceManagerStub : public sm::resourcemanager::ResourceManagerItf {
public:
    ResourceManagerStub(Optional<AlertRules> alertRules = {}) { mConfig.mAlertRules = alertRules; }

    RetWithError<StaticString<cVersionLen>> GetNodeConfigVersion() const override { return mConfig.mVersion; }

    Error GetNodeConfig(sm::resourcemanager::NodeConfig& nodeConfig) const override
    {
        nodeConfig = mConfig;

        return ErrorEnum::eNone;
    }

    Error GetDeviceInfo(const String& deviceName, DeviceInfo& deviceInfo) const override
    {
        (void)deviceName;
        (void)deviceInfo;

        return ErrorEnum::eNone;
    }

    Error GetResourceInfo(const String& resourceName, ResourceInfoObsolete& resourceInfo) const override
    {
        (void)resourceName;
        (void)resourceInfo;

        return ErrorEnum::eNone;
    }

    Error AllocateDevice(const String& deviceName, const String& instanceID) override
    {
        (void)deviceName;
        (void)instanceID;

        return ErrorEnum::eNone;
    }

    Error ReleaseDevice(const String& deviceName, const String& instanceID) override
    {
        (void)deviceName;
        (void)instanceID;

        return ErrorEnum::eNone;
    }

    Error ReleaseDevices(const String& instanceID) override
    {
        (void)instanceID;

        return ErrorEnum::eNone;
    }

    Error ResetAllocatedDevices() override { return ErrorEnum::eNone; }

    Error GetDeviceInstances(const String& deviceName, Array<StaticString<cIDLen>>& instanceIDs) const override
    {
        (void)deviceName;
        (void)instanceIDs;

        return ErrorEnum::eNone;
    }

    Error CheckNodeConfig(const String& version, const String& config) const override
    {
        (void)version;
        (void)config;

        return ErrorEnum::eNone;
    }

    Error UpdateNodeConfig(const String& version, const String& config) override
    {
        (void)version;
        (void)config;

        return ErrorEnum::eNone;
    }

    Error SubscribeCurrentNodeConfigChange(sm::resourcemanager::NodeConfigReceiverItf& receiver) override
    {
        (void)receiver;

        return ErrorEnum::eNone;
    }

    Error UnsubscribeCurrentNodeConfigChange(sm::resourcemanager::NodeConfigReceiverItf& receiver) override
    {
        (void)receiver;

        return ErrorEnum::eNone;
    }

private:
    sm::resourcemanager::NodeConfig mConfig {};
};

} // namespace aos::monitoring

#endif
