/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/mocks/certhandlermock.hpp>
#include <core/common/tests/mocks/cloudconnectionmock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/heapallocator.hpp>

#include <core/cm/rootcerts/rootcerts.hpp>
#include <core/cm/tests/mocks/nodeinfoprovidermock.hpp>

using namespace testing;

namespace aos::cm::rootcerts {

namespace {

/***********************************************************************************************************************
 * Mocks
 **********************************************************************************************************************/

class SenderMock : public SenderItf {
public:
    MOCK_METHOD(Error, SendUnitRootCertificates, (const UnitRootCertificates&), (override));
};

/***********************************************************************************************************************
 * Utils
 **********************************************************************************************************************/

SHA256Fingerprint CreateFingerprint(char symbol)
{
    SHA256Fingerprint fingerprint;

    for (size_t i = 0; i < crypto::cSHA256HexLen; ++i) {
        fingerprint.PushBack(symbol);
    }

    return fingerprint;
}

std::unique_ptr<UnitRootCertificates> CreateUnitRootCertificates(
    bool isPartial, const std::vector<std::pair<std::string, std::vector<char>>>& nodes)
{
    auto result = std::make_unique<UnitRootCertificates>();

    result->mIsPartial = isPartial;

    for (const auto& [nodeID, fingerprints] : nodes) {
        result->mNodeCertificates.EmplaceBack();

        auto& nodeCerts = result->mNodeCertificates.Back();

        nodeCerts.mNodeID = nodeID.c_str();

        for (auto symbol : fingerprints) {
            nodeCerts.mSHA256Fingerprints.PushBack(CreateFingerprint(symbol));
        }
    }

    return result;
}

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class RootCertsTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        EXPECT_CALL(mCloudConnectionMock, SubscribeListener(_))
            .WillOnce(Invoke([this](cloudconnection::ConnectionListenerItf& listener) {
                mConnectionListener = &listener;

                return ErrorEnum::eNone;
            }));

        EXPECT_CALL(mNodeInfoProviderMock, SubscribeListener(_))
            .WillOnce(Invoke([this](nodeinfoprovider::NodeInfoListenerItf& listener) {
                mNodeInfoListener = &listener;

                return ErrorEnum::eNone;
            }));

        ASSERT_TRUE(
            mRootCerts.Init(mAllocator, mCertHandlerMock, mNodeInfoProviderMock, mCloudConnectionMock, mSenderMock)
                .IsNone());
        ASSERT_TRUE(mRootCerts.Start().IsNone());

        ASSERT_NE(mConnectionListener, nullptr);
        ASSERT_NE(mNodeInfoListener, nullptr);
    }

    void TearDown() override
    {
        EXPECT_CALL(mNodeInfoProviderMock, UnsubscribeListener(Ref(*mNodeInfoListener)))
            .WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mCloudConnectionMock, UnsubscribeListener(Ref(*mConnectionListener)))
            .WillOnce(Return(ErrorEnum::eNone));

        EXPECT_TRUE(mRootCerts.Stop().IsNone());
    }

    void ExpectGetRootCerts(const std::string& nodeID, const std::vector<char>& fingerprints)
    {
        EXPECT_CALL(
            mCertHandlerMock, GetRootCerts(Truly([nodeID](const String& id) { return id == nodeID.c_str(); }), _))
            .WillOnce(Invoke([fingerprints](const String&, Array<SHA256Fingerprint>& result) {
                for (auto symbol : fingerprints) {
                    if (auto err = result.PushBack(CreateFingerprint(symbol)); !err.IsNone()) {
                        return err;
                    }
                }

                return Error(ErrorEnum::eNone);
            }));
    }

    void ExpectSendUnitRootCertificates(const UnitRootCertificates& expected)
    {
        EXPECT_CALL(mSenderMock, SendUnitRootCertificates(_))
            .WillOnce(Invoke([&expected](const UnitRootCertificates& unitRootCerts) {
                EXPECT_EQ(unitRootCerts, expected);

                return ErrorEnum::eNone;
            }));
    }

    HeapAllocator                          mAllocator;
    iamclient::CertHandlerMock             mCertHandlerMock;
    nodeinfoprovider::NodeInfoProviderMock mNodeInfoProviderMock;
    cloudconnection::CloudConnectionMock   mCloudConnectionMock;
    StrictMock<SenderMock>                 mSenderMock;
    RootCerts                              mRootCerts;

    cloudconnection::ConnectionListenerItf* mConnectionListener {};
    nodeinfoprovider::NodeInfoListenerItf*  mNodeInfoListener {};
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(RootCertsTest, ProcessDesiredRootCertificates)
{
    auto desired = std::make_unique<DesiredUnitRootCertificates>();

    for (const auto& nodeID : {"node0", "node1", "node2"}) {
        desired->mNodeCertificates.EmplaceBack();

        auto& nodeCerts = desired->mNodeCertificates.Back();

        nodeCerts.mNodeID = nodeID;
        nodeCerts.mCertificates.PushBack("pem-root-1");
        nodeCerts.mCertificates.PushBack("pem-root-2");
    }

    InSequence seq;

    for (const auto& nodeCerts : desired->mNodeCertificates) {
        // Error on one node must not prevent updating others
        auto err = nodeCerts.mNodeID == "node1" ? Error(ErrorEnum::eFailed) : Error(ErrorEnum::eNone);

        EXPECT_CALL(mCertHandlerMock, UpdateRootCerts(nodeCerts.mNodeID, nodeCerts.mCertificates))
            .WillOnce(Return(err));
    }

    EXPECT_TRUE(mRootCerts.ProcessDesiredRootCertificates(*desired).IsNone());
}

