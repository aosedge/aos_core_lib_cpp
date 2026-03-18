/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/sm/networkmanager/networkmanager.hpp>
#include <core/sm/tests/mocks/cnimock.hpp>
#include <core/sm/tests/mocks/storagemock.hpp>

#include "mocks/interfacefactorymock.hpp"
#include "mocks/interfacemanagermock.hpp"
#include "mocks/namespacemanagermock.hpp"
#include <core/common/tests/mocks/networkprovidermock.hpp>
#include "mocks/randommock.hpp"
#include "mocks/trafficmonitormock.hpp"

using namespace aos::sm::networkmanager;
using namespace aos::sm::cni;
using namespace aos::networkmanager;
using namespace testing;

class NetworkManagerTest : public Test {
protected:
    void SetUp() override
    {
        aos::tests::utils::InitLog();

        mWorkingDir = "/tmp/networkmanager_test";
        std::filesystem::create_directories(mWorkingDir.CStr());

        EXPECT_CALL(mCNI, SetConfDir(_)).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mTrafficMonitor, Start()).WillOnce(Return(aos::ErrorEnum::eNone));

        mNetManager = std::make_unique<NetworkManager>();

        EXPECT_CALL(mStorage, GetNetworksInfo(_))
            .WillOnce(DoAll(SetArgReferee<0>(mNetworkInfos), Return(aos::ErrorEnum::eNone)));

        EXPECT_CALL(mStorage, GetInstanceNetworksInfo(_))
            .WillOnce(DoAll(SetArgReferee<0>(mInstanceNetworkInfos), Return(aos::ErrorEnum::eNone)));

