/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/sm/networkmanager/networkmanager.hpp>
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
using namespace aos::networkmanager;
using namespace testing;

namespace {
constexpr auto cUplinkIfName   = "eth0";
constexpr auto cTestNetworkID  = "test-network";
constexpr auto cTestInstanceID = "test-instance";
constexpr auto cTestHostIfName = "veth-test";

} // namespace

class NetworkManagerTest : public Test {
protected:
    void SetUp() override
    {
        aos::tests::utils::InitLog();

        mWorkingDir = "/tmp/networkmanager_test";
        std::filesystem::create_directories(mWorkingDir.CStr());

        EXPECT_CALL(mFirewall, Start()).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mFirewall, RemoveOrphans(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mTrafficMonitor, Start()).WillOnce(Return(aos::ErrorEnum::eNone));

        // NetworkManager::Start reaps DNS orphans from a previous SM lifetime
        // (networkIDs no longer in storage). With an empty storage fixture
        // this is a no-op for the backend, but the call still happens.
        EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));

        // DNS factory: per-network instances are torn down by ClearNetwork (from
        // StopInstanceNetwork and ~NetworkManager). Allow any number of factory
        // RemoveInstance and per-handle RemoveHost calls; explicit per-test
        // expectations override these when sequences need to be asserted.
        EXPECT_CALL(mDNSName, RemoveServer(_)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mDNSServer, RemoveHost(_)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));

        EXPECT_CALL(mNetIf, GetLink(_, _)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNotFound));

        EXPECT_CALL(mNetIf, GetUplinkInterface(_))
            .Times(AnyNumber())
            .WillRepeatedly(DoAll(SetArgReferee<0>(aos::String(cUplinkIfName)), Return(aos::ErrorEnum::eNone)));

        // Masquerade is a per-network rule installed/removed by CreateNetwork /
        // ClearNetwork; leave it lenient so per-test sequences need not assert it.
        EXPECT_CALL(mFirewall, AddMasquerade(_, _)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mFirewall, RemoveMasquerade(_, _)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));

        mNetManager = std::make_unique<NetworkManager>();

        EXPECT_CALL(mStorage, GetNetworksInfo(_))
            .WillOnce(Invoke([this](aos::Array<aos::sm::networkmanager::NetworkInfo>& out) {
                out = mNetworkInfos;

                return aos::ErrorEnum::eNone;
            }));

        EXPECT_CALL(mStorage, GetInstanceNetworksInfo(_))
            .WillOnce(Invoke([this](aos::Array<aos::sm::networkmanager::InstanceNetworkInfo>& out) {
                out = mInstanceNetworkInfos;

                return aos::ErrorEnum::eNone;
            }));

        ASSERT_EQ(mNetManager->Init(mAllocator, mStorage, mBridgeNetwork, mFirewall, mBandwidth, mDNSName,
                      mTrafficMonitor, mNetns, mNetIf, mRandom, mNetIfFactory, mNetworkProvider, "test-node"),
            aos::ErrorEnum::eNone);
        ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
    }

    void TearDown() override
    {
        EXPECT_CALL(mTrafficMonitor, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mFirewall, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));
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
        EXPECT_CALL(mNetIfFactory, CreateVlan(_, vlanID, _)).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mDNSName, CreateServer(_, _))
            .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
    }

    void SetupEnsureNodeNetworkMocks(
        const aos::String& networkID, const aos::String& subnet, const aos::String& ip, uint64_t vlanID)
    {
        SetupEnsureNodeNetworkCreateMocks(networkID, subnet, ip, vlanID);
        SetupEnsureNodeNetworkPhysicalMocks(ip, subnet, vlanID);
    }

    void ExpectAddInstanceCalls(int times = 1)
    {
        BridgeAttachResult attachResult;
        attachResult.mHostIfName      = "veth-test";
        attachResult.mContainerIfName = "eth0";

        EXPECT_CALL(mBridgeNetwork, Attach(_, _, _))
            .Times(times)
            .WillRepeatedly(DoAll(SetArgReferee<2>(attachResult), Return(aos::ErrorEnum::eNone)));
        EXPECT_CALL(mFirewall, AddInstance(_, _)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mBandwidth, Apply(_, _)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mDNSServer, AddHost(_, _)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    }

    void ExpectPersistInstanceCalls(int times = 1)
    {
        EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    }

    NetworkInfo CreateTestNetworkInfo()
    {
        NetworkInfo network;
        network.mNetworkID    = "network1";
        network.mIP           = "192.168.1.1";
        network.mSubnet       = "192.168.1.0/24";
        network.mVlanID       = 100ULL;
        network.mVlanIfName   = "vlan-1234abcd";
        network.mBridgeIfName = "br-ef567890";

        return network;
    }

    void InitWithStoredNetwork(const NetworkInfo& network)
    {
        mNetworkInfos.PushBack(network);

        EXPECT_CALL(mStorage, GetNetworksInfo(_))
            .WillOnce(Invoke([this](aos::Array<aos::sm::networkmanager::NetworkInfo>& out) {
                out = mNetworkInfos;

                return aos::ErrorEnum::eNone;
            }));
        EXPECT_CALL(mStorage, GetInstanceNetworksInfo(_))
            .WillOnce(Invoke([this](aos::Array<aos::sm::networkmanager::InstanceNetworkInfo>& out) {
                out = mInstanceNetworkInfos;

                return aos::ErrorEnum::eNone;
            }));

        mNetManager = std::make_unique<NetworkManager>();

        ASSERT_EQ(mNetManager->Init(mAllocator, mStorage, mBridgeNetwork, mFirewall, mBandwidth, mDNSName,
                      mTrafficMonitor, mNetns, mNetIf, mRandom, mNetIfFactory, mNetworkProvider, "test-node"),
            aos::ErrorEnum::eNone);
    }

    void ExpectLinkExists(const aos::String& ifName, LinkKind kind, const aos::String& master = "")
    {
        LinkInfo link;
        link.mName   = ifName;
        link.mKind   = kind;
        link.mMaster = master;

        EXPECT_CALL(mNetIf, GetLink(ifName, _))
            .WillRepeatedly(DoAll(SetArgReferee<1>(link), Return(aos::ErrorEnum::eNone)));
    }

    void RestartWithStoredState(const aos::Array<aos::sm::networkmanager::NetworkInfo>& networks,
        const aos::Array<aos::sm::networkmanager::InstanceNetworkInfo>& instances, bool expectOrphanReaping = true)
    {
        EXPECT_CALL(mTrafficMonitor, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mFirewall, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));
        ASSERT_EQ(mNetManager->Stop(), aos::ErrorEnum::eNone);

        EXPECT_CALL(mNetIf, DeleteLink(_)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        mNetManager.reset();

        EXPECT_CALL(mFirewall, Start()).WillOnce(Return(aos::ErrorEnum::eNone));

        if (expectOrphanReaping) {
            EXPECT_CALL(mFirewall, RemoveOrphans(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
        }

        EXPECT_CALL(mTrafficMonitor, Start()).WillOnce(Return(aos::ErrorEnum::eNone));

        EXPECT_CALL(mStorage, GetNetworksInfo(_))
            .WillOnce(Invoke([&networks](aos::Array<aos::sm::networkmanager::NetworkInfo>& out) {
                out = networks;
                return aos::ErrorEnum::eNone;
            }));
        EXPECT_CALL(mStorage, GetInstanceNetworksInfo(_))
            .WillOnce(Invoke([&instances](aos::Array<aos::sm::networkmanager::InstanceNetworkInfo>& out) {
                out = instances;
                return aos::ErrorEnum::eNone;
            }));

        mNetManager = std::make_unique<NetworkManager>();

        ASSERT_EQ(mNetManager->Init(mAllocator, mStorage, mBridgeNetwork, mFirewall, mBandwidth, mDNSName,
                      mTrafficMonitor, mNetns, mNetIf, mRandom, mNetIfFactory, mNetworkProvider, "test-node"),
            aos::ErrorEnum::eNone);
    }

    aos::sm::networkmanager::InstanceNetworkInfo CreateLeftoverInstance(
        const aos::sm::networkmanager::NetworkInfo& network)
    {
        aos::sm::networkmanager::InstanceNetworkInfo leftover;
        leftover.mInstanceID                   = "leftover-instance";
        leftover.mNetworkID                    = network.mNetworkID;
        leftover.mNetworkConfig.mHostname      = "leftover-host";
        leftover.mNetworkConfig.mDownloadLimit = 4096;
        leftover.mNetworkConfig.mUploadLimit   = 2048;
        leftover.mAllocatedParams.mIP          = "192.168.1.5";
        leftover.mAllocatedParams.mSubnet      = network.mSubnet;
        leftover.mHostIfName                   = "veth-leftover";

        return leftover;
    }

    void ExpectLeftoverInstanceCleaned()
    {
        EXPECT_CALL(mDNSName, CreateServer(_, _))
            .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
        EXPECT_CALL(mDNSServer, RemoveHost(aos::String("leftover-instance"))).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mFirewall, RemoveInstance(_)).WillOnce(Return(aos::ErrorEnum::eNone));
        // The leftover instance has no bandwidth shaping, so no bandwidth clear is expected.
        // The host veth is no longer detached synchronously on cleanup; the instance
        // netns teardown reaps it asynchronously.
        EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    }

    void ExpectLeftoverInstanceUntouched()
    {
        // Adopting a running instance registers the network DNS server so later cleanup can reach it.
        EXPECT_CALL(mDNSName, CreateServer(_, _))
            .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
        EXPECT_CALL(mDNSServer, RemoveHost(_)).Times(0);
        EXPECT_CALL(mBandwidth, Clear(_)).Times(0);
        EXPECT_CALL(mFirewall, RemoveInstance(_)).Times(0);
        EXPECT_CALL(mBridgeNetwork, Detach(_, _)).Times(0);
        EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).Times(0);
        EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(0);
    }

    void ExpectStoredNetworkLinksCreated(const NetworkInfo& network)
    {
        EXPECT_CALL(mNetIfFactory, CreateBridge(network.mBridgeIfName, network.mIP, network.mSubnet))
            .WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mNetIfFactory, CreateVlan(network.mVlanIfName, network.mVlanID, network.mBridgeIfName))
            .WillOnce(Return(aos::ErrorEnum::eNone));
    }

    NetworkInfo CreateSecondTestNetworkInfo()
    {
        NetworkInfo network;
        network.mNetworkID    = "network2";
        network.mIP           = "192.168.2.1";
        network.mSubnet       = "192.168.2.0/24";
        network.mVlanID       = 200ULL;
        network.mVlanIfName   = "vlan-22222222";
        network.mBridgeIfName = "br-33333333";

        return network;
    }

    void ExpectStartInstanceOnStoredNetwork(const aos::String& instanceID, const aos::String& networkID)
    {
        EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
            .WillOnce(DoAll(SetArgReferee<4>(CreateTestAllocatedParams()), Return(aos::ErrorEnum::eNone)));
        EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

        ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, CreateTestInstanceNetworkConfig()),
            aos::ErrorEnum::eNone);
    }

    void UpdateUnitNetworks(std::initializer_list<const char*> networkIDs)
    {
        aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumOwners> ids;

        for (const auto& id : networkIDs) {
            ASSERT_TRUE(ids.PushBack(id).IsNone());
        }

        mNetManager->OnUnitNetworksUpdate("test-node", ids);
    }

    void ExpectStartInstanceCalls()
    {
        ExpectAddInstanceCalls();
        ExpectPersistInstanceCalls();
        EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
            .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    }

    void ExpectDeleteInstanceCalls(int times = 1)
    {
        EXPECT_CALL(mDNSServer, RemoveHost(_)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mFirewall, RemoveInstance(_)).Times(times).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    }

    void CreateTestInstance(const aos::String& instanceID, const InstanceNetworkConfig& params,
        const aos::InstanceNetworkAllocation& allocated, bool newNetwork = true)
    {
        if (newNetwork) {
            SetupEnsureNodeNetworkCreateMocks(cTestNetworkID, allocated.mSubnet, "192.168.1.1", 100ULL);
        }

        EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, aos::String(cTestNetworkID), _, _, _))
            .WillOnce(DoAll(SetArgReferee<4>(allocated), Return(aos::ErrorEnum::eNone)));
        EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

        ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, cTestNetworkID, params), aos::ErrorEnum::eNone);
    }

    void CreateTestInstance()
    {
        CreateTestInstance(cTestInstanceID, CreateTestInstanceNetworkConfig(), CreateTestAllocatedParams());
    }

    InstanceNetworkConfig CreateSecondInstanceNetworkConfig()
    {
        auto params = CreateTestInstanceNetworkConfig();

        params.mHosts.Clear();
        params.mAliases.Clear();
        params.mHosts.PushBack(aos::Host {"10.0.0.3", "host3.example.com"});
        params.mAliases.PushBack("alias3");
        params.mHostname                = "test-host-3";
        params.mInstanceIdent.mInstance = 1;

        return params;
    }

    void ExpectNodeNetworkParams(const aos::String& networkID)
    {
        aos::NetworkParams netParams;

        netParams.mNetworkID = networkID;
        netParams.mSubnet    = "192.168.1.0/24";
        netParams.mIP        = "192.168.1.1";
        netParams.mVlanID    = 100ULL;

        EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(networkID, aos::String("test-node"), _))
            .WillOnce(DoAll(SetArgReferee<2>(netParams), Return(aos::ErrorEnum::eNone)));
    }

    static aos::Error FillRandomBuffer(aos::Array<uint8_t>& buffer, size_t size)
    {
        buffer.Resize(size);

        for (auto& item : buffer) {
            item = 0x42;
        }

        return aos::ErrorEnum::eNone;
    }

    void ExpectNetnsCreated(bool deleted = false)
    {
        EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
            .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

        if (deleted) {
            EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
        }
    }

    void ExpectBridgeAttached(bool detached = false)
    {
        BridgeAttachResult attachResult;

        attachResult.mHostIfName      = cTestHostIfName;
        attachResult.mContainerIfName = "eth0";

        EXPECT_CALL(mBridgeNetwork, Attach(_, _, _))
            .WillOnce(DoAll(SetArgReferee<2>(attachResult), Return(aos::ErrorEnum::eNone)));

        if (detached) {
            EXPECT_CALL(mBridgeNetwork, Detach(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
        }
    }

    void StartTestInstance(const aos::String& instanceID = cTestInstanceID, bool newNetwork = true)
    {
        if (newNetwork) {
            SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);
        }

        ExpectAddInstanceCalls();
        ExpectPersistInstanceCalls();
        ExpectNetnsCreated();
        EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(instanceID, _, _, _))
            .WillOnce(Return(aos::ErrorEnum::eNone));

        ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, cTestNetworkID), aos::ErrorEnum::eNone);
    }

    void BeginTestBatch()
    {
        EXPECT_CALL(mStorage, BeginTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mFirewall, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mTrafficMonitor, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));

        ASSERT_EQ(mNetManager->BeginBatch(), aos::ErrorEnum::eNone);
    }

    void ExpectFirewallFlushFailure()
    {
        EXPECT_CALL(mFirewall, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eFailed));
        EXPECT_CALL(mTrafficMonitor, AbortBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
        EXPECT_CALL(mStorage, RollbackTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));
    }

    void RestartAndStart(const aos::Array<aos::sm::networkmanager::NetworkInfo>& networks,
        const aos::Array<aos::sm::networkmanager::InstanceNetworkInfo>&          instances)
    {
        RestartWithStoredState(networks, instances);

        EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));

        ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
    }

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    aos::HeapAllocator mAllocator;

    StrictMock<StorageMock>                                                               mStorage;
    StrictMock<BridgeNetworkMock>                                                         mBridgeNetwork;
    StrictMock<FirewallMock>                                                              mFirewall;
    StrictMock<BandwidthMock>                                                             mBandwidth;
    StrictMock<DNSNameMock>                                                               mDNSName;
    StrictMock<DNSServerMock>                                                             mDNSServer;
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
    const int                          numInstances = 4;
    std::vector<std::thread>           threads;
    std::vector<std::string>           instanceIDs;
    std::vector<std::string>           networkIDs;
    std::vector<InstanceNetworkConfig> paramsVec;

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
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).Times(numInstances).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, CreateServer(_, _))
        .Times(numInstances)
        .WillRepeatedly(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));

    ExpectAddInstanceCalls(numInstances);
    ExpectPersistInstanceCalls(numInstances);

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(numInstances)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(numInstances).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(numInstances)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    for (int i = 0; i < numInstances; i++) {
        threads.emplace_back([this, i, &instanceIDs, &networkIDs, &paramsVec]() {
            ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceIDs[i].c_str(), networkIDs[i].c_str(), paramsVec[i]),
                aos::ErrorEnum::eNone);
            ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceIDs[i].c_str(), networkIDs[i].c_str()),
                aos::ErrorEnum::eNone);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int i = 0; i < numInstances; i++) {
        aos::StaticArray<aos::Host, aos::cMaxNumHosts * 2> hosts;
        ASSERT_EQ(mNetManager->GetHosts(instanceIDs[i].c_str(), hosts), aos::ErrorEnum::eNone);

        std::string hostsContent;
        for (const auto& host : hosts) {
            hostsContent += std::string(host.mIP.CStr()) + "\t" + host.mHostname.CStr() + "\n";
        }

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

    aos::FirewallRule rule2;
    rule2.mDstIP   = "10.0.0.2/32";
    rule2.mDstPort = "7400:7650";
    rule2.mProto   = "udp";
    allocatedParams.mFirewallRules.PushBack(rule2);

    params.mExposedPorts.PushBack("8080/tcp");
    params.mExposedPorts.PushBack("9090/udp");
    params.mExposedPorts.PushBack("8089/tcp");
    params.mExposedPorts.PushBack("9000");

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
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, AddInstance(_, _))
        .WillOnce(DoAll(
            SaveArg<0>(&capturedFirewallInstance), SaveArg<1>(&capturedFirewallParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mBandwidth, Apply(_, _))
        .WillOnce(DoAll(
            SaveArg<0>(&capturedBandwidthIfName), SaveArg<1>(&capturedBandwidthParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mDNSServer, AddHost(_, _))
        .WillOnce(
            DoAll(SaveArg<0>(&capturedDNSInstance), SaveArg<1>(&capturedDNSParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {
            {"/var/run/netns/test-instance"}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_EQ(capturedAttachInstance, instanceID);
    EXPECT_EQ(std::string(capturedBridgeParams.mBridgeIfName.CStr()).substr(0, 3), "br-");
    EXPECT_EQ(capturedBridgeParams.mNetNSPath, aos::String("/var/run/netns/test-instance"));
    EXPECT_EQ(capturedBridgeParams.mContainerIfName, aos::String("eth0"));
    EXPECT_EQ(capturedBridgeParams.mGateway, aos::String("192.168.1.1"));
    EXPECT_TRUE(capturedBridgeParams.mHairpin);
    EXPECT_EQ(std::string(capturedBridgeParams.mIPWithMask.CStr()), std::string(allocatedParams.mIP.CStr()) + "/24");

    EXPECT_EQ(capturedFirewallInstance, instanceID);
    EXPECT_EQ(capturedFirewallParams.mIP, allocatedParams.mIP);
    EXPECT_TRUE(capturedFirewallParams.mAllowPublic);

    ASSERT_EQ(capturedFirewallParams.mInput.Size(), 4);
    EXPECT_EQ(capturedFirewallParams.mInput[0].mPort, aos::String("8080"));
    EXPECT_EQ(capturedFirewallParams.mInput[0].mProtocol, aos::String("tcp"));
    EXPECT_EQ(capturedFirewallParams.mInput[1].mPort, aos::String("9090"));
    EXPECT_EQ(capturedFirewallParams.mInput[1].mProtocol, aos::String("udp"));
    EXPECT_EQ(capturedFirewallParams.mInput[2].mPort, aos::String("8089"));
    EXPECT_EQ(capturedFirewallParams.mInput[2].mProtocol, aos::String("tcp"));
    EXPECT_EQ(capturedFirewallParams.mInput[3].mPort, aos::String("9000"));
    EXPECT_EQ(capturedFirewallParams.mInput[3].mProtocol, aos::String("tcp"));

    ASSERT_EQ(capturedFirewallParams.mOutput.Size(), 2);
    EXPECT_EQ(capturedFirewallParams.mOutput[0].mDstIP, aos::String("10.0.0.1/32"));
    EXPECT_EQ(capturedFirewallParams.mOutput[0].mDstPort, aos::String("80"));
    EXPECT_EQ(capturedFirewallParams.mOutput[0].mProto, aos::String("tcp"));
    EXPECT_EQ(capturedFirewallParams.mOutput[1].mDstIP, aos::String("10.0.0.2/32"));
    EXPECT_EQ(capturedFirewallParams.mOutput[1].mDstPort, aos::String("7400:7650"));
    EXPECT_EQ(capturedFirewallParams.mOutput[1].mProto, aos::String("udp"));

    EXPECT_EQ(capturedBandwidthParams.mIngressRate, params.mIngressKbit * 1000);
    EXPECT_EQ(capturedBandwidthParams.mEgressRate, params.mEgressKbit * 1000);
    EXPECT_EQ(capturedBandwidthParams.mIngressBurst, 12800u);
    EXPECT_EQ(capturedBandwidthParams.mEgressBurst, 12800u);

    EXPECT_EQ(capturedDNSInstance, instanceID);
    EXPECT_EQ(capturedDNSParams.mIP, allocatedParams.mIP);
}

TEST_F(NetworkManagerTest, CreateAndStartInstanceNetwork_ExposedPortsBeyondFirewallRuleLimit)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    constexpr uint16_t cFirstPort = 7410;
    constexpr uint16_t cLastPort  = 7450;

    for (uint16_t port = cFirstPort; port <= cLastPort; ++port) {
        ASSERT_TRUE(params.mExposedPorts.PushBack((std::to_string(port) + "/udp").c_str()).IsNone());
    }

    const size_t cNumPorts = params.mExposedPorts.Size();

    ASSERT_GT(cNumPorts, aos::cMaxNumFirewallRules);

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    InstanceFirewallParams capturedFirewallParams;

    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, AddInstance(_, _))
        .WillOnce(DoAll(SaveArg<1>(&capturedFirewallParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mBandwidth, Apply(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, AddHost(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectPersistInstanceCalls();

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    ASSERT_EQ(capturedFirewallParams.mInput.Size(), cNumPorts);
    EXPECT_EQ(capturedFirewallParams.mInput[0].mPort, aos::String("7410"));
    EXPECT_EQ(capturedFirewallParams.mInput[0].mProtocol, aos::String("udp"));
    EXPECT_EQ(capturedFirewallParams.mInput[cNumPorts - 1].mPort, aos::String("7450"));
    EXPECT_EQ(capturedFirewallParams.mInput[cNumPorts - 1].mProtocol, aos::String("udp"));
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
    ExpectPersistInstanceCalls();

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    aos::StaticArray<aos::StaticString<aos::cIPLen>, aos::cMaxNumDNSServers + 1> servers;
    ASSERT_EQ(mNetManager->GetResolvServers(instanceID, servers), aos::ErrorEnum::eNone);

    // The caller builds resolv.conf from the returned IPs; reproduce it here to
    // reuse the content assertions.
    std::string resolvContent;
    for (const auto& server : servers) {
        resolvContent += "nameserver\t" + std::string(server.CStr()) + "\n";
    }

    // Per-bridge dnsmasq listens on the bridge IP — must be the primary nameserver.
    EXPECT_THAT(resolvContent, HasSubstr("nameserver\t192.168.1.1"));
    EXPECT_THAT(resolvContent, HasSubstr("nameserver\t8.8.8.8"));
    EXPECT_THAT(resolvContent, HasSubstr("nameserver\t8.8.4.4"));

    for (const auto& dns : allocatedParams.mDNSServers) {
        EXPECT_THAT(resolvContent, HasSubstr("nameserver\t" + std::string(dns.CStr())));
    }
}

TEST_F(NetworkManagerTest, BeginFlushBatch_ForwardToBackends)
{
    EXPECT_CALL(mStorage, BeginTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->BeginBatch(), aos::ErrorEnum::eNone);

    EXPECT_CALL(mFirewall, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, CommitTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, AbortBatch()).Times(0);

    aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumInstances> failed;

    EXPECT_EQ(mNetManager->FlushBatch(failed), aos::ErrorEnum::eNone);
    EXPECT_TRUE(failed.IsEmpty());
}

TEST_F(NetworkManagerTest, BeginBatch_PropagatesBackendError)
{
    EXPECT_CALL(mStorage, BeginTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mStorage, RollbackTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_NE(mNetManager->BeginBatch(), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, FlushBatch_NftFailure_RevertsAndFallsBack)
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
    params.mHosts.PushBack(aos::Host {"10.0.0.3", "host3.example.com"});
    params.mAliases.PushBack("alias3");
    params.mHostname                = "test-host-3";
    params.mInstanceIdent.mInstance = 1;

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID2, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_CALL(mStorage, BeginTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->BeginBatch(), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    BridgeAttachResult attachResult;
    attachResult.mHostIfName      = "veth-test";
    attachResult.mContainerIfName = "eth0";

    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgReferee<2>(attachResult), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mBandwidth, Apply(_, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, AddHost(_, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mFirewall, AddInstance(instanceID1, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, AddInstance(instanceID2, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, RemoveInstance(instanceID2)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(instanceID1, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(instanceID2, _, _, _))
        .WillOnce(Return(aos::ErrorEnum::eNone))
        .WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(3).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID1, networkID), aos::ErrorEnum::eNone);
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID2, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mFirewall, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mTrafficMonitor, AbortBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, FlushBatch()).Times(0);
    EXPECT_CALL(mStorage, RollbackTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));

    aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumInstances> failed;

    EXPECT_EQ(mNetManager->FlushBatch(failed), aos::ErrorEnum::eNone);
    ASSERT_EQ(failed.Size(), 1U);
    EXPECT_EQ(failed[0], instanceID2);
}

TEST_F(NetworkManagerTest, FlushBatch_NftFailure_AbortsTrafficBatchBeforeReapply)
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

    EXPECT_CALL(mStorage, BeginTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->BeginBatch(), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    BridgeAttachResult attachResult;
    attachResult.mHostIfName      = "veth-test";
    attachResult.mContainerIfName = "eth0";

    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(attachResult), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mBandwidth, Apply(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, AddHost(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mFirewall, AddInstance(instanceID, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    // The batch is dead once the firewall flush fails, so the traffic batch must be dropped before
    // the per-instance fallback re-applies monitoring, otherwise the re-apply is staged and lost.
    Sequence trafficSeq;

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(instanceID, _, _, _))
        .InSequence(trafficSeq)
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, AbortBatch()).InSequence(trafficSeq).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(instanceID, _, _, _))
        .InSequence(trafficSeq)
        .WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mFirewall, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mTrafficMonitor, FlushBatch()).Times(0);
    EXPECT_CALL(mStorage, RollbackTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));

    aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumInstances> failed;

    EXPECT_EQ(mNetManager->FlushBatch(failed), aos::ErrorEnum::eNone);
    EXPECT_TRUE(failed.IsEmpty());
}

TEST_F(NetworkManagerTest, FlushBatch_CommitFailure_MarksAllFailed)
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
    params.mHosts.PushBack(aos::Host {"10.0.0.3", "host3.example.com"});
    params.mAliases.PushBack("alias3");
    params.mHostname                = "test-host-3";
    params.mInstanceIdent.mInstance = 1;

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID2, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_CALL(mStorage, BeginTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->BeginBatch(), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    ExpectAddInstanceCalls(2);
    ExpectPersistInstanceCalls(2);
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID1, networkID), aos::ErrorEnum::eNone);
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID2, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mFirewall, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, CommitTransaction()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mFirewall, Revert()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, Revert()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, RollbackTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, AbortBatch()).Times(0);

    aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumInstances> failed;

    EXPECT_EQ(mNetManager->FlushBatch(failed), aos::ErrorEnum::eNone);
    ASSERT_EQ(failed.Size(), 2U);
    EXPECT_EQ(failed[0], instanceID1);
    EXPECT_EQ(failed[1], instanceID2);
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailOnAttachError)
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

    EXPECT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eInvalidArgument);
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
    EXPECT_CALL(mBridgeNetwork, Detach(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();

    EXPECT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eRuntime);
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_CreatesDNSServerSeparatelyFromNetwork)
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

    // The network links are created once and kept although the DNS server fails to start.
    EXPECT_CALL(mNetIfFactory, CreateBridge(_, aos::String("192.168.1.1"), allocatedParams.mSubnet))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, 100ULL, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, CreateServer(networkID, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {nullptr, aos::ErrorEnum::eFailed}))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(0);
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(instanceID, networkID).Is(aos::ErrorEnum::eFailed));

    Mock::VerifyAndClearExpectations(&mNetns);
    Mock::VerifyAndClearExpectations(&mNetIf);

    EXPECT_CALL(mNetIf, GetLink(_, _)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNotFound));
    EXPECT_CALL(mNetIf, GetUplinkInterface(_))
        .Times(AnyNumber())
        .WillRepeatedly(DoAll(SetArgReferee<0>(aos::String(cUplinkIfName)), Return(aos::ErrorEnum::eNone)));

    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_RemovesCreatedDNSServerOnFailure)
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
    EXPECT_CALL(mDNSName, RemoveServer(networkID)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eInvalidArgument);

    EXPECT_CALL(mDNSName, RemoveServer(_)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    // The next start recreates the removed DNS server.
    EXPECT_CALL(mDNSName, CreateServer(networkID, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));

    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_KeepsDNSServerOfRunningInstanceOnFailure)
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
    params.mHostname                = "test-host-2";
    params.mInstanceIdent.mInstance = 1;

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID2, networkID, params), aos::ErrorEnum::eNone);

    // Only the first start creates the DNS server.
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);

    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID1, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mBridgeNetwork, Attach(instanceID2, _, _)).WillOnce(Return(aos::ErrorEnum::eInvalidArgument));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(instanceID2)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(instanceID2)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, RemoveServer(_)).Times(0);

    EXPECT_EQ(mNetManager->StartInstanceNetwork(instanceID2, networkID), aos::ErrorEnum::eInvalidArgument);

    EXPECT_CALL(mDNSName, RemoveServer(_)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));
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
    ExpectPersistInstanceCalls();

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(1)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();
    ExpectPersistInstanceCalls();
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
    ExpectPersistInstanceCalls(2);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID1, networkID), aos::ErrorEnum::eNone);
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID2, networkID), aos::ErrorEnum::eNone);

    // Stop instance1
    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID1)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();
    ExpectPersistInstanceCalls();
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
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, CreateServer(_, _))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));

    ExpectAddInstanceCalls(2);
    ExpectPersistInstanceCalls(3);

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

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
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, StopInstanceNetwork_FailOnFirewallRemoveError)
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
    ExpectPersistInstanceCalls();

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(1)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, RemoveHost(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, RemoveInstance(_)).WillOnce(Return(aos::ErrorEnum::eRuntime));
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

    EXPECT_EQ(mNetManager->StartInstanceNetwork(instanceID, "wrong-network"), aos::ErrorEnum::eInvalidArgument);
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
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

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
    ExpectPersistInstanceCalls(2);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID1, networkID), aos::ErrorEnum::eNone);
    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID2, networkID), aos::ErrorEnum::eNone);
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

    EXPECT_CALL(mStorage, GetNetworksInfo(_))
        .WillOnce(Invoke([this](aos::Array<aos::sm::networkmanager::NetworkInfo>& out) {
            out = mNetworkInfos;

            return aos::ErrorEnum::eNone;
        }));

    EXPECT_CALL(
        mNetIfFactory, CreateBridge(existingNetwork.mBridgeIfName, existingNetwork.mIP, existingNetwork.mSubnet))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(
        mNetIfFactory, CreateVlan(existingNetwork.mVlanIfName, existingNetwork.mVlanID, existingNetwork.mBridgeIfName))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, CreateServer(_, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mStorage, GetInstanceNetworksInfo(_))
        .WillOnce(Invoke([this](aos::Array<aos::sm::networkmanager::InstanceNetworkInfo>& out) {
            out = mInstanceNetworkInfos;

            return aos::ErrorEnum::eNone;
        }));

    mNetManager = std::make_unique<NetworkManager>();
    ASSERT_EQ(mNetManager->Init(mAllocator, mStorage, mBridgeNetwork, mFirewall, mBandwidth, mDNSName, mTrafficMonitor,
                  mNetns, mNetIf, mRandom, mNetIfFactory, mNetworkProvider, "test-node"),
        aos::ErrorEnum::eNone);

    const aos::String instanceID      = "test-instance";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, aos::String("network1"), aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, "network1", params), aos::ErrorEnum::eNone);

    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, "network1"), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, CreateNetwork_AdoptsExistingBridgeAndVlan)
{
    const auto        network    = CreateTestNetworkInfo();
    const aos::String instanceID = "test-instance";

    InitWithStoredNetwork(network);

    ExpectLinkExists(network.mBridgeIfName, LinkKindEnum::eBridge);
    ExpectLinkExists(network.mVlanIfName, LinkKindEnum::eVlan);

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).Times(0);
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).Times(0);
    EXPECT_CALL(mDNSName, CreateServer(_, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));

    ExpectStartInstanceOnStoredNetwork(instanceID, network.mNetworkID);

    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, network.mNetworkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, CreateNetwork_CreatesOnlyMissingVlan)
{
    const auto        network    = CreateTestNetworkInfo();
    const aos::String instanceID = "test-instance";

    InitWithStoredNetwork(network);

    ExpectLinkExists(network.mBridgeIfName, LinkKindEnum::eBridge);

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).Times(0);
    EXPECT_CALL(mNetIfFactory, CreateVlan(network.mVlanIfName, network.mVlanID, network.mBridgeIfName))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, CreateServer(_, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));

    ExpectStartInstanceOnStoredNetwork(instanceID, network.mNetworkID);

    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, network.mNetworkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, CreateNetwork_KeepsAdoptedBridgeWhenVlanCreationFails)
{
    const auto        network    = CreateTestNetworkInfo();
    const aos::String instanceID = "test-instance";

    InitWithStoredNetwork(network);

    ExpectLinkExists(network.mBridgeIfName, LinkKindEnum::eBridge);

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).Times(0);
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(0);

    ExpectStartInstanceOnStoredNetwork(instanceID, network.mNetworkID);

    EXPECT_FALSE(mNetManager->StartInstanceNetwork(instanceID, network.mNetworkID).IsNone());

    Mock::VerifyAndClearExpectations(&mNetIf);
    Mock::VerifyAndClearExpectations(&mNetIfFactory);
}

