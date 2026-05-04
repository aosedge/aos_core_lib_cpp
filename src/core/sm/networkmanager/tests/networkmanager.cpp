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

#include "mocks/bandwidthmock.hpp"
#include "mocks/bridgenetworkmock.hpp"
#include "mocks/dnsnamemock.hpp"
#include "mocks/firewallmock.hpp"
#include "mocks/interfacefactorymock.hpp"
#include "mocks/interfacemanagermock.hpp"
#include "mocks/namespacemanagermock.hpp"
#include "mocks/randommock.hpp"
#include "mocks/trafficmonitormock.hpp"
#include <core/common/tests/mocks/networkprovidermock.hpp>

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

        ASSERT_EQ(mNetManager->Init(mStorage, mCNI, mBridgeNetwork, mFirewall, mBandwidth, mDNSName, mTrafficMonitor,
                      mNetns, mNetIf, mRandom, mNetIfFactory, mWorkingDir, mNetworkProvider, "test-node"),
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

    void ExpectAddInstanceCalls(int times = 1)
    {
        EXPECT_CALL(mBridgeNetwork, Attach(_, _, _)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mFirewall, AddInstance(_, _)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mBandwidth, Apply(_, _)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mDNSName, AddInstance(_, _)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    }

    void ExpectDeleteInstanceCalls(int times = 1)
    {
        EXPECT_CALL(mDNSName, RemoveInstance(_)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mBandwidth, Clear(_)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mFirewall, RemoveInstance(_)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mBridgeNetwork, Detach(_, _, _)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    }

    StrictMock<StorageMock>                                                               mStorage;
    StrictMock<CNIMock>                                                                   mCNI;
    StrictMock<BridgeNetworkMock>                                                         mBridgeNetwork;
    StrictMock<FirewallMock>                                                              mFirewall;
    StrictMock<BandwidthMock>                                                             mBandwidth;
    StrictMock<DNSNameMock>                                                               mDNSName;
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

    ExpectAddInstanceCalls(numInstances);

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

    aos::StaticString<aos::cIDLen>        capturedAttachInstance;
    BridgeParams                          capturedBridgeParams;
    aos::StaticString<aos::cIDLen>        capturedFirewallInstance;
    InstanceFirewallParams                capturedFirewallParams;
    aos::StaticString<aos::cInterfaceLen> capturedBandwidthIfName;
    BandwidthParams                       capturedBandwidthParams;
    aos::StaticString<aos::cIDLen>        capturedDNSInstance;
    DNSAliasesParams                      capturedDNSParams;

    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _))
        .WillOnce(DoAll(
            SaveArg<0>(&capturedAttachInstance), SaveArg<1>(&capturedBridgeParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mFirewall, AddInstance(_, _))
        .WillOnce(DoAll(
            SaveArg<0>(&capturedFirewallInstance), SaveArg<1>(&capturedFirewallParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mBandwidth, Apply(_, _))
        .WillOnce(DoAll(
            SaveArg<0>(&capturedBandwidthIfName), SaveArg<1>(&capturedBandwidthParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mDNSName, AddInstance(_, _))
        .WillOnce(
            DoAll(SaveArg<0>(&capturedDNSInstance), SaveArg<1>(&capturedDNSParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {
            {"/var/run/netns/test-instance"}, aos::ErrorEnum::eNone}));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    EXPECT_EQ(capturedAttachInstance, instanceID);
    EXPECT_EQ(std::string(capturedBridgeParams.mBridgeIfName.CStr()).substr(0, 3), "br-");
    EXPECT_EQ(capturedBridgeParams.mNetNSPath, aos::String("/var/run/netns/test-instance"));
    EXPECT_EQ(capturedBridgeParams.mContainerIfName, aos::String("eth0"));
    EXPECT_EQ(capturedBridgeParams.mGateway, aos::String("192.168.1.1"));
    EXPECT_EQ(capturedBridgeParams.mSubnet, allocatedParams.mSubnet);
    EXPECT_TRUE(capturedBridgeParams.mHairpin);
    EXPECT_TRUE(capturedBridgeParams.mIPMasq);
    EXPECT_EQ(capturedBridgeParams.mIPWithMask, aos::String(std::string(allocatedParams.mIP.CStr()) + "/24").c_str());

    EXPECT_EQ(capturedFirewallInstance, instanceID);
    EXPECT_EQ(capturedFirewallParams.mIP, allocatedParams.mIP);
    EXPECT_TRUE(capturedFirewallParams.mAllowPublic);

    ASSERT_EQ(capturedFirewallParams.mInput.Size(), 2);
    EXPECT_EQ(capturedFirewallParams.mInput[0].mPort, aos::String("8080"));
    EXPECT_EQ(capturedFirewallParams.mInput[0].mProtocol, aos::String("tcp"));
    EXPECT_EQ(capturedFirewallParams.mInput[1].mPort, aos::String("9090"));
    EXPECT_EQ(capturedFirewallParams.mInput[1].mProtocol, aos::String("udp"));

    ASSERT_EQ(capturedFirewallParams.mOutput.Size(), 1);
    EXPECT_EQ(capturedFirewallParams.mOutput[0].mDstIP, aos::String("10.0.0.1/32"));
    EXPECT_EQ(capturedFirewallParams.mOutput[0].mDstPort, aos::String("80"));
    EXPECT_EQ(capturedFirewallParams.mOutput[0].mProto, aos::String("tcp"));

    EXPECT_EQ(capturedBandwidthParams.mIngressRate, params.mIngressKbit * 1000);
    EXPECT_EQ(capturedBandwidthParams.mEgressRate, params.mEgressKbit * 1000);
    EXPECT_EQ(capturedBandwidthParams.mIngressBurst, 12800u);
    EXPECT_EQ(capturedBandwidthParams.mEgressBurst, 12800u);

    EXPECT_EQ(capturedDNSInstance, instanceID);
    EXPECT_EQ(capturedDNSParams.mNetworkID, networkID);
    EXPECT_EQ(capturedDNSParams.mIP, allocatedParams.mIP);
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

    ExpectAddInstanceCalls();

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

    ExpectAddInstanceCalls();

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

    ExpectAddInstanceCalls();

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();

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

    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _)).WillOnce(Return(aos::ErrorEnum::eInvalidArgument));

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

    ExpectAddInstanceCalls();

    EXPECT_CALL(mTrafficMonitor,
        StartInstanceMonitoring(instanceID, allocatedParams.mIP, params.mDownloadLimit, params.mUploadLimit))
        .WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();

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

    ExpectAddInstanceCalls();

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();
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

    ExpectAddInstanceCalls(2);

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
    ExpectDeleteInstanceCalls();
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

    ExpectAddInstanceCalls(2);

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
    ExpectDeleteInstanceCalls();
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

    ExpectAddInstanceCalls();

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, RemoveInstance(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBandwidth, Clear(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, RemoveInstance(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBridgeNetwork, Detach(_, _, _)).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
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

    ExpectAddInstanceCalls();
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

    ExpectAddInstanceCalls(2);

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
    ASSERT_EQ(mNetManager->Init(mStorage, mCNI, mBridgeNetwork, mFirewall, mBandwidth, mDNSName, mTrafficMonitor,
                  mNetns, mNetIf, mRandom, mNetIfFactory, mWorkingDir, mNetworkProvider, "test-node"),
        aos::ErrorEnum::eNone);

    const aos::String instanceID      = "test-instance";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, aos::String("network1"), aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, "network1", params), aos::ErrorEnum::eNone);

    ExpectAddInstanceCalls();
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

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_UpdatesFirewallRules)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    auto err = mNetManager->CreateInstanceNetwork("test-instance", "test-network", params);
    ASSERT_EQ(err, aos::ErrorEnum::eNone);

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;

    aos::FirewallRule rule;
    rule.mDstIP   = "10.0.0.5";
    rule.mDstPort = "8080";
    rule.mProto   = "tcp";
    rule.mSrcIP   = "192.168.1.2";
    update.mFirewallRules.PushBack(rule);

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    mNetManager->OnPendingFirewallUpdate("test-node", update);
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_RunningInstance_CallsCNIUpdate)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    auto err = mNetManager->CreateInstanceNetwork("test-instance", "test-network", params);
    ASSERT_EQ(err, aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    ExpectAddInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    InstanceNetworkRuntimeParams runtimeParams;
    runtimeParams.mHostsFilePath      = "/tmp/networkmanager_test/hosts";
    runtimeParams.mResolvConfFilePath = "/tmp/networkmanager_test/resolv.conf";

    err = mNetManager->StartInstanceNetwork("test-instance", "test-network", runtimeParams);
    ASSERT_EQ(err, aos::ErrorEnum::eNone);

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;

    aos::FirewallRule rule;
    rule.mDstIP   = "10.0.0.5";
    rule.mDstPort = "8080";
    rule.mProto   = "tcp";
    rule.mSrcIP   = "192.168.1.2";
    update.mFirewallRules.PushBack(rule);

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mFirewall, UpdateInstance(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    mNetManager->OnPendingFirewallUpdate("test-node", update);
}

TEST_F(NetworkManagerTest, OnConnect_SyncsNetworkStateWithCM)
{
    const aos::String instanceID = "test-instance";
    const aos::String networkID  = "test-network";
    auto              params     = CreateTestInstanceNetworkConfig();
    auto              allocated  = CreateTestAllocatedParams();

    // Create instance network (populates mInstanceNetworkInfos)
    SetupEnsureNodeNetworkCreateMocks(networkID, allocated.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocated), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    // Start instance network (populates mRuntimeCache)
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocated.mSubnet, 100ULL);

    ExpectAddInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {
            {"/var/run/netns/test-instance"}, aos::ErrorEnum::eNone}));

    InstanceNetworkRuntimeParams runtimeParams;
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID, runtimeParams), aos::ErrorEnum::eNone);

    // OnConnect should sync only running instances
    EXPECT_CALL(mNetworkProvider, SyncNetworkState(aos::String("test-node"), _))
        .WillOnce(Invoke([&](const aos::String&, const aos::Array<aos::InstanceNetworkStateInfo>& instances) {
            EXPECT_EQ(instances.Size(), 1);
            EXPECT_EQ(instances[0].mInstanceIdent, params.mInstanceIdent);
            EXPECT_EQ(instances[0].mIP, allocated.mIP);

            return aos::ErrorEnum::eNone;
        }));

    mNetManager->OnConnect();
}
