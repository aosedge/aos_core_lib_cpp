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
#include <core/sm/launcher/updateiteminstaller.hpp>
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

class UpdateItemInstallerTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        mInstaller = std::make_unique<UpdateItemInstaller>();

        mInstaller->Init(mAllocator, mImageManager, mItemStatusProvider);
        mInstaller->Start();
    }

    // Installs the item of the instance which isn't installed yet.
    Error InstallItemOf(const InstanceInfo& instance)
    {
        StaticArray<InstallItem, 1> installItems;

        if (auto err = mInstaller->Install(Array<InstanceInfo>(&instance, 1), installItems); !err.IsNone()) {
            return err;
        }

        return installItems.IsEmpty() ? Error(ErrorEnum::eNone) : installItems[0].mError;
    }

    // Repairs the item and returns the items which were repaired.
    StaticArray<InstallItem, 1> Repair(const char* itemID)
    {
        StaticArray<InstallItem, 1> repairItems;

        EXPECT_TRUE(
            repairItems.EmplaceBack(InstallItem {{itemID, UpdateItemTypeEnum::eService, "1.0.0", ""}, {}}).IsNone());

        mInstaller->Repair(repairItems);

        return repairItems;
    }

    HeapAllocator                                          mAllocator;
    StrictMock<imagemanager::ImageManagerMock>             mImageManager;
    NiceMock<common::imagemanager::ItemStatusProviderMock> mItemStatusProvider;
    std::unique_ptr<UpdateItemInstaller>                   mInstaller;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(UpdateItemInstallerTest, GetUnusedItemsReturnsItemsNotUsedByStartedInstances)
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

    mInstaller->GetUnusedItems(*instances, Array<InstanceIdent>(stopInstances.data(), stopInstances.size()),
        Array<InstanceInfo>(startInstances.data(), startInstances.size()), removeItems);

    ASSERT_EQ(removeItems.Size(), 1);
    EXPECT_EQ(removeItems[0].mItemID, String("item0"));
    EXPECT_EQ(removeItems[0].mVersion, String("1.0.0"));
}

TEST_F(UpdateItemInstallerTest, InstallInstallsNotInstalledItemsOnce)
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

    auto err = mInstaller->Install(Array<InstanceInfo>(startInstances.data(), startInstances.size()), installItems);
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

TEST_F(UpdateItemInstallerTest, InstallInstallsAllItemsIfStatusesNotAvailable)
{
    const std::vector<InstanceInfo> startInstances = {CreateInstanceInfo("item0", 0)};

    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eNone));

    StaticArray<InstallItem, 8> installItems;

    auto err = mInstaller->Install(Array<InstanceInfo>(startInstances.data(), startInstances.size()), installItems);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(installItems.Size(), 1);
}

TEST_F(UpdateItemInstallerTest, RemoveRemovesEachItem)
{
    const std::vector<ItemVersion> removeItems = {{"item0", "1.0.0"}, {"item1", "2.0.0"}};

    EXPECT_CALL(mImageManager, RemoveItem(String("item0"), String("1.0.0"))).WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_CALL(mImageManager, RemoveItem(String("item1"), String("2.0.0"))).WillOnce(Return(ErrorEnum::eNone));

    mInstaller->Remove(Array<ItemVersion>(removeItems.data(), removeItems.size()));
}

TEST_F(UpdateItemInstallerTest, CanceledInstallerRejectsInstallsUntilStarted)
{
    EXPECT_CALL(mImageManager, Cancel).WillOnce(Return(ErrorEnum::eNone));

    mInstaller->Cancel();

    EXPECT_TRUE(InstallItemOf(CreateInstanceInfo("item0", 0)).Is(ErrorEnum::eCanceled));

    mInstaller->Start();

    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_TRUE(InstallItemOf(CreateInstanceInfo("item0", 0)).IsNone());
}

TEST_F(UpdateItemInstallerTest, CancelWaitsForInstallInProgress)
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

    Error installErr;

    std::thread installThread([&]() { installErr = InstallItemOf(CreateInstanceInfo("item0", 0)); });

    installStarted.get_future().wait();

    mInstaller->Cancel();

    // Cancel returns only after the install in progress returned.
    installThread.join();

    EXPECT_TRUE(installErr.Is(ErrorEnum::eCanceled));
}

TEST_F(UpdateItemInstallerTest, RepairsItemOnceUntilNextRequest)
{
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eNone));

    auto repaired = Repair("item0");

    ASSERT_EQ(repaired.Size(), 1);
    EXPECT_TRUE(repaired[0].mError.IsNone()) << tests::utils::ErrorToStr(repaired[0].mError);

    // The repaired item isn't repaired again.
    EXPECT_TRUE(Repair("item0").IsEmpty());

    // The next request for the installed item allows to repair it again.
    EXPECT_CALL(mItemStatusProvider, GetItemsStatuses).WillOnce(Invoke([](Array<UpdateItemStatus>& statuses) {
        statuses.PushBack(CreateItemStatus("item0", "1.0.0", ItemStateEnum::eInstalled));

        return ErrorEnum::eNone;
    }));

    EXPECT_TRUE(InstallItemOf(CreateInstanceInfo("item0", 0)).IsNone());

    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eIntegrity));

    repaired = Repair("item0");

    ASSERT_EQ(repaired.Size(), 1);
    EXPECT_TRUE(repaired[0].mError.Is(ErrorEnum::eIntegrity)) << tests::utils::ErrorToStr(repaired[0].mError);
}

TEST_F(UpdateItemInstallerTest, DoesNotRepairItemFailedToInstall)
{
    EXPECT_CALL(mImageManager, InstallItem).WillOnce(Return(ErrorEnum::eIntegrity));

    EXPECT_TRUE(InstallItemOf(CreateInstanceInfo("item0", 0)).Is(ErrorEnum::eIntegrity));

    EXPECT_TRUE(Repair("item0").IsEmpty());
}

} // namespace aos::sm::launcher