        ASSERT_EQ(mNetManager->Init(mStorage, mCNI, mTrafficMonitor, mNetns, mNetIf, mRandom, mNetIfFactory,
                      mWorkingDir, mNetworkProvider, "test-node"),
            aos::ErrorEnum::eNone);
        ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
    }

    void TearDown() override
    {
        EXPECT_CALL(mTrafficMonitor, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));
        ASSERT_EQ(mNetManager->Stop(), aos::ErrorEnum::eNone);

        EXPECT_CALL(mNetIf, DeleteLink(_)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        mNetManager.reset();

        std::filesystem::remove_all(mWorkingDir.CStr());
    }

    InstanceNetworkConfig CreateTestInstanceNetworkConfig()
    {
        InstanceNetworkConfig params;
        params.mInstanceIdent.mItemID    = "test-item";
        params.mInstanceIdent.mSubjectID = "test-subject";
        params.mInstanceIdent.mInstance  = 0;
        params.mHostname                 = "test-host";
        params.mUploadLimit              = 1000;
        params.mDownloadLimit            = 1000;

        aos::Host host1 {"10.0.0.1", "host1.example.com"};
        aos::Host host2 {"10.0.0.2", "host2.example.com"};
        params.mHosts.PushBack(host1);
        params.mHosts.PushBack(host2);

        params.mAliases.PushBack("alias1");
        params.mAliases.PushBack("alias2");

        return params;
    }

    aos::InstanceNetworkAllocation CreateTestAllocatedParams()
    {
        aos::InstanceNetworkAllocation allocatedParams;
        allocatedParams.mIP     = "192.168.1.2";
        allocatedParams.mSubnet = "192.168.1.0/24";
        allocatedParams.mDNSServers.PushBack("8.8.8.8");
        allocatedParams.mDNSServers.PushBack("8.8.4.4");

        return allocatedParams;
    }

    std::string ReadFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    void SetupEnsureNodeNetworkCreateMocks(
        const aos::String& networkID, const aos::String& subnet, const aos::String& ip, uint64_t vlanID)
    {
        aos::NetworkParams netParams;
        netParams.mNetworkID = networkID;
        netParams.mSubnet    = subnet;
        netParams.mIP        = ip;
        netParams.mVlanID    = vlanID;

        EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(networkID, aos::String("test-node"), _))
            .WillOnce(DoAll(SetArgReferee<2>(netParams), Return(aos::ErrorEnum::eNone)));

        EXPECT_CALL(mRandom, RandBuffer(_, 4))
            .Times(2)
            .WillOnce(Invoke([](aos::Array<uint8_t>& buffer, size_t) {
                buffer.Resize(4);
                uint8_t data[] = {0x12, 0x34, 0xAB, 0xCD};
                for (size_t i = 0; i < sizeof(data); i++) {
                    buffer[i] = data[i];
                }
                return aos::ErrorEnum::eNone;
            }))
            .WillOnce(Invoke([](aos::Array<uint8_t>& buffer, size_t) {
                buffer.Resize(4);
                uint8_t data[] = {0xEF, 0x56, 0x78, 0x90};
                for (size_t i = 0; i < sizeof(data); i++) {
                    buffer[i] = data[i];
                }
                return aos::ErrorEnum::eNone;
            }));

        EXPECT_CALL(mStorage, AddNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    }

    void SetupEnsureNodeNetworkPhysicalMocks(const aos::String& ip, const aos::String& subnet, uint64_t vlanID)
    {
        EXPECT_CALL(mNetIfFactory, CreateBridge(_, ip, subnet)).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mNetIfFactory, CreateVlan(_, vlanID)).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mNetIf, SetMasterLink(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    }

    void SetupEnsureNodeNetworkMocks(
        const aos::String& networkID, const aos::String& subnet, const aos::String& ip, uint64_t vlanID)
    {
        SetupEnsureNodeNetworkCreateMocks(networkID, subnet, ip, vlanID);
        SetupEnsureNodeNetworkPhysicalMocks(ip, subnet, vlanID);
    }

    StrictMock<StorageMock>                                                               mStorage;
    StrictMock<CNIMock>                                                                   mCNI;
    TrafficMonitorMock                                                                    mTrafficMonitor;
    std::unique_ptr<NetworkManager>                                                       mNetManager;
    StrictMock<NamespaceManagerMock>                                                      mNetns;
    StrictMock<InterfaceManagerMock>                                                      mNetIf;
    StrictMock<InterfaceFactoryMock>                                                      mNetIfFactory;
    StrictMock<RandomMock>                                                                mRandom;
    NetworkProviderMock                                                                   mNetworkProvider;
    aos::StaticString<aos::cFilePathLen>                                                  mWorkingDir;
    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners>            mNetworkInfos;
    aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances> mInstanceNetworkInfos;
};

