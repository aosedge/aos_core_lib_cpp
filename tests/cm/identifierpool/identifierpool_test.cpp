/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <aos/cm/identifierpool/identifierpool.hpp>
#include <aos/test/log.hpp>

using namespace testing;

namespace aos::cm::identifierpool {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

bool AllIdsAreValid(size_t id)
{
    (void)id;

    return true;
}

bool EvenIdsAreValid(size_t id)
{
    return id % 2 == 0;
}

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class IdentifierPoolTest : public Test {
protected:
    void SetUp() override
    {
        test::InitLog();

        mIdentifierPool = std::make_unique<IdentifierPool>();
    }

    std::unique_ptr<IdentifierPool> mIdentifierPool;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(IdentifierPoolTest, allIdentifiersAreValid)
{
    auto err = mIdentifierPool->Init(AllIdsAreValid);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    size_t uid = 0;

    Tie(uid, err) = mIdentifierPool->GetFreeID();
    ASSERT_EQ(err, ErrorEnum::eNone);
    ASSERT_EQ(uid, 5000);

    err = mIdentifierPool->LockID(5001);
    ASSERT_EQ(err, ErrorEnum::eNone);

    err = mIdentifierPool->LockID(5001);
    ASSERT_EQ(err, ErrorEnum::eFailed);

    err = mIdentifierPool->LockID(0);
    ASSERT_EQ(err, ErrorEnum::eOutOfRange);

    err = mIdentifierPool->LockID(std::numeric_limits<size_t>::max());
    ASSERT_EQ(err, ErrorEnum::eOutOfRange);

    Tie(uid, err) = mIdentifierPool->GetFreeID();
    ASSERT_EQ(err, ErrorEnum::eNone);
    ASSERT_EQ(uid, 5002);

    err = mIdentifierPool->ReleaseID(5001);
    ASSERT_EQ(err, ErrorEnum::eNone);

    Tie(uid, err) = mIdentifierPool->GetFreeID();
    ASSERT_EQ(err, ErrorEnum::eNone);
    ASSERT_EQ(uid, 5001);
}

TEST_F(IdentifierPoolTest, getFreeIdReturnsErrorIfPoolNotInitialized)
{
    auto [uid, err] = mIdentifierPool->GetFreeID();
    ASSERT_EQ(err, ErrorEnum::eNotFound);

    err = mIdentifierPool->LockID(5000);
    ASSERT_EQ(err, ErrorEnum::eNone);

    Tie(uid, err) = mIdentifierPool->GetFreeID();
    ASSERT_EQ(err, ErrorEnum::eNotFound);
}

TEST_F(IdentifierPoolTest, onlyEvenIdsAreValid)
{
    auto err = mIdentifierPool->Init(EvenIdsAreValid);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    size_t uid = 0;

    Tie(uid, err) = mIdentifierPool->GetFreeID();
    ASSERT_EQ(err, ErrorEnum::eNone);
    ASSERT_EQ(uid, 5000);

    Tie(uid, err) = mIdentifierPool->GetFreeID();
    ASSERT_EQ(err, ErrorEnum::eNone);
    ASSERT_EQ(uid, 5002);
}

} // namespace aos::cm::identifierpool