TEST_F(RootCertsTest, SendFullSnapshotOnConnect)
{
    EXPECT_CALL(mNodeInfoProviderMock, GetAllNodeIDs(_)).WillOnce(Invoke([](Array<StaticString<cIDLen>>& nodeIDs) {
        nodeIDs.PushBack("node0");
        nodeIDs.PushBack("node1");
        nodeIDs.PushBack("node2");

        return ErrorEnum::eNone;
    }));

    ExpectGetRootCerts("node0", {'a', 'b'});
    // Node with failed root certs request must be skipped
    EXPECT_CALL(mCertHandlerMock, GetRootCerts(String("node1"), _)).WillOnce(Return(ErrorEnum::eNotFound));
    ExpectGetRootCerts("node2", {'c'});

    auto expected = CreateUnitRootCertificates(false, {{"node0", {'a', 'b'}}, {"node2", {'c'}}});

    ExpectSendUnitRootCertificates(*expected);

    mConnectionListener->OnConnect();
}

TEST_F(RootCertsTest, SendPartialUpdateOnNodeInfoChanged)
{
    auto nodeInfo = std::make_unique<UnitNodeInfo>();

    nodeInfo->mNodeID = "node1";

    // Not sent while cloud is not connected
    mNodeInfoListener->OnNodeInfoChanged(*nodeInfo);

    EXPECT_CALL(mNodeInfoProviderMock, GetAllNodeIDs(_)).WillOnce(Return(ErrorEnum::eNone));

    auto snapshot = CreateUnitRootCertificates(false, {});

    ExpectSendUnitRootCertificates(*snapshot);

    mConnectionListener->OnConnect();

    Mock::VerifyAndClearExpectations(&mSenderMock);

    ExpectGetRootCerts("node1", {'d'});

    auto expected = CreateUnitRootCertificates(true, {{"node1", {'d'}}});

    ExpectSendUnitRootCertificates(*expected);

    mNodeInfoListener->OnNodeInfoChanged(*nodeInfo);

    Mock::VerifyAndClearExpectations(&mSenderMock);

    // Not sent after cloud is disconnected
    mConnectionListener->OnDisconnect();
    mNodeInfoListener->OnNodeInfoChanged(*nodeInfo);
}

TEST_F(RootCertsTest, PartialUpdateNotSentOnGetRootCertsError)
{
    EXPECT_CALL(mNodeInfoProviderMock, GetAllNodeIDs(_)).WillOnce(Return(ErrorEnum::eNone));

    auto snapshot = CreateUnitRootCertificates(false, {});

    ExpectSendUnitRootCertificates(*snapshot);

    mConnectionListener->OnConnect();

    Mock::VerifyAndClearExpectations(&mSenderMock);

    auto nodeInfo = std::make_unique<UnitNodeInfo>();

    nodeInfo->mNodeID = "node1";

    EXPECT_CALL(mCertHandlerMock, GetRootCerts(String("node1"), _)).WillOnce(Return(ErrorEnum::eNotFound));

    mNodeInfoListener->OnNodeInfoChanged(*nodeInfo);
}

} // namespace aos::cm::rootcerts