TEST_F(NetworkManagerTest, CreateAndStartInstanceNetwork_VerifyHostsFile)
{
    const int                                 numInstances = 4;
    std::vector<std::thread>                  threads;
    std::vector<std::string>                  instanceIDs;
    std::vector<std::string>                  networkIDs;
    std::vector<InstanceNetworkConfig>        paramsVec;
    std::vector<InstanceNetworkRuntimeParams> runtimeParamsVec;

    std::vector<aos::NetworkParams>             networkParamsVec;
    std::vector<aos::InstanceNetworkAllocation> allocatedParamsVec;

    for (int i = 0; i < numInstances; i++) {
        instanceIDs.push_back(std::string("instance-" + std::to_string(i)));
        networkIDs.push_back(std::string("network-" + std::to_string(i)));

        auto params                     = CreateTestInstanceNetworkConfig();
        params.mInstanceIdent.mInstance = i;
        std::string hostname            = "test-host-" + std::to_string(i);
        params.mHostname                = aos::String(hostname.c_str());
        paramsVec.push_back(params);

        InstanceNetworkRuntimeParams runtimeParams;
        std::string                  hostsFilePath = "hosts_" + std::to_string(i);
        runtimeParams.mHostsFilePath               = aos::fs::JoinPath(mWorkingDir, hostsFilePath.c_str());
        runtimeParamsVec.push_back(runtimeParams);

        auto        allocated = CreateTestAllocatedParams();
        std::string ip        = "192.168.1." + std::to_string(i + 2);
        allocated.mIP         = aos::String(ip.c_str());
        allocatedParamsVec.push_back(allocated);

        aos::NetworkParams netParams;
        netParams.mNetworkID = aos::String(networkIDs[i].c_str());
        netParams.mSubnet    = allocated.mSubnet;
        netParams.mIP        = aos::String(("192.168." + std::to_string(i) + ".1").c_str());
        netParams.mVlanID    = 100ULL + i;
        networkParamsVec.push_back(netParams);
    }

    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(_, aos::String("test-node"), _))
        .Times(numInstances)
        .WillRepeatedly(
            Invoke([&networkParamsVec](const aos::String& networkID, const aos::String&, aos::NetworkParams& result) {
                for (const auto& net : networkParamsVec) {
                    if (net.mNetworkID == networkID) {
                        result = net;
                        return aos::ErrorEnum::eNone;
                    }
                }
                return aos::ErrorEnum::eNotFound;
            }));

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, aos::String("test-node"), _, _))
        .Times(numInstances)
        .WillRepeatedly(Invoke(
            [&paramsVec, &allocatedParamsVec](const aos::InstanceIdent& ident, const aos::String&, const aos::String&,
                const aos::UpdateItemNetworkParams&, aos::InstanceNetworkAllocation& result) {
                for (size_t i = 0; i < paramsVec.size(); i++) {
                    if (paramsVec[i].mInstanceIdent == ident) {
                        result = allocatedParamsVec[i];
                        return aos::ErrorEnum::eNone;
                    }
                }
                return aos::ErrorEnum::eNotFound;
            }));

    EXPECT_CALL(mRandom, RandBuffer(_, 4))
        .Times(numInstances * 2)
        .WillRepeatedly(Invoke([](aos::Array<uint8_t>& buffer, size_t) {
            static int counter = 0;
            buffer.Resize(4);
            uint8_t data[] = {static_cast<uint8_t>(0x10 + counter), static_cast<uint8_t>(0x20 + counter),
                static_cast<uint8_t>(0x30 + counter), static_cast<uint8_t>(0x40 + counter)};
            for (size_t i = 0; i < sizeof(data); i++) {
                buffer[i] = data[i];
            }
            counter++;
            return aos::ErrorEnum::eNone;
        }));

    EXPECT_CALL(mStorage, AddNetworkInfo(_)).Times(numInstances).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).Times(numInstances).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).Times(numInstances).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _)).Times(numInstances).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, SetMasterLink(_, _)).Times(numInstances).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _))
        .Times(numInstances)
        .WillRepeatedly(DoAll(SetArgReferee<2>(cniResult), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(numInstances)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(numInstances).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(numInstances)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    for (int i = 0; i < numInstances; i++) {
        threads.emplace_back([this, i, &instanceIDs, &networkIDs, &paramsVec, &runtimeParamsVec]() {
            ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceIDs[i].c_str(), networkIDs[i].c_str(), paramsVec[i]),
                aos::ErrorEnum::eNone);
            ASSERT_EQ(
                mNetManager->StartInstanceNetwork(instanceIDs[i].c_str(), networkIDs[i].c_str(), runtimeParamsVec[i]),
                aos::ErrorEnum::eNone);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int i = 0; i < numInstances; i++) {
        std::string hostsContent = ReadFile(runtimeParamsVec[i].mHostsFilePath.CStr());

        EXPECT_THAT(hostsContent, HasSubstr("127.0.0.1\tlocalhost"));
        EXPECT_THAT(hostsContent, HasSubstr("::1\tlocalhost ip6-localhost ip6-loopback"));

        EXPECT_THAT(hostsContent, HasSubstr(std::string(allocatedParamsVec[i].mIP.CStr()) + "\t" + networkIDs[i]));
        EXPECT_THAT(hostsContent, HasSubstr("10.0.0.1\thost1.example.com"));
        EXPECT_THAT(hostsContent, HasSubstr("10.0.0.2\thost2.example.com"));

        if (!paramsVec[i].mHostname.IsEmpty()) {
            EXPECT_THAT(hostsContent,
                HasSubstr(std::string(allocatedParamsVec[i].mIP.CStr()) + "\t" + networkIDs[i] + " "
                    + paramsVec[i].mHostname.CStr()));
        }
    }
}

TEST_F(NetworkManagerTest, CreateAndStartInstanceNetwork_ValidateAllPluginConfigs)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    params.mIngressKbit   = 1000;
    params.mEgressKbit    = 2000;
    params.mDownloadLimit = 5000;
    params.mUploadLimit   = 6000;

    aos::FirewallRule rule1;
    rule1.mDstIP   = "10.0.0.1/32";
    rule1.mDstPort = "80";
    rule1.mProto   = "tcp";
    allocatedParams.mFirewallRules.PushBack(rule1);

    params.mExposedPorts.PushBack("8080/tcp");
    params.mExposedPorts.PushBack("9090/udp");

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    NetworkConfigList capturedNetConfig;
    RuntimeConf       capturedRuntime;

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _))
        .WillOnce(DoAll(SaveArg<0>(&capturedNetConfig), SaveArg<1>(&capturedRuntime), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {
            {"/var/run/netns/test-instance"}, aos::ErrorEnum::eNone}));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    {
        const auto& bridge = capturedNetConfig.mBridge;
        EXPECT_EQ(bridge.mType, "bridge");
        EXPECT_EQ(std::string(bridge.mBridge.CStr()).substr(0, 3), "br-");
        EXPECT_TRUE(bridge.mIsGateway);
        EXPECT_TRUE(bridge.mIPMasq);
        EXPECT_TRUE(bridge.mHairpinMode);

        EXPECT_EQ(bridge.mIPAM.mType, "host-local");
        EXPECT_EQ(bridge.mIPAM.mRange.mSubnet, allocatedParams.mSubnet);
        EXPECT_EQ(bridge.mIPAM.mRange.mRangeStart, allocatedParams.mIP);
        EXPECT_EQ(bridge.mIPAM.mRange.mRangeEnd, allocatedParams.mIP);

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

TEST_F(NetworkManagerTest, CreateAndStartInstanceNetwork_VerifyResolvConfFile)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    Result cniResult;
    cniResult.mDNSServers.PushBack("1.1.1.1");
    cniResult.mDNSServers.PushBack("10.0.0.1");

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(cniResult), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    InstanceNetworkRuntimeParams runtimeParams;
    runtimeParams.mResolvConfFilePath = aos::fs::JoinPath(mWorkingDir, "resolv.conf");

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    std::string resolvContent = ReadFile(runtimeParams.mResolvConfFilePath.CStr());

    EXPECT_THAT(resolvContent, HasSubstr("nameserver\t8.8.8.8"));
    EXPECT_THAT(resolvContent, HasSubstr("nameserver\t8.8.4.4"));

    for (const auto& dns : allocatedParams.mDNSServers) {
        EXPECT_THAT(resolvContent, HasSubstr("nameserver\t" + std::string(dns.CStr())));
    }
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_NoConfigFiles)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(cniResult), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    InstanceNetworkRuntimeParams runtimeParams;
    runtimeParams.mHostsFilePath      = "";
    runtimeParams.mResolvConfFilePath = "";

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    EXPECT_FALSE(std::filesystem::exists(aos::fs::JoinPath(mWorkingDir, "hosts").CStr()));
    EXPECT_FALSE(std::filesystem::exists(aos::fs::JoinPath(mWorkingDir, "resolv.conf").CStr()));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FileCreationError)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(cniResult), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    InstanceNetworkRuntimeParams runtimeParams;
    runtimeParams.mHostsFilePath      = "/nonexistent/directory/hosts";
    runtimeParams.mResolvConfFilePath = "/nonexistent/directory/resolv.conf";

    EXPECT_NE(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailOnCNIError)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _)).WillOnce(Return(aos::ErrorEnum::eInvalidArgument));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    InstanceNetworkRuntimeParams runtimeParams;
    EXPECT_EQ(
        mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eInvalidArgument);
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailOnTrafficMonitorError)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(cniResult), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor,
        StartInstanceMonitoring(instanceID, allocatedParams.mIP, params.mDownloadLimit, params.mUploadLimit))
        .WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    InstanceNetworkRuntimeParams runtimeParams;
    EXPECT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eRuntime);
}

