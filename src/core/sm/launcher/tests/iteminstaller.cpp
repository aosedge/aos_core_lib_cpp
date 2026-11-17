/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <atomic>
#include <future>
#include <memory>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/mocks/itemstatusprovidermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/sm/launcher/iteminstaller.hpp>
#include <core/sm/tests/mocks/instanceidprovidermock.hpp>

#include "mocks/imagemanagermock.hpp"

using namespace testing;

namespace aos::sm::launcher {

namespace {

/***********************************************************************************************************************
 * Utils
 **********************************************************************************************************************/

InstanceInfo CreateInstanceInfo(const char* itemID, uint64_t instance, const char* version = "1.0.0")
{
    InstanceInfo info;

    info.mItemID   = itemID;
    info.mInstance = instance;
    info.mType     = UpdateItemTypeEnum::eService;
    info.mVersion  = version;

    return info;
}

UpdateItemStatus CreateItemStatus(const char* itemID, const char* version, ItemState state)
{
    UpdateItemStatus status;

    status.mItemID  = itemID;
    status.mVersion = version;
    status.mState   = state;

    return status;
}

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ItemInstallerTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        mLaunchPool = std::make_unique<LaunchPool>();
        ASSERT_TRUE(mLaunchPool->Run().IsNone());

        mInstaller.Init(mAllocator, *mLaunchPool, mImageManager, mItemStatusProvider);
        mInstaller.Start();
    }

    void TearDown() override { EXPECT_TRUE(mLaunchPool->Shutdown().IsNone()); }

    HeapAllocator                                          mAllocator;
    std::unique_ptr<LaunchPool>                            mLaunchPool;
    StrictMock<imagemanager::ImageManagerMock>             mImageManager;
    NiceMock<common::imagemanager::ItemStatusProviderMock> mItemStatusProvider;
    ItemInstaller                                          mInstaller;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(ItemInstallerTest, GetUnusedItemsReturnsItemsNotUsedByStartedInstances)
{
    NiceMock<InstanceIDProviderMock> instanceIDProvider;
    auto                             instances = std::make_unique<InstanceRegistry>();

    instances->Init(instanceIDProvider);

    ASSERT_TRUE(instances->Add(CreateInstanceInfo("item0", 0)).mError.IsNone());
    ASSERT_TRUE(instances->Add(CreateInstanceInfo("item0", 1)).mError.IsNone());
    ASSERT_TRUE(instances->Add(CreateInstanceInfo("item1", 0)).mError.IsNone());

    const std::vector<InstanceIdent> stopInstances = {
        CreateInstanceInfo("item0", 0),
        CreateInstanceInfo("item0", 1),
        CreateInstanceInfo("item1", 0),
        CreateInstanceInfo("unknown", 0),
    };
    // The item1 instance is restarted, so its item is still used.
    const std::vector<InstanceInfo> startInstances = {CreateInstanceInfo("item1", 1)};

    StaticArray<ItemVersion, 4> removeItems;

    mInstaller.GetUnusedItems(*instances, Array<InstanceIdent>(stopInstances.data(), stopInstances.size()),
        Array<InstanceInfo>(startInstances.data(), startInstances.size()), removeItems);

    ASSERT_EQ(removeItems.Size(), 1);
    EXPECT_EQ(removeItems[0].mItemID, String("item0"));
    EXPECT_EQ(removeItems[0].mVersion, String("1.0.0"));
}

TEST_F(ItemInstallerTest, InstallItemsInstallsNotInstalledItemsOnce)
{
    const std::vector<InstanceInfo> startInstances = {
        CreateInstanceInfo("installed", 0),
        CreateInstanceInfo("item0", 0),
        CreateInstanceInfo("item0", 1),
        CreateInstanceInfo("item1", 0),
        CreateInstanceInfo("installed", 0, "2.0.0"),
    };

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke([](Array<UpdateItemStatus>& statuses) {
        statuses.Clear();
        statuses.PushBack(CreateItemStatus("installed", "1.0.0", ItemStateEnum::eInstalled));
        statuses.PushBack(CreateItemStatus("installed", "2.0.0", ItemStateEnum::eFailed));

        return ErrorEnum::eNone;
    }));