TEST_F(NetworkManagerTest, CreateInstanceNetwork_VerifyUpdateItemNetworkParams)
{
    const aos::String networkID       = "test-network";
    const aos::String instanceID      = "test-instance";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    params.mExposedPorts.PushBack("8080/tcp");
    params.mExposedPorts.PushBack("9090/udp");
    params.mExposedPorts.PushBack("7400/udp");
    params.mAllowedConnections.PushBack("service1:80/tcp");
    params.mAllowedConnections.PushBack("service2/7400:7650/udp");

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
    ASSERT_EQ(capturedServiceData.mExposedPorts.Size(), 3U);
    EXPECT_EQ(capturedServiceData.mExposedPorts[0], "8080/tcp");
    EXPECT_EQ(capturedServiceData.mExposedPorts[1], "9090/udp");
    EXPECT_EQ(capturedServiceData.mExposedPorts[2], "7400/udp");

    // Verify allowed connections
    ASSERT_EQ(capturedServiceData.mAllowedConnections.Size(), 2U);
    EXPECT_EQ(capturedServiceData.mAllowedConnections[0], "service1:80/tcp");
    EXPECT_EQ(capturedServiceData.mAllowedConnections[1], "service2/7400:7650/udp");

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

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_RunningInstance_CallsFirewallUpdate)
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
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    err = mNetManager->StartInstanceNetwork("test-instance", "test-network");
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

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_ReplacesRuleSet)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    // An allowed connection whose target was already up, so CM resolved it
    // while allocating the instance.
    aos::FirewallRule resolvedAtAllocation;
    resolvedAtAllocation.mDstIP   = "10.0.0.5";
    resolvedAtAllocation.mDstPort = "8080";
    resolvedAtAllocation.mProto   = "udp";
    resolvedAtAllocation.mSrcIP   = "192.168.1.2";
    allocatedParams.mFirewallRules.PushBack(resolvedAtAllocation);

    aos::FirewallRule movedTarget;
    movedTarget.mDstIP   = "10.0.0.8";
    movedTarget.mDstPort = "8080";
    movedTarget.mProto   = "udp";
    movedTarget.mSrcIP   = "192.168.1.2";
    allocatedParams.mFirewallRules.PushBack(movedTarget);

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork("test-instance", "test-network"), aos::ErrorEnum::eNone);

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;
    update.mFirewallRules.PushBack(resolvedAtAllocation);

    aos::FirewallRule resolvedLater;
    resolvedLater.mDstIP   = "10.0.0.6";
    resolvedLater.mDstPort = "8080";
    resolvedLater.mProto   = "udp";
    resolvedLater.mSrcIP   = "192.168.1.2";
    update.mFirewallRules.PushBack(resolvedLater);

    aos::sm::networkmanager::InstanceNetworkInfo stored;

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_))
        .WillOnce(DoAll(SaveArg<0>(&stored), Return(aos::ErrorEnum::eNone)));

    InstanceFirewallParams applied;

    EXPECT_CALL(mFirewall, UpdateInstance(_, _)).WillOnce(DoAll(SaveArg<1>(&applied), Return(aos::ErrorEnum::eNone)));

    mNetManager->OnPendingFirewallUpdate("test-node", update);

    ASSERT_EQ(applied.mOutput.Size(), 2U);
    EXPECT_TRUE(applied.mOutput[0].mDstIP == "10.0.0.5");
    EXPECT_TRUE(applied.mOutput[1].mDstIP == "10.0.0.6");
    EXPECT_EQ(stored.mAllocatedParams.mFirewallRules, update.mFirewallRules);
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_RemovesRulesOfReleasedTarget)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    aos::FirewallRule releasedTarget;
    releasedTarget.mDstIP   = "10.0.0.5";
    releasedTarget.mDstPort = "8080";
    releasedTarget.mProto   = "tcp";
    releasedTarget.mSrcIP   = "192.168.1.2";
    allocatedParams.mFirewallRules.PushBack(releasedTarget);

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork("test-instance", "test-network"), aos::ErrorEnum::eNone);

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;

    aos::sm::networkmanager::InstanceNetworkInfo stored;

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_))
        .WillOnce(DoAll(SaveArg<0>(&stored), Return(aos::ErrorEnum::eNone)));

    InstanceFirewallParams applied;

    EXPECT_CALL(mFirewall, UpdateInstance(_, _)).WillOnce(DoAll(SaveArg<1>(&applied), Return(aos::ErrorEnum::eNone)));

    mNetManager->OnPendingFirewallUpdate("test-node", update);

    EXPECT_TRUE(applied.mOutput.IsEmpty());
    EXPECT_TRUE(stored.mAllocatedParams.mFirewallRules.IsEmpty());
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_IgnoresUpdateForPreviousAddress)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;

    aos::FirewallRule rule;
    rule.mDstIP   = "10.0.0.5";
    rule.mDstPort = "8080";
    rule.mProto   = "tcp";
    rule.mSrcIP   = "192.168.2.7";
    update.mFirewallRules.PushBack(rule);

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(0);
    EXPECT_CALL(mFirewall, UpdateInstance(_, _)).Times(0);

    mNetManager->OnPendingFirewallUpdate("test-node", update);
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_ReappliesAfterFailedApply)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork("test-instance", "test-network"), aos::ErrorEnum::eNone);

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;

    aos::FirewallRule rule;
    rule.mDstIP   = "10.0.0.5";
    rule.mDstPort = "8080";
    rule.mProto   = "tcp";
    rule.mSrcIP   = "192.168.1.2";
    update.mFirewallRules.PushBack(rule);

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, UpdateInstance(_, _))
        .WillOnce(Return(aos::ErrorEnum::eFailed))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    mNetManager->OnPendingFirewallUpdate("test-node", update);
    mNetManager->OnPendingFirewallUpdate("test-node", update);
    mNetManager->OnPendingFirewallUpdate("test-node", update);
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_RetriesStoreAfterStorageFailure)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;

    aos::FirewallRule rule;
    rule.mDstIP   = "10.0.0.5";
    rule.mDstPort = "8080";
    rule.mProto   = "tcp";
    rule.mSrcIP   = "192.168.1.2";
    update.mFirewallRules.PushBack(rule);

    aos::sm::networkmanager::InstanceNetworkInfo stored;

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_))
        .WillOnce(Return(aos::ErrorEnum::eFailed))
        .WillOnce(DoAll(SaveArg<0>(&stored), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mFirewall, UpdateInstance(_, _)).Times(0);

    mNetManager->OnPendingFirewallUpdate("test-node", update);
    mNetManager->OnPendingFirewallUpdate("test-node", update);
    mNetManager->OnPendingFirewallUpdate("test-node", update);

    EXPECT_EQ(stored.mAllocatedParams.mFirewallRules, update.mFirewallRules);
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_ReplacesRulesAgainWhileUnapplied)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork("test-instance", "test-network"), aos::ErrorEnum::eNone);

    aos::networkmanager::PendingFirewallUpdate first;
    first.mInstanceIdent = params.mInstanceIdent;

    aos::FirewallRule rule;
    rule.mDstIP   = "10.0.0.5";
    rule.mDstPort = "8080";
    rule.mProto   = "tcp";
    rule.mSrcIP   = "192.168.1.2";
    first.mFirewallRules.PushBack(rule);

    auto second                     = first;
    second.mFirewallRules[0].mDstIP = "10.0.0.6";

    InstanceFirewallParams applied;

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, UpdateInstance(_, _))
        .WillOnce(Return(aos::ErrorEnum::eFailed))
        .WillOnce(Return(aos::ErrorEnum::eFailed))
        .WillOnce(DoAll(SaveArg<1>(&applied), Return(aos::ErrorEnum::eNone)));

    mNetManager->OnPendingFirewallUpdate("test-node", first);
    mNetManager->OnPendingFirewallUpdate("test-node", second);
    mNetManager->OnPendingFirewallUpdate("test-node", second);
    mNetManager->OnPendingFirewallUpdate("test-node", second);

    ASSERT_EQ(applied.mOutput.Size(), 1U);
    EXPECT_TRUE(applied.mOutput[0].mDstIP == "10.0.0.6");
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_SameRulesDoNotUpdateFirewall)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    aos::FirewallRule first;
    first.mDstIP   = "10.0.0.5";
    first.mDstPort = "8080";
    first.mProto   = "tcp";
    first.mSrcIP   = "192.168.1.2";
    allocatedParams.mFirewallRules.PushBack(first);

    aos::FirewallRule second = first;
    second.mDstIP            = "10.0.0.6";
    allocatedParams.mFirewallRules.PushBack(second);

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;
    update.mFirewallRules.PushBack(second);
    update.mFirewallRules.PushBack(first);

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(0);
    EXPECT_CALL(mFirewall, UpdateInstance(_, _)).Times(0);

    mNetManager->OnPendingFirewallUpdate("test-node", update);
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_AppliesRulesArrivedBeforeInstance)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;

    aos::FirewallRule resolvedWhileAllocating;
    resolvedWhileAllocating.mDstIP   = "10.0.0.7";
    resolvedWhileAllocating.mDstPort = "8080";
    resolvedWhileAllocating.mProto   = "udp";
    resolvedWhileAllocating.mSrcIP   = "192.168.1.2";
    update.mFirewallRules.PushBack(resolvedWhileAllocating);

    EXPECT_CALL(mFirewall, UpdateInstance(_, _)).Times(0);

    aos::networkmanager::PendingFirewallUpdate outdated = update;
    outdated.mFirewallRules[0].mDstIP                   = "10.0.0.6";

    mNetManager->OnPendingFirewallUpdate("test-node", outdated);
    mNetManager->OnPendingFirewallUpdate("test-node", update);

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    BridgeAttachResult attachResult;
    attachResult.mHostIfName      = "veth-test";
    attachResult.mContainerIfName = "eth0";

    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(attachResult), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mBandwidth, Apply(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, AddHost(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    InstanceFirewallParams applied;

    EXPECT_CALL(mFirewall, AddInstance(_, _)).WillOnce(DoAll(SaveArg<1>(&applied), Return(aos::ErrorEnum::eNone)));

    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StartInstanceNetwork("test-instance", "test-network"), aos::ErrorEnum::eNone);

    ASSERT_EQ(applied.mOutput.Size(), 1U);
    EXPECT_TRUE(applied.mOutput[0].mDstIP == "10.0.0.7");
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_FailedCreateDropsDeferredUpdate)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;

    aos::FirewallRule rule;
    rule.mDstIP   = "10.0.0.7";
    rule.mDstPort = "8080";
    rule.mProto   = "udp";
    rule.mSrcIP   = "192.168.1.2";
    update.mFirewallRules.PushBack(rule);

    mNetManager->OnPendingFirewallUpdate("test-node", update);

    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(aos::String("test-network"), aos::String("test-node"), _))
        .WillOnce(Return(aos::ErrorEnum::eFailed));

    ASSERT_NE(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    aos::sm::networkmanager::InstanceNetworkInfo stored;

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_))
        .WillOnce(DoAll(SaveArg<0>(&stored), Return(aos::ErrorEnum::eNone)));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    EXPECT_TRUE(stored.mAllocatedParams.mFirewallRules.IsEmpty());
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_DeferredRulesMergeWithoutDuplicates)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    aos::FirewallRule resolvedAtAllocation;
    resolvedAtAllocation.mDstIP   = "10.0.0.5";
    resolvedAtAllocation.mDstPort = "8080";
    resolvedAtAllocation.mProto   = "udp";
    resolvedAtAllocation.mSrcIP   = "192.168.1.2";
    allocatedParams.mFirewallRules.PushBack(resolvedAtAllocation);

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;
    update.mFirewallRules.PushBack(resolvedAtAllocation);

    aos::FirewallRule resolvedWhileAllocating = resolvedAtAllocation;
    resolvedWhileAllocating.mDstIP            = "10.0.0.7";
    update.mFirewallRules.PushBack(resolvedWhileAllocating);

    mNetManager->OnPendingFirewallUpdate("test-node", update);

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    aos::sm::networkmanager::InstanceNetworkInfo stored;

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_))
        .WillOnce(DoAll(SaveArg<0>(&stored), Return(aos::ErrorEnum::eNone)));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    ASSERT_EQ(stored.mAllocatedParams.mFirewallRules.Size(), 2U);
    EXPECT_EQ(stored.mAllocatedParams.mFirewallRules[0], resolvedAtAllocation);
    EXPECT_EQ(stored.mAllocatedParams.mFirewallRules[1], resolvedWhileAllocating);
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_DeferredRulesBeyondLimitAreDropped)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    aos::FirewallRule rule;
    rule.mDstPort = "8080";
    rule.mProto   = "udp";
    rule.mSrcIP   = "192.168.1.2";

    for (size_t i = 0; i < aos::cMaxNumFirewallRules; ++i) {
        const auto dstIP = "10.0.1." + std::to_string(i);

        rule.mDstIP = dstIP.c_str();
        allocatedParams.mFirewallRules.PushBack(rule);
    }

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;

    rule.mDstIP = "10.0.0.7";
    update.mFirewallRules.PushBack(rule);

    mNetManager->OnPendingFirewallUpdate("test-node", update);

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    aos::sm::networkmanager::InstanceNetworkInfo stored;

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_))
        .WillOnce(DoAll(SaveArg<0>(&stored), Return(aos::ErrorEnum::eNone)));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    EXPECT_EQ(stored.mAllocatedParams.mFirewallRules, allocatedParams.mFirewallRules);
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_DropsDeferredUpdateForPreviousAddress)
{
    auto params          = CreateTestInstanceNetworkConfig();
    auto allocatedParams = CreateTestAllocatedParams();

    aos::networkmanager::PendingFirewallUpdate update;
    update.mInstanceIdent = params.mInstanceIdent;

    aos::FirewallRule outdated;
    outdated.mDstIP   = "10.0.0.7";
    outdated.mDstPort = "8080";
    outdated.mProto   = "udp";
    outdated.mSrcIP   = "192.168.2.7";
    update.mFirewallRules.PushBack(outdated);

    mNetManager->OnPendingFirewallUpdate("test-node", update);

    SetupEnsureNodeNetworkCreateMocks("test-network", "192.168.1.0/24", "192.168.1.1", 100);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));

    aos::sm::networkmanager::InstanceNetworkInfo stored;

    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_))
        .WillOnce(DoAll(SaveArg<0>(&stored), Return(aos::ErrorEnum::eNone)));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("test-instance", "test-network", params), aos::ErrorEnum::eNone);

    EXPECT_TRUE(stored.mAllocatedParams.mFirewallRules.IsEmpty());
}

