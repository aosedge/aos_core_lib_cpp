/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "aos/test/log.hpp"

#include "mocks/cnimock.hpp"
#include "mocks/networkmanagermock.hpp"

using namespace aos::sm::networkmanager;
using namespace aos::sm::cni;
using namespace testing;

class NetworkManagerTest : public Test {
protected:
    void SetUp() override
    {
        aos::test::InitLog();

        mWorkingDir = "/tmp/networkmanager_test";
        std::filesystem::create_directories(mWorkingDir.CStr());

        mCNI   = std::make_unique<StrictMock<CNIMock>>();
        mNetns = std::make_unique<StrictMock<NamespaceManagerMock>>();
        mNetIf = std::make_unique<StrictMock<NetworkInterfaceManagerMock>>();

        EXPECT_CALL(*mCNI, Init(_)).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mTrafficMonitor, Start()).WillOnce(Return(aos::ErrorEnum::eNone));

        ASSERT_EQ(
            mNetManager.Init(mStorage, *mCNI, mTrafficMonitor, *mNetns, *mNetIf, mWorkingDir), aos::ErrorEnum::eNone);
        ASSERT_EQ(mNetManager.Start(), aos::ErrorEnum::eNone);
    }

    void TearDown() override
    {
        ASSERT_EQ(mNetManager.Stop(), aos::ErrorEnum::eNone);

        std::filesystem::remove_all(mWorkingDir.CStr());
    }

    NetworkParams CreateTestNetworkParams()
    {
        NetworkParams params;
        params.mInstanceIdent.mServiceID  = "test-service";
        params.mInstanceIdent.mSubjectID  = "test-subject";
        params.mInstanceIdent.mInstance   = 0;
        params.mNetworkParameters.mIP     = "192.168.1.2";
        params.mNetworkParameters.mSubnet = "192.168.1.0/24";
        params.mHostname                  = "test-host";
        params.mUploadLimit               = 1000;
        params.mDownloadLimit             = 1000;

        params.mDNSSevers.PushBack("8.8.8.8");
        params.mDNSSevers.PushBack("8.8.4.4");

        aos::Host host1 {"10.0.0.1", "host1.example.com"};
        aos::Host host2 {"10.0.0.2", "host2.example.com"};
        params.mHosts.PushBack(host1);
        params.mHosts.PushBack(host2);

        params.mAliases.PushBack("alias1");
        params.mAliases.PushBack("alias2");

        return params;
    }

    std::string ReadFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    StorageMock                                  mStorage;
    std::unique_ptr<CNIMock>                     mCNI;
    TrafficMonitorMock                           mTrafficMonitor;
    NetworkManager                               mNetManager;
    std::unique_ptr<NamespaceManagerMock>        mNetns;
    std::unique_ptr<NetworkInterfaceManagerMock> mNetIf;
    aos::StaticString<aos::cFilePathLen>         mWorkingDir;
};