TEST_F(NetworkManagerTest, CreateInstanceNetwork_Idempotent)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eAlreadyExist);
}

TEST_F(NetworkManagerTest, StopAndReleaseInstanceNetwork)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(cniResult), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mCNI, GetNetworkListCachedConfig(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->StopInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);
    EXPECT_CALL(mStorage, RemoveInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetworkProvider, ReleaseInstanceNetwork(_, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mStorage, RemoveNetworkInfo(networkID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(networkID, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->ReleaseInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, StopAndReleaseInstanceNetwork_MultipleInstances)
{
    const aos::String instanceID1     = "test-instance-1";
    const aos::String instanceID2     = "test-instance-2";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID1, networkID, params), aos::ErrorEnum::eNone);

    params.mHosts.Clear();
    params.mAliases.Clear();

    aos::Host host3 {"10.0.0.3", "host3.example.com"};

    params.mHosts.PushBack(host3);
    params.mAliases.PushBack("alias3");
    params.mHostname                = "test-host-3";
    params.mInstanceIdent.mInstance = 1;

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID2, networkID, params), aos::ErrorEnum::eNone);
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(3)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID1, networkID, runtimeParams), aos::ErrorEnum::eNone);
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID2, networkID, runtimeParams), aos::ErrorEnum::eNone);

    // Stop instance1
    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID1)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mCNI, GetNetworkListCachedConfig(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->StopInstanceNetwork(instanceID1, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mStorage, RemoveInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetworkProvider, ReleaseInstanceNetwork(_, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->ReleaseInstanceNetwork(instanceID1, networkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, StopReleaseAndRecreateInstance)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(networkID, aos::String("test-node"), _))
        .Times(2)
        .WillRepeatedly(Invoke([&](const aos::String&, const aos::String&, aos::NetworkParams& result) {
            result.mNetworkID = networkID;
            result.mSubnet    = "192.168.1.0/24";
            result.mIP        = "192.168.1.1";
            result.mVlanID    = 100ULL;
            return aos::ErrorEnum::eNone;
        }));

    EXPECT_CALL(mRandom, RandBuffer(_, 4)).Times(4).WillRepeatedly(Invoke([](aos::Array<uint8_t>& buffer, size_t) {
        static int counter = 0;
        buffer.Resize(4);
        uint8_t data[] = {static_cast<uint8_t>(0x12 + counter), static_cast<uint8_t>(0x34 + counter),
            static_cast<uint8_t>(0xAB + counter), static_cast<uint8_t>(0xCD + counter)};
        for (size_t i = 0; i < sizeof(data); i++) {
            buffer[i] = data[i];
        }
        counter++;
        return aos::ErrorEnum::eNone;
    }));

    EXPECT_CALL(mStorage, AddNetworkInfo(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, SetMasterLink(_, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgReferee<2>(cniResult), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(3)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mCNI, GetNetworkListCachedConfig(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->StopInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mStorage, RemoveInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetworkProvider, ReleaseInstanceNetwork(_, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mStorage, RemoveNetworkInfo(networkID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(networkID, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->ReleaseInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, StopInstanceNetwork_FailOnCNIError)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    Result cniResult;
    cniResult.mDNSServers.PushBack("8.8.8.8");

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(cniResult), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mCNI, DeleteNetworkList(_, _)).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mCNI, GetNetworkListCachedConfig(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->StopInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eRuntime);
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_NetworkIDMismatch)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    InstanceNetworkRuntimeParams runtimeParams;
    EXPECT_EQ(mNetManager->StartInstanceNetwork(instanceID, "wrong-network", runtimeParams),
        aos::ErrorEnum::eInvalidArgument);
}

TEST_F(NetworkManagerTest, ReleaseInstanceNetwork_NetworkIDMismatch)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_EQ(mNetManager->ReleaseInstanceNetwork(instanceID, "wrong-network"), aos::ErrorEnum::eInvalidArgument);
}

TEST_F(NetworkManagerTest, ReleaseInstanceNetwork_WithoutStop)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    EXPECT_EQ(mNetManager->ReleaseInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eInvalidArgument);
}

TEST_F(NetworkManagerTest, GetNetnsPath)
{
    const aos::String                    instanceID = "test-instance";
    aos::StaticString<aos::cFilePathLen> netNS;

    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {
            {"/var/run/netns/test-instance"}, aos::ErrorEnum::eNone}));

    auto [netNSPath, err] = mNetManager->GetNetnsPath(instanceID);

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

    EXPECT_EQ(mNetManager->GetInstanceTraffic(instanceID, inputTraffic, outputTraffic), aos::ErrorEnum::eNone);
    EXPECT_EQ(inputTraffic, 1000);
    EXPECT_EQ(outputTraffic, 2000);

    EXPECT_CALL(mTrafficMonitor, GetInstanceTraffic(instanceID, _, _)).WillOnce(Return(aos::ErrorEnum::eNotFound));

    EXPECT_EQ(mNetManager->GetInstanceTraffic(instanceID, inputTraffic, outputTraffic), aos::ErrorEnum::eNotFound);
}

TEST_F(NetworkManagerTest, GetSystemTraffic)
{
    uint64_t inputTraffic  = 0;
    uint64_t outputTraffic = 0;

    EXPECT_CALL(mTrafficMonitor, GetSystemTraffic(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(5000), SetArgReferee<1>(7000), Return(aos::ErrorEnum::eNone)));

    EXPECT_EQ(mNetManager->GetSystemTraffic(inputTraffic, outputTraffic), aos::ErrorEnum::eNone);
    EXPECT_EQ(inputTraffic, 5000);
    EXPECT_EQ(outputTraffic, 7000);

    EXPECT_CALL(mTrafficMonitor, GetSystemTraffic(_, _)).WillOnce(Return(aos::ErrorEnum::eFailed));

    EXPECT_EQ(mNetManager->GetSystemTraffic(inputTraffic, outputTraffic), aos::ErrorEnum::eFailed);
}

TEST_F(NetworkManagerTest, CreateAndStartInstanceNetwork_EnsureNodeNetworkCreatedOnce)
{
    const aos::String instanceID1      = "test-instance-1";
    const aos::String instanceID2      = "test-instance-2";
    const aos::String networkID        = "test-network";
    auto              params1          = CreateTestInstanceNetworkConfig();
    auto              params2          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams1 = CreateTestAllocatedParams();
    auto              allocatedParams2 = CreateTestAllocatedParams();

    params2.mInstanceIdent.mInstance = 1;
    allocatedParams2.mIP             = "192.168.1.3";
    params2.mHostname                = "test-host-2";
    params2.mAliases.Clear();
    params2.mAliases.PushBack("alias3");
    params2.mAliases.PushBack("alias4");

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams1.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams1), Return(aos::ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams2), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID1, networkID, params1), aos::ErrorEnum::eNone);
    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID2, networkID, params2), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams1.mSubnet, 100ULL);

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID1, networkID, runtimeParams), aos::ErrorEnum::eNone);
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID2, networkID, runtimeParams), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, InitWithExistingNetworks)
{
    NetworkInfo existingNetwork;
    existingNetwork.mNetworkID    = "network1";
    existingNetwork.mIP           = "192.168.1.1";
    existingNetwork.mSubnet       = "192.168.1.0/24";
    existingNetwork.mVlanID       = 100ULL;
    existingNetwork.mVlanIfName   = "vlan-1234abcd";
    existingNetwork.mBridgeIfName = "br-ef567890";
    mNetworkInfos.PushBack(existingNetwork);

    EXPECT_CALL(mCNI, SetConfDir(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, GetNetworksInfo(_))
        .WillOnce(DoAll(SetArgReferee<0>(mNetworkInfos), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(
        mNetIfFactory, CreateBridge(existingNetwork.mBridgeIfName, existingNetwork.mIP, existingNetwork.mSubnet))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(existingNetwork.mVlanIfName, existingNetwork.mVlanID))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, SetMasterLink(existingNetwork.mVlanIfName, existingNetwork.mBridgeIfName))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mStorage, GetInstanceNetworksInfo(_))
        .WillOnce(DoAll(SetArgReferee<0>(mInstanceNetworkInfos), Return(aos::ErrorEnum::eNone)));

    mNetManager = std::make_unique<NetworkManager>();
    ASSERT_EQ(mNetManager->Init(mStorage, mCNI, mTrafficMonitor, mNetns, mNetIf, mRandom, mNetIfFactory, mWorkingDir,
                  mNetworkProvider, "test-node"),
        aos::ErrorEnum::eNone);

    const aos::String instanceID      = "test-instance";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, aos::String("network1"), aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, "network1", params), aos::ErrorEnum::eNone);

    EXPECT_CALL(mCNI, AddNetworkList(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, "network1", runtimeParams), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, CreateInstanceNetwork_VerifyUpdateItemNetworkParams)
{
    const aos::String networkID       = "test-network";
    const aos::String instanceID      = "test-instance";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    params.mExposedPorts.PushBack("8080/tcp");
    params.mExposedPorts.PushBack("9090/udp");
    params.mAllowedConnections.PushBack("service1:80/tcp");

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    aos::UpdateItemNetworkParams capturedServiceData;

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(Invoke(
            [&capturedServiceData, &allocatedParams](const aos::InstanceIdent&, const aos::String&, const aos::String&,
                const aos::UpdateItemNetworkParams& serviceData, aos::InstanceNetworkAllocation& result) {
                capturedServiceData = serviceData;
                result              = allocatedParams;
                return aos::ErrorEnum::eNone;
            }));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    // Verify exposed ports
    ASSERT_EQ(capturedServiceData.mExposedPorts.Size(), 2U);
    EXPECT_EQ(capturedServiceData.mExposedPorts[0], "8080/tcp");
    EXPECT_EQ(capturedServiceData.mExposedPorts[1], "9090/udp");

    // Verify allowed connections
    ASSERT_EQ(capturedServiceData.mAllowedConnections.Size(), 1U);
    EXPECT_EQ(capturedServiceData.mAllowedConnections[0], "service1:80/tcp");

    // Verify hosts: hostname + instance ident variants
    // Expected: test-host, 0.test-subject.test-item, 0.test-subject.test-item.test-network,
    //           test-subject.test-item, test-subject.test-item.test-network (instance == 0)
    ASSERT_EQ(capturedServiceData.mHosts.Size(), 5U);
    EXPECT_EQ(capturedServiceData.mHosts[0], "test-host");
    EXPECT_EQ(capturedServiceData.mHosts[1], "0.test-subject.test-item");
    EXPECT_EQ(capturedServiceData.mHosts[2], "0.test-subject.test-item.test-network");
    EXPECT_EQ(capturedServiceData.mHosts[3], "test-subject.test-item");
    EXPECT_EQ(capturedServiceData.mHosts[4], "test-subject.test-item.test-network");
}
