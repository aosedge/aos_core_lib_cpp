/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/sm/launcher/instanceregistry.hpp>
#include <core/sm/tests/mocks/instanceidprovidermock.hpp>

using namespace testing;

namespace aos::sm::launcher {

namespace {

/***********************************************************************************************************************
 * Utils
 **********************************************************************************************************************/

InstanceInfo CreateInstanceInfo(const char* itemID, uint64_t instance = 0, const char* version = "1.0.0")
{
    InstanceInfo info;

    info.mItemID    = itemID;
    info.mSubjectID = "subject";
    info.mInstance  = instance;
    info.mType      = UpdateItemTypeEnum::eService;
    info.mVersion   = version;
    info.mRuntimeID = "runtime";

    return info;
}

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class InstanceRegistryTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        ON_CALL(mInstanceIDProvider, GetInstanceID)
            .WillByDefault(Invoke(
                [](const InstanceIdent& ident, String& instanceID) { return instanceID.Assign(ident.mItemID); }));

        mRegistry = std::make_unique<InstanceRegistry>();
        mRegistry->Init(mInstanceIDProvider);
    }

    NiceMock<InstanceIDProviderMock>  mInstanceIDProvider;
    std::unique_ptr<InstanceRegistry> mRegistry;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(InstanceRegistryTest, AddInitializesInactiveInstance)
{
    const auto info = CreateInstanceInfo("item1", 2, "2.0.0");

    auto [instance, err] = mRegistry->Add(info);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    ASSERT_NE(instance, nullptr);

    EXPECT_EQ(instance->mInfo, info);
    EXPECT_EQ(static_cast<const InstanceIdent&>(instance->mStatus), static_cast<const InstanceIdent&>(info));
    EXPECT_EQ(instance->mStatus.mVersion, info.mVersion);
    EXPECT_EQ(instance->mStatus.mRuntimeID, info.mRuntimeID);
    EXPECT_EQ(instance->mStatus.mState, InstanceStateEnum::eInactive);
    EXPECT_EQ(instance->mInstanceID, String("item1"));
    EXPECT_FALSE(instance->mConfigsLoaded);

    EXPECT_EQ(mRegistry->Find(info), instance);
    EXPECT_EQ(mRegistry->FindByID("item1"), instance);
    EXPECT_EQ(mRegistry->Size(), 1);
}

TEST_F(InstanceRegistryTest, AddFailsIfInstanceIDNotProvided)
{
    EXPECT_CALL(mInstanceIDProvider, GetInstanceID).WillOnce(Return(ErrorEnum::eNotFound));

    auto [instance, err] = mRegistry->Add(CreateInstanceInfo("item1"));

    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound));
    EXPECT_EQ(instance, nullptr);
    EXPECT_EQ(mRegistry->Size(), 0);
}

TEST_F(InstanceRegistryTest, FindReturnsNullForUnknownInstance)
{
    ASSERT_TRUE(mRegistry->Add(CreateInstanceInfo("item1", 0)).mError.IsNone());

    EXPECT_EQ(mRegistry->Find(CreateInstanceInfo("item1", 1)), nullptr);
    EXPECT_EQ(mRegistry->Find(CreateInstanceInfo("item2", 0)), nullptr);
    EXPECT_EQ(mRegistry->FindByID("item2"), nullptr);
}

TEST_F(InstanceRegistryTest, RemoveRemovesOnlyMatchingInstance)
{
    ASSERT_TRUE(mRegistry->Add(CreateInstanceInfo("item1")).mError.IsNone());
    ASSERT_TRUE(mRegistry->Add(CreateInstanceInfo("item2")).mError.IsNone());

    mRegistry->Remove(CreateInstanceInfo("item1"));

    EXPECT_EQ(mRegistry->Size(), 1);
    EXPECT_EQ(mRegistry->Find(CreateInstanceInfo("item1")), nullptr);
    EXPECT_NE(mRegistry->Find(CreateInstanceInfo("item2")), nullptr);
}

TEST_F(InstanceRegistryTest, GetStatusesReturnsAllInstances)
{
    ASSERT_TRUE(mRegistry->Add(CreateInstanceInfo("item1")).mError.IsNone());
    ASSERT_TRUE(mRegistry->Add(CreateInstanceInfo("item2")).mError.IsNone());

    mRegistry->Find(CreateInstanceInfo("item2"))->mStatus.mState = InstanceStateEnum::eActive;

    StaticArray<InstanceStatus, 4> statuses;

    ASSERT_TRUE(mRegistry->GetStatuses(statuses).IsNone());
    ASSERT_EQ(statuses.Size(), 2);

    EXPECT_EQ(statuses[0].mItemID, String("item1"));
    EXPECT_EQ(statuses[0].mState, InstanceStateEnum::eInactive);
    EXPECT_EQ(statuses[1].mItemID, String("item2"));
    EXPECT_EQ(statuses[1].mState, InstanceStateEnum::eActive);
}

TEST_F(InstanceRegistryTest, GetMinOfflineTTLUsesActiveAndActivatingInstances)
{
    const struct {
        const char*   mItemID;
        InstanceState mState;
        Duration      mOfflineTTL;
    } instances[] = {
        {"active", InstanceStateEnum::eActive, 30 * Time::cSeconds},
        {"activating", InstanceStateEnum::eActivating, 20 * Time::cSeconds},
        {"inactive", InstanceStateEnum::eInactive, 5 * Time::cSeconds},
        {"failed", InstanceStateEnum::eFailed, 5 * Time::cSeconds},
        {"noTTL", InstanceStateEnum::eActive, 0},
    };

    EXPECT_FALSE(mRegistry->GetMinOfflineTTL().HasValue());

    for (const auto& item : instances) {
        auto [instance, err] = mRegistry->Add(CreateInstanceInfo(item.mItemID));
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        instance->mStatus.mState = item.mState;
        instance->mOfflineTTL    = item.mOfflineTTL;
    }

    auto ttl = mRegistry->GetMinOfflineTTL();

    ASSERT_TRUE(ttl.HasValue());
    EXPECT_EQ(*ttl, 20 * Time::cSeconds);
}

} // namespace aos::sm::launcher
