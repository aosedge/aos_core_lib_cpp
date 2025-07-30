/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/identifierpool.hpp>

using namespace testing;

namespace aos {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr size_t cIdsRangeBegin   = 5000;
constexpr size_t cIdsRangeEnd     = 5010;
constexpr size_t cMaxNumLockedIDs = 5;

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

using IDPool = IdentifierRangePool<cIdsRangeBegin, cIdsRangeEnd, cMaxNumLockedIDs>;

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

        mIdentifierPool = std::make_unique<IDPool>();
    }

    std::unique_ptr<IDPool> mIdentifierPool;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(IdentifierPoolTest, allIdentifiersAreValid)
{
    auto err = mIdentifierPool->Init(AllIdsAreValid);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    size_t id = 0;

    Tie(id, err) = mIdentifierPool->Acquire();
    ASSERT_EQ(err, ErrorEnum::eNone);
    ASSERT_EQ(id, 5000);

    err = mIdentifierPool->TryAcquire(5001);
    ASSERT_EQ(err, ErrorEnum::eNone);

    err = mIdentifierPool->TryAcquire(5001);
    ASSERT_EQ(err, ErrorEnum::eFailed);

    err = mIdentifierPool->TryAcquire(0);
    ASSERT_EQ(err, ErrorEnum::eOutOfRange);

    err = mIdentifierPool->TryAcquire(std::numeric_limits<size_t>::max());
    ASSERT_EQ(err, ErrorEnum::eOutOfRange);

    Tie(id, err) = mIdentifierPool->Acquire();
    ASSERT_EQ(err, ErrorEnum::eNone);
    ASSERT_EQ(id, 5002);

    err = mIdentifierPool->Release(5001);
    ASSERT_EQ(err, ErrorEnum::eNone);

    Tie(id, err) = mIdentifierPool->Acquire();
    ASSERT_EQ(err, ErrorEnum::eNone);
    ASSERT_EQ(id, 5001);
}

TEST_F(IdentifierPoolTest, getFreeIdReturnsErrorIfPoolNotInitialized)
{
    auto [id, err] = mIdentifierPool->Acquire();
    ASSERT_EQ(err, ErrorEnum::eFailed);

    err = mIdentifierPool->TryAcquire(5000);
    ASSERT_EQ(err, ErrorEnum::eNone);

    Tie(id, err) = mIdentifierPool->Acquire();
    ASSERT_EQ(err, ErrorEnum::eFailed);
}

TEST_F(IdentifierPoolTest, onlyEvenIdsAreValid)
{
    auto err = mIdentifierPool->Init(EvenIdsAreValid);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    size_t id = 0;

    Tie(id, err) = mIdentifierPool->Acquire();
    ASSERT_EQ(err, ErrorEnum::eNone);
    ASSERT_EQ(id, 5000);

    Tie(id, err) = mIdentifierPool->Acquire();
    ASSERT_EQ(err, ErrorEnum::eNone);
    ASSERT_EQ(id, 5002);
}

TEST_F(IdentifierPoolTest, lockedIdsExceedsLimit)
{
    auto err = mIdentifierPool->Init(AllIdsAreValid);
    ASSERT_TRUE(err.IsNone()) << err.Message();

    for (size_t i = cIdsRangeBegin; i < cIdsRangeBegin + cMaxNumLockedIDs; ++i) {
        err = mIdentifierPool->TryAcquire(i);
        ASSERT_EQ(err, ErrorEnum::eNone);
    }

    size_t id = 0;

    Tie(id, err) = mIdentifierPool->Acquire();
    ASSERT_EQ(err.Value(), ErrorEnum::eNoMemory);
}

} // namespace aos
