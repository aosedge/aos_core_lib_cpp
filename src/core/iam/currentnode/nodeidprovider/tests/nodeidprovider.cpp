/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <core/common/crypto/cryptoprovider.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/common/tools/uuid.hpp>
#include <core/iam/currentnode/nodeidprovider/nodeidprovider.hpp>
#include <core/iam/tests/mocks/hwinfoprovidermock.hpp>

using namespace testing;

namespace aos::iam::currentnode {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cHWID = "hw-id-1234";

class UUIDMock : public crypto::UUIDItf {
public:
    MOCK_METHOD(RetWithError<uuid::UUID>, CreateUUIDv4, (), (override));
    MOCK_METHOD(RetWithError<uuid::UUID>, CreateUUIDv5, (const uuid::UUID&, const Array<uint8_t>&), (override));
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class NodeIDProviderTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        ASSERT_TRUE(mCryptoProvider.Init(mAllocator).IsNone());
    }

    void ExpectHWID(const char* hwID)
    {
        EXPECT_CALL(mHWInfoProvider, GetHWID).WillOnce(Invoke([hwID](String& id) { return id.Assign(hwID); }));
    }

    StaticString<cIDLen> ExpectedNodeID(const char* hwID)
    {
        auto [space, err] = uuid::StringToUUID(cNodeIDNamespaceUUID);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        uuid::UUID nodeUUID;

        Tie(nodeUUID, err) = mCryptoProvider.CreateUUIDv5(space, String(hwID).AsByteArray());
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        return uuid::UUIDToString(nodeUUID);
    }

    HeapAllocator                  mAllocator;
    crypto::DefaultCryptoProvider  mCryptoProvider;
    StrictMock<HWInfoProviderMock> mHWInfoProvider;
    NodeIDProvider                 mProvider;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(NodeIDProviderTest, NodeIDIsDerivedFromHWID)
{
    ExpectHWID(cHWID);

    auto err = mProvider.Init(mHWInfoProvider, mCryptoProvider);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StaticString<cIDLen> nodeID;

    err = mProvider.GetNodeID(nodeID);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(nodeID, ExpectedNodeID(cHWID));
}

TEST_F(NodeIDProviderTest, NodeIDDiffersForDifferentHWID)
{
    ExpectHWID(cHWID);

    ASSERT_TRUE(mProvider.Init(mHWInfoProvider, mCryptoProvider).IsNone());

    StaticString<cIDLen> nodeID1;

    ASSERT_TRUE(mProvider.GetNodeID(nodeID1).IsNone());

    ExpectHWID("another-hw-id");

    ASSERT_TRUE(mProvider.Init(mHWInfoProvider, mCryptoProvider).IsNone());

    StaticString<cIDLen> nodeID2;

    ASSERT_TRUE(mProvider.GetNodeID(nodeID2).IsNone());

    EXPECT_NE(nodeID1, nodeID2);
}

TEST_F(NodeIDProviderTest, InitFailsIfGetHWIDFails)
{
    EXPECT_CALL(mHWInfoProvider, GetHWID).WillOnce(Return(ErrorEnum::eNotFound));

    auto err = mProvider.Init(mHWInfoProvider, mCryptoProvider);
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);

    StaticString<cIDLen> nodeID;

    err = mProvider.GetNodeID(nodeID);
    EXPECT_TRUE(err.Is(ErrorEnum::eWrongState)) << tests::utils::ErrorToStr(err);
}

TEST_F(NodeIDProviderTest, GetNodeIDFailsIfOutputBufferTooSmall)
{
    ExpectHWID(cHWID);

    ASSERT_TRUE(mProvider.Init(mHWInfoProvider, mCryptoProvider).IsNone());

    StaticString<4> nodeID;

    EXPECT_FALSE(mProvider.GetNodeID(nodeID).IsNone());
}

TEST_F(NodeIDProviderTest, InitFailsIfCreateUUIDFails)
{
    StrictMock<UUIDMock> uuidProvider;

    ExpectHWID(cHWID);
    EXPECT_CALL(uuidProvider, CreateUUIDv5).WillOnce(Return(RetWithError<uuid::UUID>({}, ErrorEnum::eFailed)));

    auto err = mProvider.Init(mHWInfoProvider, uuidProvider);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    StaticString<cIDLen> nodeID;

    err = mProvider.GetNodeID(nodeID);
    EXPECT_TRUE(err.Is(ErrorEnum::eWrongState)) << tests::utils::ErrorToStr(err);
}

} // namespace aos::iam::currentnode