    EXPECT_CALL(mImageManager, InstallItem(Field(&imagemanager::UpdateItemInfo::mID, String("item0"))))
        .WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mImageManager, InstallItem(Field(&imagemanager::UpdateItemInfo::mID, String("item1"))))
        .WillOnce(Return(ErrorEnum::eIntegrity));
    EXPECT_CALL(mImageManager, InstallItem(Field(&imagemanager::UpdateItemInfo::mID, String("installed"))))
        .WillOnce(Return(ErrorEnum::eNone));

    StaticArray<InstallItem, 8> installItems;

    auto err = mInstaller.InstallItems(Array<InstanceInfo>(startInstances.data(), startInstances.size()), installItems);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(installItems.Size(), 3);

    for (const auto& item : installItems) {
        if (item.mID == "item1") {
            EXPECT_TRUE(item.mError.Is(ErrorEnum::eIntegrity)) << tests::utils::ErrorToStr(item.mError);
        } else {
            EXPECT_TRUE(item.mError.IsNone()) << tests::utils::ErrorToStr(item.mError);
        }
    }
}

TEST_F(ItemInstallerTest, InstallItemsInstallsAllItemsIfStatusesNotAvailable)
{
    const std::vector<InstanceInfo> startInstances = {CreateInstanceInfo("item0", 0)};

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eNone));

    StaticArray<InstallItem, 8> installItems;

    auto err = mInstaller.InstallItems(Array<InstanceInfo>(startInstances.data(), startInstances.size()), installItems);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(installItems.Size(), 1);
}

TEST_F(ItemInstallerTest, RemoveItemsRemovesEachItem)
{
    const std::vector<ItemVersion> removeItems = {{"item0", "1.0.0"}, {"item1", "2.0.0"}};

    EXPECT_CALL(mImageManager, RemoveItem(String("item0"), String("1.0.0"))).WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_CALL(mImageManager, RemoveItem(String("item1"), String("2.0.0"))).WillOnce(Return(ErrorEnum::eNone));

    mInstaller.RemoveItems(Array<ItemVersion>(removeItems.data(), removeItems.size()));
}

TEST_F(ItemInstallerTest, CanceledInstallerRejectsInstallsUntilStarted)
{
    EXPECT_CALL(mImageManager, Cancel).WillOnce(Return(ErrorEnum::eNone));

    mInstaller.Cancel();

    imagemanager::UpdateItemInfo item {"item0", UpdateItemTypeEnum::eService, "1.0.0", ""};

    EXPECT_TRUE(mInstaller.Install(item).Is(ErrorEnum::eCanceled));

    mInstaller.Start();

    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_TRUE(mInstaller.Install(item).IsNone());
}

TEST_F(ItemInstallerTest, CancelWaitsForInstallInProgress)
{
    std::promise<void> installStarted;
    std::promise<void> canceled;
    auto               canceledFuture = canceled.get_future().share();

    // The install in progress returns only after the image manager is canceled.
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Invoke([&](const imagemanager::UpdateItemInfo&) {
        installStarted.set_value();
        canceledFuture.wait();

        return Error(ErrorEnum::eCanceled);
    }));
    std::atomic_bool cancelSignaled {};

    EXPECT_CALL(mImageManager, Cancel).WillRepeatedly(Invoke([&]() {
        if (!cancelSignaled.exchange(true)) {
            canceled.set_value();
        }

        return ErrorEnum::eNone;
    }));

    imagemanager::UpdateItemInfo item {"item0", UpdateItemTypeEnum::eService, "1.0.0", ""};
    Error                        installErr;

    std::thread installThread([&]() { installErr = mInstaller.Install(item); });

    installStarted.get_future().wait();

    mInstaller.Cancel();

    // Cancel returns only after the install in progress returned.
    installThread.join();

    EXPECT_TRUE(installErr.Is(ErrorEnum::eCanceled));
}

} // namespace aos::sm::launcher
