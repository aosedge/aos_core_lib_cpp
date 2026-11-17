/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/sm/launcher/runtimemanager.hpp>

#include "mocks/runtimemock.hpp"

using namespace testing;

namespace aos::sm::launcher {

namespace {

/***********************************************************************************************************************
 * Utils
 **********************************************************************************************************************/

RuntimeInfo CreateRuntimeInfo(const char* runtimeID)
{
    RuntimeInfo info;

    info.mRuntimeID   = runtimeID;
    info.mRuntimeType = "runc";

    return info;
}

InstanceInfo CreateInstanceInfo(const char* itemID, const char* runtimeID)
{
    InstanceInfo info;

    info.mItemID    = itemID;
    info.mRuntimeID = runtimeID;

    return info;
}

std::vector<std::string> GetItemIDs(const Array<InstanceInfo>& instances)
{
    std::vector<std::string> itemIDs;

    for (const auto& instance : instances) {
        itemIDs.emplace_back(instance.mItemID.CStr());
    }

    return itemIDs;
}

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class RuntimeManagerTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        ON_CALL(mRuntime0, GetRuntimeInfo)
            .WillByDefault(DoAll(SetArgReferee<0>(CreateRuntimeInfo("runtime0")), Return(ErrorEnum::eNone)));
        ON_CALL(mRuntime1, GetRuntimeInfo)
            .WillByDefault(DoAll(SetArgReferee<0>(CreateRuntimeInfo("runtime1")), Return(ErrorEnum::eNone)));

        StaticArray<RuntimeItf*, cMaxNumNodeRuntimes> runtimes;

        runtimes.PushBack(&mRuntime0);
        runtimes.PushBack(&mRuntime1);

        ASSERT_TRUE(mRuntimes.Init(mAllocator, runtimes).IsNone());
    }

    HeapAllocator         mAllocator;
    NiceMock<RuntimeMock> mRuntime0;
    NiceMock<RuntimeMock> mRuntime1;
    RuntimeManager        mRuntimes;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(RuntimeManagerTest, StartReadsRuntimeIDs)
{
    EXPECT_EQ(mRuntimes.Find("runtime0"), nullptr);

    EXPECT_CALL(mRuntime0, Start).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mRuntime1, Start).WillOnce(Return(ErrorEnum::eNone));

    auto err = mRuntimes.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(mRuntimes.Find("runtime0"), &mRuntime0);
    EXPECT_EQ(mRuntimes.Find("runtime1"), &mRuntime1);
    EXPECT_EQ(mRuntimes.Find("unknown"), nullptr);
}

TEST_F(RuntimeManagerTest, StartFailsOnFirstRuntimeError)
{
    EXPECT_CALL(mRuntime0, Start).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mRuntime1, Start).Times(0);

    EXPECT_TRUE(mRuntimes.Start().Is(ErrorEnum::eFailed));
}

TEST_F(RuntimeManagerTest, StopStopsAllRuntimesAndReturnsFirstError)
{
    EXPECT_CALL(mRuntime0, Stop).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mRuntime1, Stop).WillOnce(Return(ErrorEnum::eWrongState));

    EXPECT_TRUE(mRuntimes.Stop().Is(ErrorEnum::eFailed));
}

TEST_F(RuntimeManagerTest, GetRuntimesInfosReturnsInfoOfEachRuntime)
{
    StaticArray<RuntimeInfo, cMaxNumNodeRuntimes> infos;

    auto err = mRuntimes.GetRuntimesInfos(infos);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(infos.Size(), 2);
    EXPECT_EQ(infos[0].mRuntimeID, String("runtime0"));
    EXPECT_EQ(infos[1].mRuntimeID, String("runtime1"));
}

TEST_F(RuntimeManagerTest, InitInstancesPassesEachRuntimeItsInstances)
{
    ASSERT_TRUE(mRuntimes.Start().IsNone());

    const std::vector<InstanceInfo> instances = {
        CreateInstanceInfo("item0", "runtime0"),
        CreateInstanceInfo("item1", "runtime1"),
        CreateInstanceInfo("item2", "runtime0"),
        CreateInstanceInfo("item3", "unknown"),
    };

    std::vector<std::string> runtime0Items;
    std::vector<std::string> runtime1Items;

    EXPECT_CALL(mRuntime0, InitInstances).WillOnce(Invoke([&](const Array<InstanceInfo>& runtimeInstances) {
        runtime0Items = GetItemIDs(runtimeInstances);

        return ErrorEnum::eNone;
    }));
    // The failure of a runtime is logged and doesn't fail the others.
    EXPECT_CALL(mRuntime1, InitInstances).WillOnce(Invoke([&](const Array<InstanceInfo>& runtimeInstances) {
        runtime1Items = GetItemIDs(runtimeInstances);

        return ErrorEnum::eFailed;
    }));

    auto err = mRuntimes.InitInstances(Array<InstanceInfo>(instances.data(), instances.size()));
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(runtime0Items, (std::vector<std::string> {"item0", "item2"}));
    EXPECT_EQ(runtime1Items, (std::vector<std::string> {"item1"}));
}

TEST_F(RuntimeManagerTest, RebootRebootsKnownRuntimes)
{
    ASSERT_TRUE(mRuntimes.Start().IsNone());

    const std::vector<StaticString<cIDLen>> runtimeIDs = {"runtime0", "unknown", "runtime1"};

    // The failure of a runtime and an unknown runtime are logged and don't stop the others.
    EXPECT_CALL(mRuntime0, Reboot).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mRuntime1, Reboot).WillOnce(Return(ErrorEnum::eNone));

    mRuntimes.Reboot(Array<StaticString<cIDLen>>(runtimeIDs.data(), runtimeIDs.size()));
}

} // namespace aos::sm::launcher