TEST_F(NetworkManagerTest, AddInstanceToNetwork_VerifyHostsFile)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    params.mHostsFilePath = aos::FS::JoinPath(mWorkingDir, "hosts");

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .WillOnce(Return(aos::RetWithError<Result>(cniResult, aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    aos::StaticString<aos::cIPLen> ip;

    EXPECT_EQ(mNetManager.GetInstanceIP(instanceID, networkID, ip), aos::ErrorEnum::eNone);

    EXPECT_EQ(ip, params.mNetworkParameters.mIP);

    std::string hostsContent = ReadFile(params.mHostsFilePath.CStr());
    EXPECT_THAT(hostsContent, HasSubstr("127.0.0.1\tlocalhost"));
    EXPECT_THAT(hostsContent, HasSubstr("::1\tlocalhost ip6-localhost ip6-loopback"));
    EXPECT_THAT(hostsContent, HasSubstr(params.mNetworkParameters.mIP + "\t" + networkID));
    EXPECT_THAT(hostsContent, HasSubstr("10.0.0.1\thost1.example.com"));
    EXPECT_THAT(hostsContent, HasSubstr("10.0.0.2\thost2.example.com"));

    if (!params.mHostname.IsEmpty()) {
        EXPECT_THAT(hostsContent, HasSubstr(params.mNetworkParameters.mIP + "\t" + params.mHostname));
    }
}

TEST_F(NetworkManagerTest, AddInstanceToNetwork_ValidateAllPluginConfigs)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    params.mNetworkParameters.mIP     = "192.168.1.2";
    params.mNetworkParameters.mSubnet = "192.168.1.0/24";
    params.mIngressKbit               = 1000;
    params.mEgressKbit                = 2000;
    params.mDownloadLimit             = 5000;
    params.mUploadLimit               = 6000;

    aos::FirewallRule rule1;
    rule1.mDstIP   = "10.0.0.1/32";
    rule1.mDstPort = "80";
    rule1.mProto   = "tcp";
    params.mNetworkParameters.mFirewallRules.PushBack(rule1);

    params.mExposedPorts.PushBack("8080/tcp");
    params.mExposedPorts.PushBack("9090/udp");

    NetworkConfigList capturedNetConfig;
    RuntimeConf       capturedRuntime;

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .WillOnce(DoAll(SaveArg<0>(&capturedNetConfig), SaveArg<1>(&capturedRuntime),
            Return(aos::RetWithError<Result>(Result(), aos::ErrorEnum::eNone))));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {
            {"/var/run/netns/test-instance"}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    {
        const auto& bridge = capturedNetConfig.mBridge;
        EXPECT_EQ(bridge.mType, "bridge");
        EXPECT_EQ(std::string(bridge.mBridge.CStr()), "br-" + std::string(networkID.CStr()));
        EXPECT_TRUE(bridge.mIsGateway);
        EXPECT_TRUE(bridge.mIPMasq);
        EXPECT_TRUE(bridge.mHairpinMode);

        EXPECT_EQ(bridge.mIPAM.mType, "host-local");
        EXPECT_EQ(bridge.mIPAM.mRange.mSubnet, params.mNetworkParameters.mSubnet);
        EXPECT_EQ(bridge.mIPAM.mRange.mRangeStart, params.mNetworkParameters.mIP);
        EXPECT_EQ(bridge.mIPAM.mRange.mRangeEnd, params.mNetworkParameters.mIP);

        ASSERT_FALSE(bridge.mIPAM.mRouters.IsEmpty());
        EXPECT_EQ(bridge.mIPAM.mRouters[0].mDst, "0.0.0.0/0");
    }

    {
        const auto& firewall = capturedNetConfig.mFirewall;
        EXPECT_EQ(firewall.mType, "aos-firewall");
        EXPECT_EQ(std::string(firewall.mIptablesAdminChainName.CStr()), "INSTANCE_" + std::string(instanceID.CStr()));
        EXPECT_EQ(firewall.mUUID, instanceID);
        EXPECT_TRUE(firewall.mAllowPublicConnections);

        ASSERT_EQ(firewall.mInputAccess.Size(), 2);
        EXPECT_EQ(firewall.mInputAccess[0].mPort, "8080");
        EXPECT_EQ(firewall.mInputAccess[0].mProtocol, "tcp");
        EXPECT_EQ(firewall.mInputAccess[1].mPort, "9090");
        EXPECT_EQ(firewall.mInputAccess[1].mProtocol, "udp");

        ASSERT_EQ(firewall.mOutputAccess.Size(), 1);
        EXPECT_EQ(firewall.mOutputAccess[0].mDstIP, "10.0.0.1/32");
        EXPECT_EQ(firewall.mOutputAccess[0].mDstPort, "80");
        EXPECT_EQ(firewall.mOutputAccess[0].mProto, "tcp");
    }

    {
        const auto& bandwidth = capturedNetConfig.mBandwidth;
        EXPECT_EQ(bandwidth.mType, "bandwidth");
        EXPECT_EQ(bandwidth.mIngressRate, params.mIngressKbit * 1000);
        EXPECT_EQ(bandwidth.mEgressRate, params.mEgressKbit * 1000);
        EXPECT_EQ(bandwidth.mIngressBurst, 12800);
        EXPECT_EQ(bandwidth.mEgressBurst, 12800);
    }

    {
        const auto& dns = capturedNetConfig.mDNS;
        EXPECT_EQ(dns.mType, "dnsname");
        EXPECT_TRUE(dns.mMultiDomain);
        EXPECT_EQ(dns.mDomainName, networkID);
        EXPECT_TRUE(dns.mCapabilities.mAliases);

        ASSERT_EQ(dns.mRemoteServers.Size(), 2);
        EXPECT_EQ(dns.mRemoteServers[0], "8.8.8.8");
        EXPECT_EQ(dns.mRemoteServers[1], "8.8.4.4");
    }

    {
        EXPECT_EQ(capturedRuntime.mContainerID, instanceID);
        EXPECT_EQ(capturedRuntime.mIfName, "eth0");

        ASSERT_GE(capturedRuntime.mArgs.Size(), 2);
        EXPECT_EQ(capturedRuntime.mArgs[0].mName, "IgnoreUnknown");
        EXPECT_EQ(capturedRuntime.mArgs[0].mValue, "1");
        EXPECT_EQ(capturedRuntime.mArgs[1].mName, "K8S_POD_NAME");
        EXPECT_EQ(capturedRuntime.mArgs[1].mValue, instanceID);

        EXPECT_EQ(capturedRuntime.mNetNS, aos::String("/var/run/netns/test-instance"));
    }
}

TEST_F(NetworkManagerTest, AddInstanceToNetwork_VerifyResolvConfFile)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    params.mResolvConfFilePath = aos::FS::JoinPath(mWorkingDir, "resolv.conf");

    Result cniResult;
    cniResult.mDNSServers.PushBack("1.1.1.1");
    cniResult.mDNSServers.PushBack("10.0.0.1");

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .WillOnce(Return(aos::RetWithError<Result>(cniResult, aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    std::string resolvContent = ReadFile(params.mResolvConfFilePath.CStr());

    EXPECT_THAT(resolvContent, HasSubstr("nameserver\t8.8.8.8"));
    EXPECT_THAT(resolvContent, HasSubstr("nameserver\t8.8.4.4"));

    for (const auto& dns : params.mDNSSevers) {
        EXPECT_THAT(resolvContent, HasSubstr("nameserver\t" + std::string(dns.CStr())));
    }
}

TEST_F(NetworkManagerTest, AddInstanceToNetwork_NoConfigFiles)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    params.mHostsFilePath      = "";
    params.mResolvConfFilePath = "";

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .WillOnce(Return(aos::RetWithError<Result>(cniResult, aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_FALSE(std::filesystem::exists(aos::FS::JoinPath(mWorkingDir, "hosts").CStr()));
    EXPECT_FALSE(std::filesystem::exists(aos::FS::JoinPath(mWorkingDir, "resolv.conf").CStr()));
}

TEST_F(NetworkManagerTest, AddInstanceToNetwork_FileCreationError)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    params.mHostsFilePath      = "/nonexistent/directory/hosts";
    params.mResolvConfFilePath = "/nonexistent/directory/resolv.conf";

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .WillOnce(Return(aos::RetWithError<Result>(cniResult, aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(*mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetIf, RemoveInterface(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_NE(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, AddInstanceToNetwork_FailOnCNIError)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .WillOnce(Return(aos::RetWithError<Result>(Result(), aos::ErrorEnum::eInvalidArgument)));

    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(*mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetIf, RemoveInterface(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eInvalidArgument);
}

TEST_F(NetworkManagerTest, AddInstanceToNetwork_FailOnTrafficMonitorError)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .WillOnce(Return(aos::RetWithError<Result>(cniResult, aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor,
        StartInstanceMonitoring(instanceID, params.mNetworkParameters.mIP, params.mDownloadLimit, params.mUploadLimit))
        .WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(*mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetIf, RemoveInterface(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eRuntime);
}

TEST_F(NetworkManagerTest, AddInstanceToNetwork_DuplicateInstance)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .WillOnce(Return(aos::RetWithError<Result>(cniResult, aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    EXPECT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eAlreadyExist);
}

TEST_F(NetworkManagerTest, RemoveInstanceFromNetwork)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_EQ(mNetManager.RemoveInstanceFromNetwork(instanceID, networkID), aos::ErrorEnum::eNotFound);

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .WillOnce(Return(aos::RetWithError<Result>(cniResult, aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetIf, RemoveInterface(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager.RemoveInstanceFromNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    aos::StaticString<aos::cIPLen> ip;

    EXPECT_EQ(mNetManager.GetInstanceIP(instanceID, networkID, ip), aos::ErrorEnum::eNotFound);
}

TEST_F(NetworkManagerTest, RemoveInstanceFromNetwork_MultipleInstances)
{
    const aos::String instanceID1 = "test-instance-1";
    const aos::String instanceID2 = "test-instance-2";
    const aos::String networkID   = "test-network";
    auto              params      = CreateTestNetworkParams();

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<Result>(cniResult, aos::ErrorEnum::eNone)));

    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .Times(3)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager.AddInstanceToNetwork(instanceID1, networkID, params), aos::ErrorEnum::eNone);

    params.mHosts.Clear();
    params.mAliases.Clear();

    aos::Host host3 {"10.0.0.3", "host3.example.com"};

    params.mHosts.PushBack(host3);
    params.mAliases.PushBack("alias3");
    params.mHostname                = "test-host-3";
    params.mInstanceIdent.mInstance = 1;

    ASSERT_EQ(mNetManager.AddInstanceToNetwork(instanceID2, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID1)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager.RemoveInstanceFromNetwork(instanceID1, networkID), aos::ErrorEnum::eNone);

    aos::StaticString<aos::cIPLen> ip;

    EXPECT_EQ(mNetManager.GetInstanceIP(instanceID2, networkID, ip), aos::ErrorEnum::eNone);

    EXPECT_EQ(ip, params.mNetworkParameters.mIP);
}

TEST_F(NetworkManagerTest, RemoveInstanceFromNetwork_AddRemovedInstance)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<Result>(cniResult, aos::ErrorEnum::eNone)));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .Times(3)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetIf, RemoveInterface(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager.RemoveInstanceFromNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, RemoveInstanceFromNetwork_FailOnCNIError)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestNetworkParams();

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(*mCNI, AddNetworkList(_, _))
        .WillOnce(Return(aos::RetWithError<Result>(cniResult, aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager.AddInstanceToNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(*mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_EQ(mNetManager.RemoveInstanceFromNetwork(instanceID, networkID), aos::ErrorEnum::eRuntime);
}

TEST_F(NetworkManagerTest, GetNetnsPath)
{
    const aos::String                    instanceID = "test-instance";
    aos::StaticString<aos::cFilePathLen> netNS;

    EXPECT_CALL(*mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {
            {"/var/run/netns/test-instance"}, aos::ErrorEnum::eNone}));

    auto [netNSPath, err] = mNetManager.GetNetnsPath(instanceID);

    EXPECT_EQ(err, aos::ErrorEnum::eNone);
    EXPECT_EQ(netNSPath, aos::String("/var/run/netns/test-instance"));
}

TEST_F(NetworkManagerTest, GetInstanceTraffic)
{
    const aos::String instanceID    = "test-instance";
    uint64_t          inputTraffic  = 0;
    uint64_t          outputTraffic = 0;

    EXPECT_CALL(mTrafficMonitor, GetInstanceTraffic(instanceID, _, _))
        .WillOnce(DoAll(SetArgReferee<1>(1000), SetArgReferee<2>(2000), Return(aos::ErrorEnum::eNone)));

    EXPECT_EQ(mNetManager.GetInstanceTraffic(instanceID, inputTraffic, outputTraffic), aos::ErrorEnum::eNone);
    EXPECT_EQ(inputTraffic, 1000);
    EXPECT_EQ(outputTraffic, 2000);

    EXPECT_CALL(mTrafficMonitor, GetInstanceTraffic(instanceID, _, _)).WillOnce(Return(aos::ErrorEnum::eNotFound));

    EXPECT_EQ(mNetManager.GetInstanceTraffic(instanceID, inputTraffic, outputTraffic), aos::ErrorEnum::eNotFound);
}

TEST_F(NetworkManagerTest, GetSystemTraffic)
{
    uint64_t inputTraffic  = 0;
    uint64_t outputTraffic = 0;

    EXPECT_CALL(mTrafficMonitor, GetSystemData(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(5000), SetArgReferee<1>(7000), Return(aos::ErrorEnum::eNone)));

    EXPECT_EQ(mNetManager.GetSystemTraffic(inputTraffic, outputTraffic), aos::ErrorEnum::eNone);
    EXPECT_EQ(inputTraffic, 5000);
    EXPECT_EQ(outputTraffic, 7000);

    EXPECT_CALL(mTrafficMonitor, GetSystemData(_, _)).WillOnce(Return(aos::ErrorEnum::eFailed));

    EXPECT_EQ(mNetManager.GetSystemTraffic(inputTraffic, outputTraffic), aos::ErrorEnum::eFailed);
}