TEST_F(NetworkManagerTest, OnUnitNetworksUpdate_CreatesAnnouncedNetworkWithoutDNSServer)
{
    const aos::String networkID = "unit-network";

    SetupEnsureNodeNetworkCreateMocks(networkID, "192.168.1.0/24", "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, aos::String("192.168.1.1"), aos::String("192.168.1.0/24")))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, 100ULL, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, AddMasquerade(aos::String("192.168.1.0/24"), aos::String(cUplinkIfName)))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, CreateServer(_, _)).Times(0);

    UpdateUnitNetworks({"unit-network"});

    UpdateUnitNetworks({"unit-network"});
}

TEST_F(NetworkManagerTest, OnUnitNetworksUpdate_KnownNetworkDoesNotAskCM)
{
    const auto network = CreateTestNetworkInfo();

    InitWithStoredNetwork(network);

    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(_, _, _)).Times(0);
    EXPECT_CALL(mNetIfFactory, CreateBridge(network.mBridgeIfName, network.mIP, network.mSubnet))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(network.mVlanIfName, network.mVlanID, network.mBridgeIfName))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, CreateServer(_, _)).Times(0);

    UpdateUnitNetworks({"network1"});
}

TEST_F(NetworkManagerTest, OnUnitNetworksUpdate_RemovesDroppedNetworkWithoutInstances)
{
    const aos::String networkID = "unit-network";

    SetupEnsureNodeNetworkCreateMocks(networkID, "192.168.1.0/24", "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    UpdateUnitNetworks({"unit-network"});

    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, RemoveNetworkInfo(networkID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(networkID, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    UpdateUnitNetworks({});

    Mock::VerifyAndClearExpectations(&mNetIf);
    Mock::VerifyAndClearExpectations(&mNetworkProvider);

    SetupEnsureNodeNetworkCreateMocks(networkID, "192.168.1.0/24", "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetIf, GetLink(_, _)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNotFound));
    EXPECT_CALL(mNetIf, GetUplinkInterface(_))
        .Times(AnyNumber())
        .WillRepeatedly(DoAll(SetArgReferee<0>(aos::String(cUplinkIfName)), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    UpdateUnitNetworks({"unit-network"});
}

TEST_F(NetworkManagerTest, OnUnitNetworksUpdate_RemovesDroppedStoredNetwork)
{
    const auto network = CreateTestNetworkInfo();

    InitWithStoredNetwork(network);

    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(0);
    EXPECT_CALL(mStorage, RemoveNetworkInfo(network.mNetworkID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(network.mNetworkID, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    UpdateUnitNetworks({});

    Mock::VerifyAndClearExpectations(&mNetIf);
}

TEST_F(NetworkManagerTest, OnUnitNetworksUpdate_KeepsDroppedNetworkWithInstances)
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

    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(_, _)).Times(0);
    EXPECT_CALL(mStorage, RemoveNetworkInfo(_)).Times(0);

    UpdateUnitNetworks({});

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);
    ExpectStartInstanceCalls();

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(0);

    UpdateUnitNetworks({});

    Mock::VerifyAndClearExpectations(&mNetIf);
    Mock::VerifyAndClearExpectations(&mNetworkProvider);
    Mock::VerifyAndClearExpectations(&mStorage);

    EXPECT_CALL(mNetIf, GetLink(_, _)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNotFound));
    EXPECT_CALL(mNetIf, GetUplinkInterface(_))
        .Times(AnyNumber())
        .WillRepeatedly(DoAll(SetArgReferee<0>(aos::String(cUplinkIfName)), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();
    ExpectPersistInstanceCalls();
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

TEST_F(NetworkManagerTest, StopInstanceNetwork_UnitNetworkKeepsLinks)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    UpdateUnitNetworks({"test-network"});

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_CALL(mDNSName, CreateServer(networkID, _))
        .Times(2)
        .WillRepeatedly(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
    ExpectStartInstanceCalls();

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, RemoveServer(networkID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(0);
    EXPECT_CALL(mFirewall, RemoveMasquerade(_, _)).Times(0);

    EXPECT_EQ(mNetManager->StopInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mDNSName, RemoveServer(_)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, RemoveMasquerade(_, _)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    ExpectStartInstanceCalls();

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, ReleaseInstanceNetwork_KeepsUnitNetworkUntilDropped)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    UpdateUnitNetworks({"test-network"});

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_CALL(mDNSName, CreateServer(networkID, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
    ExpectStartInstanceCalls();

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(0);

    EXPECT_EQ(mNetManager->StopInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mStorage, RemoveInstanceNetworkInfo(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseInstanceNetwork(_, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, RemoveNetworkInfo(_)).Times(0);
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(_, _)).Times(0);

    EXPECT_EQ(mNetManager->ReleaseInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(_, _, _)).Times(0);

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    EXPECT_CALL(mStorage, RemoveInstanceNetworkInfo(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseInstanceNetwork(_, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->ReleaseInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, RemoveNetworkInfo(networkID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(networkID, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    UpdateUnitNetworks({});
}

TEST_F(NetworkManagerTest, OnUnitNetworksUpdate_KeepsNetworkOfInstanceBeingCreated)
{
    const aos::String instanceID      = "test-instance";
    const aos::String networkID       = "test-network";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(networkID, allocatedParams.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(_, _)).Times(0);
    EXPECT_CALL(mStorage, RemoveNetworkInfo(_)).Times(0);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, networkID, aos::String("test-node"), _, _))
        .WillOnce(Invoke([&](const aos::InstanceIdent&, const aos::String&, const aos::String&,
                             const aos::UpdateItemNetworkParams&, aos::InstanceNetworkAllocation& result) {
            UpdateUnitNetworks({});

            EXPECT_TRUE(mNetManager->StartInstanceNetwork(instanceID, networkID).Is(aos::ErrorEnum::eWrongState));
            EXPECT_TRUE(mNetManager->ReleaseInstanceNetwork(instanceID, networkID).Is(aos::ErrorEnum::eWrongState));

            result = allocatedParams;

            return aos::ErrorEnum::eNone;
        }));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(instanceID, networkID, params), aos::ErrorEnum::eNone);

    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocatedParams.mSubnet, 100ULL);
    ExpectStartInstanceCalls();

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, OnUnitNetworksUpdate_CreateWaitsForNodeNetworkRelease)
{
    const auto        network         = CreateTestNetworkInfo();
    const aos::String instanceID      = "test-instance";
    auto              params          = CreateTestInstanceNetworkConfig();
    auto              allocatedParams = CreateTestAllocatedParams();
    aos::Error        createErr       = aos::ErrorEnum::eFailed;
    std::thread       createThread;

    InitWithStoredNetwork(network);

    Sequence seq;

    EXPECT_CALL(mStorage, RemoveNetworkInfo(network.mNetworkID)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(network.mNetworkID, aos::String("test-node")))
        .InSequence(seq)
        .WillOnce(Invoke([&](const aos::String&, const aos::String&) {
            createThread = std::thread(
                [&]() { createErr = mNetManager->CreateInstanceNetwork(instanceID, network.mNetworkID, params); });

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            return aos::ErrorEnum::eNone;
        }));

    aos::NetworkParams netParams;
    netParams.mNetworkID = network.mNetworkID;
    netParams.mSubnet    = network.mSubnet;
    netParams.mIP        = network.mIP;
    netParams.mVlanID    = network.mVlanID;

    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(network.mNetworkID, aos::String("test-node"), _))
        .InSequence(seq)
        .WillOnce(DoAll(SetArgReferee<2>(netParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mRandom, RandBuffer(_, 4)).Times(2).WillRepeatedly(Invoke([](aos::Array<uint8_t>& buffer, size_t) {
        static uint8_t counter = 0;

        buffer.Resize(4);

        for (size_t i = 0; i < buffer.Size(); i++) {
            buffer[i] = static_cast<uint8_t>(counter + i);
        }

        counter += 4;

        return aos::ErrorEnum::eNone;
    }));
    EXPECT_CALL(mStorage, AddNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, network.mNetworkID, aos::String("test-node"), _, _))
        .InSequence(seq)
        .WillOnce(DoAll(SetArgReferee<4>(allocatedParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    UpdateUnitNetworks({});

    ASSERT_TRUE(createThread.joinable());
    createThread.join();

    EXPECT_EQ(createErr, aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, OnUnitNetworksUpdate_ContinuesAfterNetworkFailure)
{
    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(aos::String("failed-network"), aos::String("test-node"), _))
        .WillOnce(Return(aos::ErrorEnum::eFailed));

    SetupEnsureNodeNetworkCreateMocks("unit-network", "192.168.1.0/24", "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, aos::String("192.168.1.1"), aos::String("192.168.1.0/24")))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, 100ULL, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    UpdateUnitNetworks({"failed-network", "unit-network"});
}

TEST_F(NetworkManagerTest, OnUnitNetworksUpdate_IgnoresUpdateForDifferentNode)
{
    const auto network = CreateTestNetworkInfo();

    InitWithStoredNetwork(network);

    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(_, _, _)).Times(0);
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(_, _)).Times(0);
    EXPECT_CALL(mStorage, RemoveNetworkInfo(_)).Times(0);
    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).Times(0);

    aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumOwners> ids;

    ASSERT_TRUE(ids.PushBack("unit-network").IsNone());

    mNetManager->OnUnitNetworksUpdate("other-node", ids);
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
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {
            {"/var/run/netns/test-instance"}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

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

TEST_F(NetworkManagerTest, Start_CleansLeftoverInstanceWithMissingInterface)
{
    const auto network  = CreateTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network);

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    EXPECT_CALL(mDNSName, RemoveOrphans(_))
        .WillOnce(Invoke([&](const aos::Array<aos::StaticString<aos::cIDLen>>& known) {
            EXPECT_EQ(known.Size(), 1U);
            if (known.Size() == 1) {
                EXPECT_EQ(known[0], network.mNetworkID);
            }
            return aos::ErrorEnum::eNone;
        }));

    ExpectLeftoverInstanceCleaned();

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, Start_KeepsLeftoverInstanceWithLiveInterface)
{
    const auto network  = CreateTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network);

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    ExpectLinkExists(leftover.mHostIfName, LinkKindEnum::eVeth, network.mBridgeIfName);
    EXPECT_CALL(mNetns, IsNetworkNamespaceExist(leftover.mInstanceID))
        .WillRepeatedly(Return(aos::RetWithError<bool> {true, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ExpectLeftoverInstanceUntouched();

    EXPECT_CALL(mTrafficMonitor,
        StartInstanceMonitoring(leftover.mInstanceID, leftover.mAllocatedParams.mIP,
            leftover.mNetworkConfig.mDownloadLimit, leftover.mNetworkConfig.mUploadLimit))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);

    EXPECT_TRUE(
        mNetManager->StartInstanceNetwork(leftover.mInstanceID, network.mNetworkID).Is(aos::ErrorEnum::eAlreadyExist));
}

TEST_F(NetworkManagerTest, Start_AdoptsDNSServerForRunningInstanceCleanedOnStop)
{
    const auto network  = CreateTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network);

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    ExpectLinkExists(leftover.mHostIfName, LinkKindEnum::eVeth, network.mBridgeIfName);
    EXPECT_CALL(mNetns, IsNetworkNamespaceExist(leftover.mInstanceID))
        .WillRepeatedly(Return(aos::RetWithError<bool> {true, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    // Adopting the running instance must register the network DNS server.
    EXPECT_CALL(mDNSName, CreateServer(network.mNetworkID, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mTrafficMonitor,
        StartInstanceMonitoring(leftover.mInstanceID, leftover.mAllocatedParams.mIP,
            leftover.mNetworkConfig.mDownloadLimit, leftover.mNetworkConfig.mUploadLimit))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);

    // Stopping the adopted instance must reach the DNS server and drop its host entry.
    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, RemoveHost(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, RemoveInstance(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->StopInstanceNetwork(leftover.mInstanceID, network.mNetworkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, Start_CleansLeftoverInstanceWhenNamespaceMissing)
{
    const auto network  = CreateTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network);

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    ExpectLinkExists(leftover.mHostIfName, LinkKindEnum::eVeth, network.mBridgeIfName);
    EXPECT_CALL(mNetns, IsNetworkNamespaceExist(leftover.mInstanceID))
        .WillRepeatedly(Return(aos::RetWithError<bool> {false, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ExpectLeftoverInstanceCleaned();

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, Start_CleansLeftoverInstanceAttachedToForeignBridge)
{
    const auto network  = CreateTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network);

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    ExpectLinkExists(leftover.mHostIfName, LinkKindEnum::eVeth, "br-someoneelse");

    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ExpectLeftoverInstanceCleaned();

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, CreateNetwork_MasqueradesOnUplinkNotBridge)
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

    aos::StaticString<aos::cSubnetLen>    capturedSubnet;
    aos::StaticString<aos::cInterfaceLen> capturedOutIf;
    aos::StaticString<aos::cInterfaceLen> capturedBridge;

    EXPECT_CALL(mNetIfFactory, CreateBridge(_, aos::String("192.168.1.1"), allocatedParams.mSubnet))
        .WillOnce(Invoke([&](const aos::String& bridge, const aos::String&, const aos::String&) {
            capturedBridge = bridge;

            return aos::ErrorEnum::eNone;
        }));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, 100ULL, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, CreateServer(_, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));

    EXPECT_CALL(mFirewall, AddMasquerade(_, _))
        .WillOnce(Invoke([&](const aos::String& subnet, const aos::String& outIf) {
            capturedSubnet = subnet;
            capturedOutIf  = outIf;

            return aos::ErrorEnum::eNone;
        }));

    ExpectAddInstanceCalls();
    ExpectPersistInstanceCalls();

    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .Times(1)
        .WillRepeatedly(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, networkID), aos::ErrorEnum::eNone);

    EXPECT_EQ(capturedOutIf, aos::String(cUplinkIfName));
    EXPECT_EQ(capturedSubnet, allocatedParams.mSubnet);
    EXPECT_FALSE(capturedBridge.IsEmpty());
    EXPECT_NE(capturedOutIf, capturedBridge);
}

TEST_F(NetworkManagerTest, CreateNetwork_FailsWhenNoDefaultRoute)
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

    EXPECT_CALL(mNetIf, GetUplinkInterface(_)).WillRepeatedly(Return(aos::ErrorEnum::eNotFound));

    EXPECT_CALL(mFirewall, AddMasquerade(_, _)).Times(0);
    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).Times(0);
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).Times(0);

    EXPECT_FALSE(mNetManager->StartInstanceNetwork(instanceID, networkID).IsNone());
}

TEST_F(NetworkManagerTest, CreateNetwork_FailsWhenUplinkNameIsEmpty)
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

    EXPECT_CALL(mNetIf, GetUplinkInterface(_))
        .WillRepeatedly(DoAll(SetArgReferee<0>(aos::String("")), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mFirewall, AddMasquerade(_, _)).Times(0);
    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).Times(0);

    EXPECT_FALSE(mNetManager->StartInstanceNetwork(instanceID, networkID).IsNone());
}

TEST_F(NetworkManagerTest, Start_ReassertsMasqueradeOnCurrentUplink)
{
    const auto network = CreateTestNetworkInfo();

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    aos::StaticString<aos::cInterfaceLen> capturedOutIf;

    EXPECT_CALL(mFirewall, AddMasquerade(network.mSubnet, _))
        .Times(2)
        .WillRepeatedly(Invoke([&](const aos::String&, const aos::String& outIf) {
            EXPECT_EQ(outIf, aos::String(cUplinkIfName));

            capturedOutIf = outIf;

            return aos::ErrorEnum::eNone;
        }));

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);

    EXPECT_EQ(capturedOutIf, aos::String(cUplinkIfName));
}

TEST_F(NetworkManagerTest, Start_RestoresStoredNetworksWithoutDNSServer)
{
    const auto network1 = CreateTestNetworkInfo();
    const auto network2 = CreateSecondTestNetworkInfo();

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network1);
    networks.PushBack(network2);

    RestartWithStoredState(networks, *instances, false);

    EXPECT_CALL(mDNSName, RemoveOrphans(_))
        .WillOnce(Invoke([](const aos::Array<aos::StaticString<aos::cIDLen>>& known) {
            EXPECT_TRUE(known.IsEmpty());

            return aos::ErrorEnum::eNone;
        }));

    EXPECT_CALL(mFirewall, RemoveOrphans(_, _))
        .WillOnce(Invoke([&](const aos::Array<aos::StaticString<aos::cIDLen>>&,
                             const aos::Array<MasqueradeParams>& knownMasquerades) {
            EXPECT_EQ(knownMasquerades.Size(), 2U);
            EXPECT_TRUE(knownMasquerades.ContainsIf([&](const MasqueradeParams& params) {
                return params.mSubnet == network1.mSubnet && params.mOutIfName == cUplinkIfName;
            }));
            EXPECT_TRUE(knownMasquerades.ContainsIf([&](const MasqueradeParams& params) {
                return params.mSubnet == network2.mSubnet && params.mOutIfName == cUplinkIfName;
            }));

            return aos::ErrorEnum::eNone;
        }));
    EXPECT_CALL(mFirewall, AddMasquerade(network1.mSubnet, aos::String(cUplinkIfName)))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, AddMasquerade(network2.mSubnet, aos::String(cUplinkIfName)))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));

    ExpectStoredNetworkLinksCreated(network1);
    ExpectStoredNetworkLinksCreated(network2);
    EXPECT_CALL(mDNSName, CreateServer(_, _)).Times(0);
    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(_, _, _)).Times(0);

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, Start_ContinuesWhenStoredNetworkFails)
{
    const auto network1 = CreateTestNetworkInfo();
    const auto network2 = CreateSecondTestNetworkInfo();

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network1);
    networks.PushBack(network2);

    RestartWithStoredState(networks, *instances);

    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mNetIfFactory, CreateBridge(network1.mBridgeIfName, _, _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetIfFactory, CreateVlan(network1.mVlanIfName, _, _)).Times(0);
    ExpectStoredNetworkLinksCreated(network2);

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, Start_StoredNetworkIsKeptAsUnitNetwork)
{
    const auto        network    = CreateTestNetworkInfo();
    const aos::String instanceID = "test-instance";

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);

    RestartWithStoredState(networks, *instances);

    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectStoredNetworkLinksCreated(network);

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);

    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(_, _, _)).Times(0);

    ExpectStartInstanceOnStoredNetwork(instanceID, network.mNetworkID);

    EXPECT_CALL(mDNSName, CreateServer(network.mNetworkID, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
    ExpectStartInstanceCalls();

    ASSERT_EQ(mNetManager->StartInstanceNetwork(instanceID, network.mNetworkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(0);

    EXPECT_EQ(mNetManager->StopInstanceNetwork(instanceID, network.mNetworkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mStorage, RemoveInstanceNetworkInfo(instanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseInstanceNetwork(_, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, RemoveNetworkInfo(_)).Times(0);
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(_, _)).Times(0);

    EXPECT_EQ(mNetManager->ReleaseInstanceNetwork(instanceID, network.mNetworkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, RemoveNetworkInfo(network.mNetworkID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(network.mNetworkID, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    UpdateUnitNetworks({});
}

TEST_F(NetworkManagerTest, Start_RemovesDNSServerOfNetworkWithoutRunningInstances)
{
    const auto network1 = CreateTestNetworkInfo();
    const auto network2 = CreateSecondTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network1);

    auto alive                     = CreateLeftoverInstance(network2);
    alive.mInstanceID              = "alive-instance";
    alive.mNetworkConfig.mHostname = "alive-host";
    alive.mAllocatedParams.mIP     = "192.168.2.5";
    alive.mHostIfName              = "veth-alive";

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network1);
    networks.PushBack(network2);
    instances->PushBack(leftover);
    instances->PushBack(alive);

    RestartWithStoredState(networks, *instances);

    EXPECT_CALL(mDNSName, RemoveOrphans(_))
        .WillOnce(Invoke([&](const aos::Array<aos::StaticString<aos::cIDLen>>& known) {
            EXPECT_EQ(known.Size(), 2U);
            EXPECT_TRUE(known.Contains(network1.mNetworkID));
            EXPECT_TRUE(known.Contains(network2.mNetworkID));

            return aos::ErrorEnum::eNone;
        }));

    ExpectStoredNetworkLinksCreated(network1);
    ExpectStoredNetworkLinksCreated(network2);

    ExpectLinkExists(alive.mHostIfName, LinkKindEnum::eVeth, network2.mBridgeIfName);
    EXPECT_CALL(mNetns, IsNetworkNamespaceExist(alive.mInstanceID))
        .WillRepeatedly(Return(aos::RetWithError<bool> {true, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(alive.mInstanceID, _, _, _))
        .WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mDNSName, CreateServer(network1.mNetworkID, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mDNSName, CreateServer(network2.mNetworkID, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mDNSServer, RemoveHost(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, RemoveInstance(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_CALL(mDNSName, RemoveServer(network1.mNetworkID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, RemoveServer(network2.mNetworkID)).Times(0);

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);

    EXPECT_CALL(mDNSName, RemoveServer(_)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));
}

TEST_F(NetworkManagerTest, Start_FailsWhenNoDefaultRoute)
{
    const auto network = CreateTestNetworkInfo();

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);

    RestartWithStoredState(networks, *instances, false);

    EXPECT_CALL(mNetIf, GetUplinkInterface(_)).WillRepeatedly(Return(aos::ErrorEnum::eNotFound));

    EXPECT_CALL(mFirewall, RemoveOrphans(_, _)).Times(0);
    EXPECT_CALL(mFirewall, AddMasquerade(_, _)).Times(0);

    EXPECT_CALL(mFirewall, Stop()).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, Stop()).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_FALSE(mNetManager->Start().IsNone());
}

TEST_F(NetworkManagerTest, Init_FailsOnStorageErrors)
{
    auto netManager = std::make_unique<NetworkManager>();

    EXPECT_CALL(mStorage, GetInstanceNetworksInfo(_)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mStorage, GetNetworksInfo(_)).Times(0);

    EXPECT_TRUE(netManager
                    ->Init(mAllocator, mStorage, mBridgeNetwork, mFirewall, mBandwidth, mDNSName, mTrafficMonitor,
                        mNetns, mNetIf, mRandom, mNetIfFactory, mNetworkProvider, "test-node")
                    .Is(aos::ErrorEnum::eFailed));

    EXPECT_CALL(mStorage, GetInstanceNetworksInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, GetNetworksInfo(_)).WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_TRUE(netManager
                    ->Init(mAllocator, mStorage, mBridgeNetwork, mFirewall, mBandwidth, mDNSName, mTrafficMonitor,
                        mNetns, mNetIf, mRandom, mNetIfFactory, mNetworkProvider, "test-node")
                    .Is(aos::ErrorEnum::eRuntime));
}

TEST_F(NetworkManagerTest, Start_FailsOnFirewallStartError)
{
    EXPECT_CALL(mFirewall, Start()).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mTrafficMonitor, Start()).Times(0);

    EXPECT_TRUE(mNetManager->Start().Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, Start_StopsFirewallOnTrafficMonitorStartError)
{
    EXPECT_CALL(mFirewall, Start()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, Start()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mFirewall, Stop()).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mFirewall, RemoveOrphans(_, _)).Times(0);

    EXPECT_TRUE(mNetManager->Start().Is(aos::ErrorEnum::eRuntime));
}

TEST_F(NetworkManagerTest, Start_StopsBackendsOnFirewallOrphansError)
{
    EXPECT_CALL(mFirewall, Start()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, Start()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, RemoveOrphans(_, _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mTrafficMonitor, Stop()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mFirewall, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, RemoveOrphans(_)).Times(0);

    EXPECT_TRUE(mNetManager->Start().Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, Start_StopsBackendsOnDNSOrphansError)
{
    EXPECT_CALL(mFirewall, Start()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, Start()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, RemoveOrphans(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mTrafficMonitor, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_TRUE(mNetManager->Start().Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, Start_FailsOnReassertMasqueradeError)
{
    const auto network = CreateTestNetworkInfo();

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);

    RestartWithStoredState(networks, *instances);

    EXPECT_CALL(mFirewall, AddMasquerade(network.mSubnet, _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mDNSName, RemoveOrphans(_)).Times(0);
    EXPECT_CALL(mTrafficMonitor, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_TRUE(mNetManager->Start().Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, Stop_ReturnsFirstBackendError)
{
    EXPECT_CALL(mTrafficMonitor, Stop()).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mFirewall, Stop()).WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_TRUE(mNetManager->Stop().Is(aos::ErrorEnum::eFailed));

    EXPECT_CALL(mTrafficMonitor, Stop()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, Stop()).WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_TRUE(mNetManager->Stop().Is(aos::ErrorEnum::eRuntime));
}

TEST_F(NetworkManagerTest, SetTrafficPeriod_ForwardsToTrafficMonitor)
{
    EXPECT_CALL(mTrafficMonitor, SetPeriod(TrafficPeriod(TrafficPeriodEnum::eHourPeriod)));

    EXPECT_EQ(mNetManager->SetTrafficPeriod(TrafficPeriodEnum::eHourPeriod), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, CreateInstanceNetwork_RollsBackOnAllocateError)
{
    const auto params    = CreateTestInstanceNetworkConfig();
    const auto allocated = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(cTestNetworkID, allocated.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, aos::String(cTestNetworkID), _, _, _))
        .WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).Times(0);

    EXPECT_TRUE(
        mNetManager->CreateInstanceNetwork(cTestInstanceID, cTestNetworkID, params).Is(aos::ErrorEnum::eFailed));

    CreateTestInstance(cTestInstanceID, params, allocated, false);
}

TEST_F(NetworkManagerTest, CreateInstanceNetwork_ReleasesAllocationOnStorageError)
{
    const auto params    = CreateTestInstanceNetworkConfig();
    const auto allocated = CreateTestAllocatedParams();

    SetupEnsureNodeNetworkCreateMocks(cTestNetworkID, allocated.mSubnet, "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, aos::String(cTestNetworkID), _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(allocated), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetworkProvider, ReleaseInstanceNetwork(params.mInstanceIdent, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_TRUE(
        mNetManager->CreateInstanceNetwork(cTestInstanceID, cTestNetworkID, params).Is(aos::ErrorEnum::eFailed));

    aos::StaticArray<aos::Host, aos::cMaxNumHosts> hosts;

    EXPECT_TRUE(mNetManager->GetHosts(cTestInstanceID, hosts).Is(aos::ErrorEnum::eNotFound));
}

TEST_F(NetworkManagerTest, CreateInstanceNetwork_FailsOnAddNetworkInfoError)
{
    ExpectNodeNetworkParams(cTestNetworkID);

    EXPECT_CALL(mRandom, RandBuffer(_, _)).WillRepeatedly(Invoke(FillRandomBuffer));
    EXPECT_CALL(mStorage, AddNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _)).Times(0);

    EXPECT_TRUE(mNetManager->CreateInstanceNetwork(cTestInstanceID, cTestNetworkID, CreateTestInstanceNetworkConfig())
                    .Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, CreateInstanceNetwork_FailsOnIfNameGenerationError)
{
    const auto params = CreateTestInstanceNetworkConfig();

    EXPECT_CALL(mNetworkProvider, GetNodeNetworkParams(aos::String(cTestNetworkID), aos::String("test-node"), _))
        .Times(2)
        .WillRepeatedly(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mRandom, RandBuffer(_, _))
        .WillOnce(Return(aos::ErrorEnum::eFailed))
        .WillOnce(Invoke(FillRandomBuffer))
        .WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mStorage, AddNetworkInfo(_)).Times(0);

    EXPECT_TRUE(
        mNetManager->CreateInstanceNetwork(cTestInstanceID, cTestNetworkID, params).Is(aos::ErrorEnum::eFailed));

    EXPECT_TRUE(
        mNetManager->CreateInstanceNetwork(cTestInstanceID, cTestNetworkID, params).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, CreateInstanceNetwork_FailsWhenNoUniqueIfNameGenerated)
{
    const auto params = CreateTestInstanceNetworkConfig();

    // The random source always returns the same bytes, so the second network can't get unique interface names.
    EXPECT_CALL(mRandom, RandBuffer(_, _)).WillRepeatedly(Invoke(FillRandomBuffer));

    ExpectNodeNetworkParams("network-1");

    EXPECT_CALL(mStorage, AddNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, aos::String("network-1"), _, _, _))
        .WillOnce(DoAll(SetArgReferee<4>(CreateTestAllocatedParams()), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("instance-1", "network-1", params), aos::ErrorEnum::eNone);

    ExpectNodeNetworkParams("network-2");

    EXPECT_TRUE(mNetManager->CreateInstanceNetwork("instance-2", "network-2", params).Is(aos::ErrorEnum::eNotFound));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailsForUnknownInstance)
{
    EXPECT_TRUE(mNetManager->StartInstanceNetwork("unknown-instance", cTestNetworkID).Is(aos::ErrorEnum::eNotFound));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailsWhenNodeNetworkInfoMissing)
{
    const auto network  = CreateTestNetworkInfo();
    auto       leftover = CreateLeftoverInstance(network);

    leftover.mHostIfName.Clear();

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    instances->PushBack(leftover);

    RestartAndStart(networks, *instances);

    EXPECT_TRUE(
        mNetManager->StartInstanceNetwork(leftover.mInstanceID, network.mNetworkID).Is(aos::ErrorEnum::eNotFound));

    // The runtime cache was rolled back, so the second attempt fails the same way instead of eAlreadyExist.
    EXPECT_TRUE(
        mNetManager->StartInstanceNetwork(leftover.mInstanceID, network.mNetworkID).Is(aos::ErrorEnum::eNotFound));
}

TEST_F(NetworkManagerTest, CreateNetwork_FailsOnBridgeLookupError)
{
    const auto network = CreateTestNetworkInfo();

    InitWithStoredNetwork(network);
    ExpectStartInstanceOnStoredNetwork(cTestInstanceID, network.mNetworkID);

    EXPECT_CALL(mNetIf, GetLink(network.mBridgeIfName, _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, network.mNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, CreateNetwork_FailsOnBridgeCreateError)
{
    const auto network = CreateTestNetworkInfo();

    InitWithStoredNetwork(network);
    ExpectStartInstanceOnStoredNetwork(cTestInstanceID, network.mNetworkID);

    EXPECT_CALL(mNetIfFactory, CreateBridge(network.mBridgeIfName, _, _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, network.mNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, CreateNetwork_RemovesCreatedBridgeOnVlanLookupError)
{
    const auto network = CreateTestNetworkInfo();

    InitWithStoredNetwork(network);
    ExpectStartInstanceOnStoredNetwork(cTestInstanceID, network.mNetworkID);

    EXPECT_CALL(mNetIfFactory, CreateBridge(network.mBridgeIfName, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, GetLink(network.mVlanIfName, _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).Times(0);
    EXPECT_CALL(mNetIf, DeleteLink(network.mBridgeIfName)).WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, network.mNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, CreateNetwork_RemovesCreatedLinksOnMasqueradeError)
{
    const auto network = CreateTestNetworkInfo();

    InitWithStoredNetwork(network);
    ExpectStartInstanceOnStoredNetwork(cTestInstanceID, network.mNetworkID);

    EXPECT_CALL(mNetIfFactory, CreateBridge(network.mBridgeIfName, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIfFactory, CreateVlan(network.mVlanIfName, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, AddMasquerade(network.mSubnet, _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mDNSName, CreateServer(_, _)).Times(0);
    EXPECT_CALL(mNetIf, DeleteLink(network.mVlanIfName)).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mNetIf, DeleteLink(network.mBridgeIfName)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, network.mNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_KeepsMasqueradeOnDNSServerError)
{
    const auto network = CreateTestNetworkInfo();

    InitWithStoredNetwork(network);
    ExpectStartInstanceOnStoredNetwork(cTestInstanceID, network.mNetworkID);

    ExpectLinkExists(network.mBridgeIfName, LinkKindEnum::eBridge);
    ExpectLinkExists(network.mVlanIfName, LinkKindEnum::eVlan);

    // The DNS server is created per instance start, so its failure must not tear down the network.
    EXPECT_CALL(mDNSName, CreateServer(network.mNetworkID, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {nullptr, aos::ErrorEnum::eFailed}));
    EXPECT_CALL(mFirewall, RemoveMasquerade(network.mSubnet, _)).Times(0);
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(0);
    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, network.mNetworkID).Is(aos::ErrorEnum::eFailed));

    EXPECT_CALL(mFirewall, RemoveMasquerade(_, _)).Times(AnyNumber()).WillRepeatedly(Return(aos::ErrorEnum::eNone));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailsOnNetnsCreateError)
{
    CreateTestInstance();
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(aos::String(cTestInstanceID))).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).Times(0);
    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailsOnConflictingAliases)
{
    auto params = CreateTestInstanceNetworkConfig();

    params.mAliases.Clear();
    params.mAliases.PushBack("a.b.test-network");
    params.mAliases.PushBack("a.b");

    CreateTestInstance(cTestInstanceID, params, CreateTestAllocatedParams());
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eAlreadyExist));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailsOnHostnameUsedByAnotherInstance)
{
    CreateTestInstance();
    CreateTestInstance("test-instance-2", CreateTestInstanceNetworkConfig(), CreateTestAllocatedParams(), false);
    StartTestInstance();

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork("test-instance-2", cTestNetworkID).Is(aos::ErrorEnum::eAlreadyExist));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailsOnNetnsPathError)
{
    CreateTestInstance();
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);

    EXPECT_CALL(mNetns, CreateNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, GetNetworkNamespacePath(_))
        .WillOnce(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eFailed}));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_UsesPlainIPWhenSubnetHasNoMask)
{
    auto allocated = CreateTestAllocatedParams();

    allocated.mSubnet = "192.168.1.0";

    CreateTestInstance(cTestInstanceID, CreateTestInstanceNetworkConfig(), allocated);
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", allocated.mSubnet, 100ULL);

    BridgeParams capturedBridgeParams;

    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _))
        .WillOnce(DoAll(SaveArg<1>(&capturedBridgeParams), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mFirewall, AddInstance(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBandwidth, Apply(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, AddHost(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectPersistInstanceCalls();
    ExpectNetnsCreated();

    ASSERT_EQ(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID), aos::ErrorEnum::eNone);

    EXPECT_EQ(capturedBridgeParams.mIPWithMask, allocated.mIP);
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailsOnEmptyExposedPort)
{
    auto params = CreateTestInstanceNetworkConfig();

    params.mExposedPorts.PushBack("");

    CreateTestInstance(cTestInstanceID, params, CreateTestAllocatedParams());
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);
    ExpectNetnsCreated(true);
    ExpectBridgeAttached();

    EXPECT_CALL(mBridgeNetwork, Detach(aos::String(cTestInstanceID), _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mFirewall, AddInstance(_, _)).Times(0);

    EXPECT_TRUE(
        mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eInvalidArgument));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_FailsOnMalformedExposedPort)
{
    auto params = CreateTestInstanceNetworkConfig();

    params.mExposedPorts.PushBack("80/tcp/extra");

    CreateTestInstance(cTestInstanceID, params, CreateTestAllocatedParams());
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);
    ExpectNetnsCreated(true);
    ExpectBridgeAttached(true);

    EXPECT_CALL(mFirewall, AddInstance(_, _)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eNoMemory));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_DetachesBridgeOnFirewallError)
{
    CreateTestInstance();
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);
    ExpectNetnsCreated(true);
    ExpectBridgeAttached(true);

    EXPECT_CALL(mFirewall, AddInstance(aos::String(cTestInstanceID), _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mBandwidth, Apply(_, _)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_RemovesFirewallOnBandwidthError)
{
    CreateTestInstance();
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);
    ExpectNetnsCreated(true);
    ExpectBridgeAttached(true);

    EXPECT_CALL(mFirewall, AddInstance(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBandwidth, Apply(aos::String(cTestHostIfName), _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mFirewall, RemoveInstance(aos::String(cTestInstanceID))).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mDNSServer, AddHost(_, _)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_ClearsBandwidthOnDNSHostError)
{
    auto params = CreateTestInstanceNetworkConfig();

    params.mIngressKbit = 100;

    CreateTestInstance(cTestInstanceID, params, CreateTestAllocatedParams());
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);
    ExpectNetnsCreated(true);
    ExpectBridgeAttached(true);

    EXPECT_CALL(mFirewall, AddInstance(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBandwidth, Apply(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, AddHost(aos::String(cTestInstanceID), _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mBandwidth, Clear(aos::String(cTestHostIfName))).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mFirewall, RemoveInstance(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_RollsBackOnMonitoringErrorWhenDNSHostRemoveFails)
{
    CreateTestInstance();
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);
    ExpectNetnsCreated(true);
    ExpectBridgeAttached(true);

    EXPECT_CALL(mFirewall, AddInstance(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBandwidth, Apply(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, AddHost(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mDNSServer, RemoveHost(aos::String(cTestInstanceID))).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mFirewall, RemoveInstance(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBandwidth, Clear(_)).Times(0);

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StartInstanceNetwork_RollsBackOnPersistError)
{
    CreateTestInstance();
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);
    ExpectNetnsCreated(true);
    ExpectBridgeAttached(true);

    EXPECT_CALL(mFirewall, AddInstance(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBandwidth, Apply(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, AddHost(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(aos::String(cTestInstanceID)))
        .WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mDNSServer, RemoveHost(aos::String(cTestInstanceID))).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, RemoveInstance(_)).WillOnce(Return(aos::ErrorEnum::eNone));

    EXPECT_TRUE(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StopInstanceNetwork_KeepsPhysicalNetworkWhenClearFails)
{
    auto params = CreateTestInstanceNetworkConfig();

    params.mIngressKbit = 100;

    // A network requested from CM is not a unit network, so stopping its last instance clears it.
    CreateTestInstance(cTestInstanceID, params, CreateTestAllocatedParams());
    StartTestInstance();

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(aos::String(cTestInstanceID)))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, RemoveHost(aos::String(cTestInstanceID))).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBandwidth, Clear(aos::String(cTestHostIfName))).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mFirewall, RemoveInstance(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(0);
    EXPECT_CALL(mNetIf, DeleteLink(_))
        .WillOnce(Return(aos::ErrorEnum::eNone))
        .WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_TRUE(mNetManager->StopInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));

    // The physical network is kept as its clear failed, and the next start recreates only the removed DNS server.
    EXPECT_CALL(mNetIfFactory, CreateBridge(_, _, _)).Times(0);
    EXPECT_CALL(mNetIfFactory, CreateVlan(_, _, _)).Times(0);
    EXPECT_CALL(mDNSName, CreateServer(aos::String(cTestNetworkID), _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));

    StartTestInstance(cTestInstanceID, false);
}

TEST_F(NetworkManagerTest, Destructor_ClearsNetworkDespiteDNSServerRemoveError)
{
    CreateTestInstance();
    StartTestInstance();

    EXPECT_CALL(mDNSName, RemoveServer(aos::String(cTestNetworkID))).WillOnce(Return(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StopInstanceNetwork_FailsOnNetworkIDMismatch)
{
    CreateTestInstance();

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(_)).Times(0);

    EXPECT_TRUE(
        mNetManager->StopInstanceNetwork(cTestInstanceID, "wrong-network").Is(aos::ErrorEnum::eInvalidArgument));
}

TEST_F(NetworkManagerTest, StopInstanceNetwork_IgnoresNotStartedInstance)
{
    CreateTestInstance();

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(_)).Times(0);
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).Times(0);

    EXPECT_EQ(mNetManager->StopInstanceNetwork(cTestInstanceID, cTestNetworkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, StopInstanceNetwork_ReturnsMonitoringError)
{
    CreateTestInstance();
    StartTestInstance();

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(aos::String(cTestInstanceID)))
        .WillOnce(Return(aos::ErrorEnum::eFailed));
    ExpectDeleteInstanceCalls();
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_TRUE(mNetManager->StopInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StopInstanceNetwork_ReturnsDNSHostRemoveError)
{
    CreateTestInstance();
    StartTestInstance();

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, RemoveHost(aos::String(cTestInstanceID))).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mFirewall, RemoveInstance(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(0);
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_TRUE(mNetManager->StopInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StopInstanceNetwork_ReturnsNamespaceDeleteError)
{
    CreateTestInstance();
    StartTestInstance();

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(aos::String(cTestInstanceID))).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(0);
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_TRUE(mNetManager->StopInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StopInstanceNetwork_ReturnsPersistError)
{
    CreateTestInstance();
    StartTestInstance();

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_TRUE(mNetManager->StopInstanceNetwork(cTestInstanceID, cTestNetworkID).Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, StopInstanceNetwork_SkipsInterfaceCleanupWithoutHostInterface)
{
    CreateTestInstance();
    SetupEnsureNodeNetworkPhysicalMocks("192.168.1.1", "192.168.1.0/24", 100ULL);
    ExpectNetnsCreated();

    EXPECT_CALL(mBridgeNetwork, Attach(_, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, AddInstance(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mBandwidth, Apply(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, AddHost(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectPersistInstanceCalls();

    ASSERT_EQ(mNetManager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, RemoveHost(_)).Times(0);
    EXPECT_CALL(mFirewall, RemoveInstance(_)).Times(0);
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(0);
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    EXPECT_EQ(mNetManager->StopInstanceNetwork(cTestInstanceID, cTestNetworkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, ReleaseInstanceNetwork_IgnoresBackendErrors)
{
    const auto params = CreateTestInstanceNetworkConfig();

    CreateTestInstance();

    EXPECT_CALL(mStorage, RemoveInstanceNetworkInfo(aos::String(cTestInstanceID)))
        .WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetworkProvider, ReleaseInstanceNetwork(params.mInstanceIdent, aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mStorage, RemoveNetworkInfo(aos::String(cTestNetworkID))).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(aos::String(cTestNetworkID), aos::String("test-node")))
        .WillOnce(Return(aos::ErrorEnum::eFailed));

    EXPECT_EQ(mNetManager->ReleaseInstanceNetwork(cTestInstanceID, cTestNetworkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mStorage, RemoveInstanceNetworkInfo(aos::String("unknown-instance")))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, RemoveNetworkInfo(_)).Times(0);
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(_, _)).Times(0);

    EXPECT_EQ(mNetManager->ReleaseInstanceNetwork("unknown-instance", "unknown-network"), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, BeginBatch_FailsOnTransactionError)
{
    EXPECT_CALL(mStorage, BeginTransaction()).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mFirewall, BeginBatch()).Times(0);

    EXPECT_TRUE(mNetManager->BeginBatch().Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, BeginBatch_AbortsFirewallBatchOnTrafficMonitorError)
{
    EXPECT_CALL(mStorage, BeginTransaction()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mFirewall, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, BeginBatch()).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mFirewall, AbortBatch()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mStorage, RollbackTransaction()).WillOnce(Return(aos::ErrorEnum::eRuntime));

    EXPECT_TRUE(mNetManager->BeginBatch().Is(aos::ErrorEnum::eFailed));
}

TEST_F(NetworkManagerTest, FlushBatch_FirewallFailure_IgnoresAbortAndRollbackErrors)
{
    BeginTestBatch();

    EXPECT_CALL(mFirewall, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mTrafficMonitor, AbortBatch()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mStorage, RollbackTransaction()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mTrafficMonitor, FlushBatch()).Times(0);

    aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumInstances> failed;

    EXPECT_EQ(mNetManager->FlushBatch(failed), aos::ErrorEnum::eNone);
    EXPECT_TRUE(failed.IsEmpty());
}

TEST_F(NetworkManagerTest, FlushBatch_TrafficFailure_RevertsFirewallAndRollsBack)
{
    BeginTestBatch();

    EXPECT_CALL(mFirewall, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mFirewall, Revert()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mStorage, RollbackTransaction()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mStorage, CommitTransaction()).Times(0);

    aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumInstances> failed;

    EXPECT_EQ(mNetManager->FlushBatch(failed), aos::ErrorEnum::eNone);
    EXPECT_TRUE(failed.IsEmpty());
}

TEST_F(NetworkManagerTest, FlushBatch_CommitFailure_IgnoresRevertErrorsAndFailedListOverflow)
{
    CreateTestInstance();
    CreateTestInstance("test-instance-2", CreateSecondInstanceNetworkConfig(), CreateTestAllocatedParams(), false);
    BeginTestBatch();
    StartTestInstance();
    StartTestInstance("test-instance-2", false);

    EXPECT_CALL(mFirewall, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, FlushBatch()).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, CommitTransaction()).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mFirewall, Revert()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mTrafficMonitor, Revert()).WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mStorage, RollbackTransaction()).WillOnce(Return(aos::ErrorEnum::eRuntime));

    aos::StaticArray<aos::StaticString<aos::cIDLen>, 1> failed;

    EXPECT_EQ(mNetManager->FlushBatch(failed), aos::ErrorEnum::eNone);
    ASSERT_EQ(failed.Size(), 1U);
    EXPECT_EQ(failed[0], cTestInstanceID);
}

TEST_F(NetworkManagerTest, FlushBatch_FirewallFailure_ReappliesStoppedAndReleasedInstance)
{
    CreateTestInstance();
    BeginTestBatch();
    StartTestInstance();

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(aos::String(cTestInstanceID)))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    ExpectDeleteInstanceCalls();
    ExpectPersistInstanceCalls();
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(2).WillRepeatedly(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->StopInstanceNetwork(cTestInstanceID, cTestNetworkID), aos::ErrorEnum::eNone);

    EXPECT_CALL(mStorage, RemoveInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseInstanceNetwork(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, RemoveNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetworkProvider, ReleaseNodeNetwork(_, _)).WillOnce(Return(aos::ErrorEnum::eNone));

    ASSERT_EQ(mNetManager->ReleaseInstanceNetwork(cTestInstanceID, cTestNetworkID), aos::ErrorEnum::eNone);

    ExpectFirewallFlushFailure();

    // The add entry can't be re-applied as the instance is released, the remove entry fails on monitoring.
    EXPECT_CALL(mFirewall, AddInstance(_, _)).Times(0);
    EXPECT_CALL(mFirewall, RemoveInstance(aos::String(cTestInstanceID))).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(aos::String(cTestInstanceID)))
        .WillOnce(Return(aos::ErrorEnum::eFailed));

    aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumInstances> failed;

    EXPECT_EQ(mNetManager->FlushBatch(failed), aos::ErrorEnum::eNone);
    ASSERT_EQ(failed.Size(), 2U);
    EXPECT_EQ(failed[0], cTestInstanceID);
    EXPECT_EQ(failed[1], cTestInstanceID);
}

TEST_F(NetworkManagerTest, FlushBatch_FirewallFailure_ReportsFirewallReapplyError)
{
    CreateTestInstance();
    BeginTestBatch();
    StartTestInstance();

    ExpectFirewallFlushFailure();

    EXPECT_CALL(mFirewall, AddInstance(aos::String(cTestInstanceID), _)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).Times(0);

    aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumInstances> failed;

    EXPECT_EQ(mNetManager->FlushBatch(failed), aos::ErrorEnum::eNone);
    ASSERT_EQ(failed.Size(), 1U);
    EXPECT_EQ(failed[0], cTestInstanceID);
}

TEST_F(NetworkManagerTest, FlushBatch_FirewallFailure_RollsBackReapplyOnPersistError)
{
    CreateTestInstance();
    BeginTestBatch();
    StartTestInstance();

    ExpectFirewallFlushFailure();

    EXPECT_CALL(mFirewall, AddInstance(aos::String(cTestInstanceID), _)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(aos::String(cTestInstanceID), _, _, _))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(aos::String(cTestInstanceID)))
        .WillOnce(Return(aos::ErrorEnum::eRuntime));
    EXPECT_CALL(mFirewall, RemoveInstance(aos::String(cTestInstanceID))).WillOnce(Return(aos::ErrorEnum::eRuntime));

    aos::StaticArray<aos::StaticString<aos::cIDLen>, aos::cMaxNumInstances> failed;

    EXPECT_EQ(mNetManager->FlushBatch(failed), aos::ErrorEnum::eNone);
    ASSERT_EQ(failed.Size(), 1U);
    EXPECT_EQ(failed[0], cTestInstanceID);
}

TEST_F(NetworkManagerTest, GetResolvServers_FailsForUnknownInstanceOrSmallOutput)
{
    aos::StaticArray<aos::StaticString<aos::cIPLen>, aos::cMaxNumDNSServers + 1> servers;

    EXPECT_TRUE(mNetManager->GetResolvServers("unknown-instance", servers).Is(aos::ErrorEnum::eNotFound));

    CreateTestInstance();

    aos::Array<aos::StaticString<aos::cIPLen>> noServers(
        static_cast<const aos::StaticString<aos::cIPLen>*>(nullptr), 0);

    EXPECT_TRUE(mNetManager->GetResolvServers(cTestInstanceID, noServers).Is(aos::ErrorEnum::eNoMemory));

    aos::StaticArray<aos::StaticString<aos::cIPLen>, 1> oneServer;

    EXPECT_TRUE(mNetManager->GetResolvServers(cTestInstanceID, oneServer).Is(aos::ErrorEnum::eNoMemory));
    ASSERT_EQ(oneServer.Size(), 1U);
    EXPECT_EQ(oneServer[0], "192.168.1.1");
}

TEST_F(NetworkManagerTest, GetResolvServers_FallsBackToPublicDNS)
{
    const auto network  = CreateTestNetworkInfo();
    auto       leftover = CreateLeftoverInstance(network);

    leftover.mHostIfName.Clear();

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    instances->PushBack(leftover);

    RestartAndStart(networks, *instances);

    aos::Array<aos::StaticString<aos::cIPLen>> noServers(
        static_cast<const aos::StaticString<aos::cIPLen>*>(nullptr), 0);

    EXPECT_TRUE(mNetManager->GetResolvServers(leftover.mInstanceID, noServers).Is(aos::ErrorEnum::eNoMemory));

    aos::StaticArray<aos::StaticString<aos::cIPLen>, 2> servers;

    ASSERT_EQ(mNetManager->GetResolvServers(leftover.mInstanceID, servers), aos::ErrorEnum::eNone);
    ASSERT_EQ(servers.Size(), 1U);
    EXPECT_EQ(servers[0], "8.8.8.8");
}

TEST_F(NetworkManagerTest, GetHosts_FailsForUnknownInstanceOrSmallOutput)
{
    aos::StaticArray<aos::Host, aos::cMaxNumHosts> hosts;

    EXPECT_TRUE(mNetManager->GetHosts("unknown-instance", hosts).Is(aos::ErrorEnum::eNotFound));

    CreateTestInstance();

    aos::Array<aos::Host> noHosts(static_cast<const aos::Host*>(nullptr), 0);

    EXPECT_TRUE(mNetManager->GetHosts(cTestInstanceID, noHosts).Is(aos::ErrorEnum::eNoMemory));

    aos::StaticArray<aos::Host, 1> oneHost;

    EXPECT_TRUE(mNetManager->GetHosts(cTestInstanceID, oneHost).Is(aos::ErrorEnum::eNoMemory));
    EXPECT_EQ(oneHost.Size(), 1U);

    aos::StaticArray<aos::Host, 2> twoHosts;

    EXPECT_TRUE(mNetManager->GetHosts(cTestInstanceID, twoHosts).Is(aos::ErrorEnum::eNoMemory));
    EXPECT_EQ(twoHosts.Size(), 2U);

    aos::StaticArray<aos::Host, 3> threeHosts;

    EXPECT_TRUE(mNetManager->GetHosts(cTestInstanceID, threeHosts).Is(aos::ErrorEnum::eNoMemory));
    EXPECT_EQ(threeHosts.Size(), 3U);
}

TEST_F(NetworkManagerTest, Start_CleansLeftoverInstanceWhenLinkLookupFails)
{
    const auto network  = CreateTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network);

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    EXPECT_CALL(mNetIf, GetLink(leftover.mHostIfName, _)).WillRepeatedly(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).Times(0);

    ExpectLeftoverInstanceCleaned();

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, Start_CleansLeftoverInstanceWhenNamespaceCheckFails)
{
    const auto network  = CreateTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network);

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    ExpectLinkExists(leftover.mHostIfName, LinkKindEnum::eVeth, network.mBridgeIfName);
    EXPECT_CALL(mNetns, IsNetworkNamespaceExist(leftover.mInstanceID))
        .WillRepeatedly(Return(aos::RetWithError<bool> {true, aos::ErrorEnum::eFailed}));
    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).Times(0);

    ExpectLeftoverInstanceCleaned();

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, Start_CleansLeftoverInstanceWhenAdoptMonitoringFails)
{
    const auto network  = CreateTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network);

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    ExpectLinkExists(leftover.mHostIfName, LinkKindEnum::eVeth, network.mBridgeIfName);
    EXPECT_CALL(mNetns, IsNetworkNamespaceExist(leftover.mInstanceID))
        .WillRepeatedly(Return(aos::RetWithError<bool> {true, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(leftover.mInstanceID, _, _, _))
        .WillOnce(Return(aos::ErrorEnum::eFailed));

    ExpectLeftoverInstanceCleaned();

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);

    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(_)).Times(0);

    EXPECT_EQ(mNetManager->StopInstanceNetwork(leftover.mInstanceID, network.mNetworkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, Start_CleansLeftoverInstanceWithConflictingAliases)
{
    const auto network  = CreateTestNetworkInfo();
    auto       leftover = CreateLeftoverInstance(network);

    leftover.mNetworkConfig.mAliases.PushBack("a.b.network1");
    leftover.mNetworkConfig.mAliases.PushBack("a.b");

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    ExpectLinkExists(leftover.mHostIfName, LinkKindEnum::eVeth, network.mBridgeIfName);
    EXPECT_CALL(mNetns, IsNetworkNamespaceExist(leftover.mInstanceID))
        .WillRepeatedly(Return(aos::RetWithError<bool> {true, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).Times(0);

    ExpectLeftoverInstanceCleaned();

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, Start_KeepsRunningInstanceWhenDNSServerAdoptionFails)
{
    const auto network  = CreateTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network);

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    networks.PushBack(network);
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);
    ExpectStoredNetworkLinksCreated(network);

    ExpectLinkExists(leftover.mHostIfName, LinkKindEnum::eVeth, network.mBridgeIfName);
    EXPECT_CALL(mNetns, IsNetworkNamespaceExist(leftover.mInstanceID))
        .WillRepeatedly(Return(aos::RetWithError<bool> {true, aos::ErrorEnum::eNone}));
    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(leftover.mInstanceID, _, _, _))
        .WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, CreateServer(network.mNetworkID, _))
        .WillOnce(Return(aos::RetWithError<DNSServerItf*> {nullptr, aos::ErrorEnum::eFailed}));

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);

    // The instance is running, but without the DNS server its host entry can't be removed on stop.
    EXPECT_CALL(mTrafficMonitor, StopInstanceMonitoring(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSServer, RemoveHost(_)).Times(0);
    EXPECT_CALL(mFirewall, RemoveInstance(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    // The stored network is a unit network, so its links are kept after the last instance stops.
    EXPECT_CALL(mNetIf, DeleteLink(_)).Times(0);

    EXPECT_EQ(mNetManager->StopInstanceNetwork(leftover.mInstanceID, network.mNetworkID), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, Start_CleansLeftoverInstanceOfUnknownNetwork)
{
    const auto network  = CreateTestNetworkInfo();
    const auto leftover = CreateLeftoverInstance(network);

    aos::StaticArray<aos::sm::networkmanager::NetworkInfo, aos::cMaxNumOwners> networks;
    auto                                                                       instances
        = std::make_unique<aos::StaticArray<aos::sm::networkmanager::InstanceNetworkInfo, aos::cMaxNumInstances>>();
    instances->PushBack(leftover);

    RestartWithStoredState(networks, *instances);

    EXPECT_CALL(mDNSName, RemoveOrphans(_)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mDNSName, CreateServer(_, _)).Times(0);
    EXPECT_CALL(mTrafficMonitor, StartInstanceMonitoring(_, _, _, _)).Times(0);
    EXPECT_CALL(mDNSServer, RemoveHost(_)).Times(0);
    EXPECT_CALL(mFirewall, RemoveInstance(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eFailed));
    EXPECT_CALL(mNetns, DeleteNetworkNamespace(leftover.mInstanceID)).WillOnce(Return(aos::ErrorEnum::eNone));
    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(0);

    ASSERT_EQ(mNetManager->Start(), aos::ErrorEnum::eNone);
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_IgnoresUpdateForOtherNode)
{
    const auto params = CreateTestInstanceNetworkConfig();

    CreateTestInstance();

    aos::networkmanager::PendingFirewallUpdate update;

    update.mInstanceIdent = params.mInstanceIdent;

    aos::FirewallRule rule;

    rule.mDstIP   = "10.0.0.5";
    rule.mDstPort = "8080";
    rule.mProto   = "tcp";
    rule.mSrcIP   = "192.168.1.2";
    update.mFirewallRules.PushBack(rule);

    EXPECT_CALL(mStorage, UpdateInstanceNetworkInfo(_)).Times(0);
    EXPECT_CALL(mFirewall, UpdateInstance(_, _)).Times(0);

    mNetManager->OnPendingFirewallUpdate("other-node", update);
}

TEST_F(NetworkManagerTest, OnPendingFirewallUpdate_KeepsDeferredUpdateOfOtherInstance)
{
    const auto params      = CreateTestInstanceNetworkConfig();
    auto       otherParams = CreateSecondInstanceNetworkConfig();

    otherParams.mInstanceIdent.mItemID = "other-item";

    aos::networkmanager::PendingFirewallUpdate update;

    update.mInstanceIdent = otherParams.mInstanceIdent;

    aos::FirewallRule rule;

    rule.mDstIP   = "10.0.0.5";
    rule.mDstPort = "8080";
    rule.mProto   = "tcp";
    rule.mSrcIP   = "192.168.1.2";
    update.mFirewallRules.PushBack(rule);

    mNetManager->OnPendingFirewallUpdate("test-node", update);

    auto stored = std::make_unique<InstanceNetworkInfo>();

    SetupEnsureNodeNetworkCreateMocks(cTestNetworkID, "192.168.1.0/24", "192.168.1.1", 100ULL);

    EXPECT_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgReferee<4>(CreateTestAllocatedParams()), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddInstanceNetworkInfo(_))
        .Times(2)
        .WillRepeatedly(DoAll(SaveArg<0>(stored.get()), Return(aos::ErrorEnum::eNone)));

    ASSERT_EQ(mNetManager->CreateInstanceNetwork(cTestInstanceID, cTestNetworkID, params), aos::ErrorEnum::eNone);
    EXPECT_TRUE(stored->mAllocatedParams.mFirewallRules.IsEmpty());

    ASSERT_EQ(mNetManager->CreateInstanceNetwork("other-instance", cTestNetworkID, otherParams), aos::ErrorEnum::eNone);
    ASSERT_EQ(stored->mAllocatedParams.mFirewallRules.Size(), 1U);
    EXPECT_EQ(stored->mAllocatedParams.mFirewallRules[0], rule);
}

TEST_F(NetworkManagerTest, OnConnect_SkipsStoppedInstancesAndIgnoresSyncError)
{
    CreateTestInstance();

    EXPECT_CALL(mNetworkProvider, SyncNetworkState(aos::String("test-node"), _))
        .WillOnce(Invoke([](const aos::String&, const aos::Array<aos::InstanceNetworkStateInfo>& instances) {
            EXPECT_TRUE(instances.IsEmpty());

            return aos::ErrorEnum::eFailed;
        }));

    mNetManager->OnConnect();
}

/***********************************************************************************************************************
 * Allocation failure tests
 **********************************************************************************************************************/

namespace {
class FailingAllocator : public aos::AllocatorItf {
public:
    void Arm(size_t failAt)
    {
        mFailAt    = failAt;
        mCount     = 0;
        mTriggered = false;
    }

    void Disarm() { mFailAt = SIZE_MAX; }

    bool IsTriggered() const { return mTriggered; }

    void* Allocate(size_t size) override
    {
        if (mCount++ == mFailAt) {
            mTriggered = true;

            return nullptr;
        }

        return mHeap.Allocate(size);
    }

    void Free(void* data) override { mHeap.Free(data); }

private:
    aos::HeapAllocator mHeap;
    size_t             mFailAt {SIZE_MAX};
    size_t             mCount {};
    bool               mTriggered {};
};

} // namespace

class NetworkManagerAllocFailureTest : public Test {
protected:
    void SetUp() override
    {
        aos::tests::utils::InitLog();

        ON_CALL(mNetIf, GetUplinkInterface(_))
            .WillByDefault(DoAll(SetArgReferee<0>(aos::String(cUplinkIfName)), Return(aos::ErrorEnum::eNone)));
        ON_CALL(mNetIf, GetLink(_, _)).WillByDefault(Return(aos::ErrorEnum::eNotFound));
        ON_CALL(mNetIf, DeleteLink(_)).WillByDefault(Return(aos::ErrorEnum::eNone));
        ON_CALL(mDNSName, CreateServer(_, _))
            .WillByDefault(Return(aos::RetWithError<DNSServerItf*> {&mDNSServer, aos::ErrorEnum::eNone}));
        ON_CALL(mRandom, RandBuffer(_, _)).WillByDefault(Invoke([](aos::Array<uint8_t>& buffer, size_t size) {
            buffer.Resize(size);

            for (auto& item : buffer) {
                item = 0x42;
            }

            return aos::ErrorEnum::eNone;
        }));
        ON_CALL(mNetworkProvider, GetNodeNetworkParams(_, _, _))
            .WillByDefault(Invoke([](const aos::String& networkID, const aos::String&, aos::NetworkParams& params) {
                params.mNetworkID = networkID;
                params.mSubnet    = "192.168.1.0/24";
                params.mIP        = "192.168.1.1";
                params.mVlanID    = 100ULL;

                return aos::ErrorEnum::eNone;
            }));
        ON_CALL(mNetworkProvider, AllocateInstanceNetwork(_, _, _, _, _))
            .WillByDefault(DoAll(SetArgReferee<4>(CreateAllocatedParams()), Return(aos::ErrorEnum::eNone)));
        ON_CALL(mNetns, GetNetworkNamespacePath(_))
            .WillByDefault(Return(aos::RetWithError<aos::StaticString<aos::cFilePathLen>> {{}, aos::ErrorEnum::eNone}));
        ON_CALL(mNetns, IsNetworkNamespaceExist(_)).WillByDefault(Return(aos::RetWithError<bool> {true}));

        BridgeAttachResult attachResult;

        attachResult.mHostIfName      = cTestHostIfName;
        attachResult.mContainerIfName = "eth0";

        ON_CALL(mBridgeNetwork, Attach(_, _, _))
            .WillByDefault(DoAll(SetArgReferee<2>(attachResult), Return(aos::ErrorEnum::eNone)));
        ON_CALL(mStorage, GetNetworksInfo(_)).WillByDefault(Invoke([this](aos::Array<NetworkInfo>& out) {
            out = mStoredNetworks;

            return aos::ErrorEnum::eNone;
        }));
        ON_CALL(mStorage, GetInstanceNetworksInfo(_))
            .WillByDefault(Invoke([this](aos::Array<InstanceNetworkInfo>& out) {
                out = mStoredInstances;

                return aos::ErrorEnum::eNone;
            }));
    }

    static aos::InstanceNetworkAllocation CreateAllocatedParams()
    {
        aos::InstanceNetworkAllocation allocatedParams;

        allocatedParams.mIP     = "192.168.1.2";
        allocatedParams.mSubnet = "192.168.1.0/24";
        allocatedParams.mDNSServers.PushBack("8.8.8.8");

        return allocatedParams;
    }

    static InstanceNetworkConfig CreateConfig()
    {
        InstanceNetworkConfig config;

        config.mInstanceIdent.mItemID    = "test-item";
        config.mInstanceIdent.mSubjectID = "test-subject";
        config.mInstanceIdent.mInstance  = 0;
        config.mHostname                 = "test-host";
        config.mIngressKbit              = 100;
        config.mEgressKbit               = 100;
        config.mHosts.PushBack(aos::Host {"10.0.0.1", "host1.example.com"});
        config.mAliases.PushBack("alias1");

        return config;
    }

    std::unique_ptr<NetworkManager> InitManager(bool arm = false, size_t failAt = 0)
    {
        auto manager = std::make_unique<NetworkManager>();

        mAllocator.Disarm();

        if (arm) {
            mAllocator.Arm(failAt);
        }

        auto err = manager->Init(mAllocator, mStorage, mBridgeNetwork, mFirewall, mBandwidth, mDNSName, mTrafficMonitor,
            mNetns, mNetIf, mRandom, mNetIfFactory, mNetworkProvider, "test-node");
        if (!err.IsNone()) {
            return nullptr;
        }

        return manager;
    }

    std::unique_ptr<NetworkManager> StartManager()
    {
        auto manager = InitManager();

        EXPECT_TRUE(manager);
        EXPECT_EQ(manager->Start(), aos::ErrorEnum::eNone);

        return manager;
    }

    void CreateInstance(NetworkManager& manager, bool start)
    {
        ASSERT_EQ(
            manager.CreateInstanceNetwork(cTestInstanceID, cTestNetworkID, CreateConfig()), aos::ErrorEnum::eNone);

        if (start) {
            ASSERT_EQ(manager.StartInstanceNetwork(cTestInstanceID, cTestNetworkID), aos::ErrorEnum::eNone);
        }
    }

    template <typename Scenario>
    void ExpectNoMemoryForEachAllocation(size_t expectedFailures, Scenario&& scenario)
    {
        size_t failures = 0;

        for (size_t failAt = 0; failAt < 64; ++failAt) {
            mAllocator.Disarm();

            aos::Error err = scenario(failAt);

            if (!mAllocator.IsTriggered()) {
                EXPECT_TRUE(err.IsNone()) << "failAt=" << failAt << " err=" << err.Message();

                break;
            }

            EXPECT_TRUE(err.Is(aos::ErrorEnum::eNoMemory)) << "failAt=" << failAt << " err=" << err.Message();

            failures++;
        }

        EXPECT_EQ(failures, expectedFailures);
    }

    FailingAllocator mAllocator;

    NiceMock<StorageMock>                                        mStorage;
    NiceMock<BridgeNetworkMock>                                  mBridgeNetwork;
    NiceMock<FirewallMock>                                       mFirewall;
    NiceMock<BandwidthMock>                                      mBandwidth;
    NiceMock<DNSNameMock>                                        mDNSName;
    NiceMock<DNSServerMock>                                      mDNSServer;
    NiceMock<TrafficMonitorMock>                                 mTrafficMonitor;
    NiceMock<NamespaceManagerMock>                               mNetns;
    NiceMock<InterfaceManagerMock>                               mNetIf;
    NiceMock<InterfaceFactoryMock>                               mNetIfFactory;
    NiceMock<RandomMock>                                         mRandom;
    NiceMock<NetworkProviderMock>                                mNetworkProvider;
    aos::StaticArray<NetworkInfo, aos::cMaxNumOwners>            mStoredNetworks;
    aos::StaticArray<InstanceNetworkInfo, aos::cMaxNumInstances> mStoredInstances;
};

TEST_F(NetworkManagerAllocFailureTest, Init_FailsOnAllocationError)
{
    ExpectNoMemoryForEachAllocation(2, [this](size_t failAt) {
        auto manager = std::make_unique<NetworkManager>();

        mAllocator.Arm(failAt);

        return manager->Init(mAllocator, mStorage, mBridgeNetwork, mFirewall, mBandwidth, mDNSName, mTrafficMonitor,
            mNetns, mNetIf, mRandom, mNetIfFactory, mNetworkProvider, "test-node");
    });
}

TEST_F(NetworkManagerAllocFailureTest, Start_FailsOnAllocationError)
{
    ExpectNoMemoryForEachAllocation(7, [this](size_t failAt) {
        auto manager = InitManager();

        mAllocator.Arm(failAt);

        return manager->Start();
    });
}

TEST_F(NetworkManagerAllocFailureTest, CreateInstanceNetwork_FailsOnAllocationError)
{
    ExpectNoMemoryForEachAllocation(3, [this](size_t failAt) {
        auto manager = StartManager();

        mAllocator.Arm(failAt);

        return manager->CreateInstanceNetwork(cTestInstanceID, cTestNetworkID, CreateConfig());
    });
}

TEST_F(NetworkManagerAllocFailureTest, StartInstanceNetwork_FailsOnAllocationError)
{
    ExpectNoMemoryForEachAllocation(7, [this](size_t failAt) {
        auto manager = StartManager();

        CreateInstance(*manager, false);

        mAllocator.Arm(failAt);

        return manager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID);
    });
}

TEST_F(NetworkManagerAllocFailureTest, StopInstanceNetwork_FailsOnAllocationError)
{
    ExpectNoMemoryForEachAllocation(1, [this](size_t failAt) {
        auto manager = StartManager();

        CreateInstance(*manager, true);

        mAllocator.Arm(failAt);

        return manager->StopInstanceNetwork(cTestInstanceID, cTestNetworkID);
    });
}

TEST_F(NetworkManagerAllocFailureTest, GetResolvServers_FailsOnAllocationError)
{
    ExpectNoMemoryForEachAllocation(1, [this](size_t failAt) {
        auto manager = StartManager();

        CreateInstance(*manager, false);

        aos::StaticArray<aos::StaticString<aos::cIPLen>, 4> servers;

        mAllocator.Arm(failAt);

        return manager->GetResolvServers(cTestInstanceID, servers);
    });
}

TEST_F(NetworkManagerAllocFailureTest, GetHosts_FailsOnAllocationError)
{
    ExpectNoMemoryForEachAllocation(1, [this](size_t failAt) {
        auto manager = StartManager();

        CreateInstance(*manager, false);

        aos::StaticArray<aos::Host, 8> hosts;

        mAllocator.Arm(failAt);

        return manager->GetHosts(cTestInstanceID, hosts);
    });
}

TEST_F(NetworkManagerAllocFailureTest, FlushBatch_ReportsInstanceFailedOnAllocationError)
{
    for (size_t failAt = 0; failAt < 2; ++failAt) {
        auto manager = StartManager();

        CreateInstance(*manager, false);

        ASSERT_EQ(manager->BeginBatch(), aos::ErrorEnum::eNone);
        ASSERT_EQ(manager->StartInstanceNetwork(cTestInstanceID, cTestNetworkID), aos::ErrorEnum::eNone);

        ON_CALL(mFirewall, FlushBatch()).WillByDefault(Return(aos::ErrorEnum::eFailed));

        mAllocator.Arm(failAt);

        aos::StaticArray<aos::StaticString<aos::cIDLen>, 4> failed;

        EXPECT_EQ(manager->FlushBatch(failed), aos::ErrorEnum::eNone);
        EXPECT_TRUE(mAllocator.IsTriggered());
        ASSERT_EQ(failed.Size(), 1u) << "failAt=" << failAt;
        EXPECT_EQ(failed[0], aos::String(cTestInstanceID));

        ON_CALL(mFirewall, FlushBatch()).WillByDefault(Return(aos::ErrorEnum::eNone));
    }
}

TEST_F(NetworkManagerAllocFailureTest, Start_FallsBackToCleanupWhenAdoptingInstanceFailsOnAllocationError)
{
    NetworkInfo network;

    network.mNetworkID    = cTestNetworkID;
    network.mIP           = "192.168.1.1";
    network.mSubnet       = "192.168.1.0/24";
    network.mVlanID       = 100ULL;
    network.mVlanIfName   = "vlan-1234abcd";
    network.mBridgeIfName = "br-ef567890";

    InstanceNetworkInfo instance;

    instance.mInstanceID                 = cTestInstanceID;
    instance.mNetworkID                  = cTestNetworkID;
    instance.mNetworkConfig              = CreateConfig();
    instance.mAllocatedParams            = CreateAllocatedParams();
    instance.mHostIfName                 = cTestHostIfName;
    instance.mNetworkConfig.mIngressKbit = 0;
    instance.mNetworkConfig.mEgressKbit  = 0;

    mStoredNetworks.PushBack(network);
    mStoredInstances.PushBack(instance);

    LinkInfo link;

    link.mName   = cTestHostIfName;
    link.mKind   = LinkKindEnum::eVeth;
    link.mMaster = network.mBridgeIfName;

    ON_CALL(mNetIf, GetLink(aos::String(cTestHostIfName), _))
        .WillByDefault(DoAll(SetArgReferee<1>(link), Return(aos::ErrorEnum::eNone)));

    size_t cleaned = 0;

    ON_CALL(mNetns, DeleteNetworkNamespace(_)).WillByDefault(Invoke([&cleaned](const aos::String&) {
        cleaned++;

        return aos::ErrorEnum::eNone;
    }));

    // Allocation order in Start: firewall orphans (2), masquerades (1), DNS orphans (1), unit networks (1),
    // reconcile entries (1), then the adopted instance config and hosts.
    constexpr size_t cFirstAdoptAllocation = 6;

    for (size_t failAt = cFirstAdoptAllocation; failAt < cFirstAdoptAllocation + 2; ++failAt) {
        cleaned = 0;

        auto manager = InitManager();

        ASSERT_TRUE(manager);

        mAllocator.Arm(failAt);

        EXPECT_EQ(manager->Start(), aos::ErrorEnum::eNone);
        EXPECT_TRUE(mAllocator.IsTriggered()) << "failAt=" << failAt;
        EXPECT_EQ(cleaned, 1u) << "failAt=" << failAt;
    }

    cleaned = 0;

    auto manager = InitManager();

    ASSERT_TRUE(manager);

    EXPECT_EQ(manager->Start(), aos::ErrorEnum::eNone);
    EXPECT_EQ(cleaned, 0u);
}

TEST_F(NetworkManagerAllocFailureTest, OnPendingFirewallUpdate_SkipsRunningInstanceOnAllocationError)
{
    aos::networkmanager::PendingFirewallUpdate update;

    update.mInstanceIdent = CreateConfig().mInstanceIdent;

    aos::FirewallRule rule;

    rule.mDstIP   = "10.0.0.5";
    rule.mDstPort = "8080";
    rule.mProto   = "tcp";
    rule.mSrcIP   = "192.168.1.2";
    update.mFirewallRules.PushBack(rule);

    // Allocations: instance info, stored info copy, firewall params.
    for (size_t failAt = 0; failAt < 4; ++failAt) {
        auto manager = StartManager();

        CreateInstance(*manager, true);

        size_t updated = 0;

        ON_CALL(mFirewall, UpdateInstance(_, _)).WillByDefault(Invoke([&updated](const aos::String&, auto&) {
            updated++;

            return aos::ErrorEnum::eNone;
        }));

        mAllocator.Arm(failAt);

        manager->OnPendingFirewallUpdate("test-node", update);

        EXPECT_EQ(mAllocator.IsTriggered(), failAt < 3) << "failAt=" << failAt;
        EXPECT_EQ(updated, failAt < 3 ? 0u : 1u) << "failAt=" << failAt;
    }
}

TEST_F(NetworkManagerAllocFailureTest, OnConnect_SkipsSyncOnAllocationError)
{
    auto manager = StartManager();

    size_t synced = 0;

    ON_CALL(mNetworkProvider, SyncNetworkState(_, _))
        .WillByDefault(Invoke([&synced](const aos::String&, const aos::Array<aos::InstanceNetworkStateInfo>&) {
            synced++;

            return aos::ErrorEnum::eNone;
        }));

    mAllocator.Arm(0);

    manager->OnConnect();

    EXPECT_TRUE(mAllocator.IsTriggered());
    EXPECT_EQ(synced, 0u);

    mAllocator.Disarm();

    manager->OnConnect();

    EXPECT_EQ(synced, 1u);
}
