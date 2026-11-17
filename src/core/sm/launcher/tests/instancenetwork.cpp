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
#include <core/common/tools/heapallocator.hpp>
#include <core/sm/launcher/instancenetwork.hpp>
#include <core/sm/tests/mocks/networkmanagermock.hpp>
#include <core/sm/tests/mocks/resourcemanagermock.hpp>

using namespace testing;

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class InstanceNetworkTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        mInstance = std::make_unique<InstanceData>();

        mInstance->mInfo.mItemID   = "item0";
        mInstance->mInfo.mInstance = 1;
        mInstance->mInfo.mOwnerID  = "owner0";
        mInstance->mInstanceID     = "instanceID0";

        mItemConfig  = std::make_unique<oci::ItemConfig>();
        mImageConfig = std::make_unique<oci::ImageConfig>();

        mNetwork.Init(mAllocator, mNetworkManager, mResourceInfoProvider);
    }

    HeapAllocator                                         mAllocator;
    StrictMock<networkmanager::NetworkManagerMock>        mNetworkManager;
    StrictMock<resourcemanager::ResourceInfoProviderMock> mResourceInfoProvider;
    std::unique_ptr<InstanceData>                         mInstance;
    std::unique_ptr<oci::ItemConfig>                      mItemConfig;
    std::unique_ptr<oci::ImageConfig>                     mImageConfig;
    InstanceNetwork                                       mNetwork;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(InstanceNetworkTest, CreateConfiguresNetworkFromItemAndImageConfigs)
{
    mItemConfig->mHostname.EmplaceValue("hostname0");
    mItemConfig->mQuotas.mDownloadSpeed.EmplaceValue(100);
    mItemConfig->mQuotas.mUploadSpeed.EmplaceValue(200);
    mItemConfig->mQuotas.mDownloadLimit.EmplaceValue(300);
    mItemConfig->mQuotas.mUploadLimit.EmplaceValue(400);
    mItemConfig->mAllowedConnections.EmplaceBack("item1/8080/tcp");
    mItemConfig->mResources.EmplaceBack();
    mItemConfig->mResources.Back().mName = "resource0";
    mItemConfig->mResources.EmplaceBack();
    mItemConfig->mResources.Back().mName = "resource1";
    mImageConfig->mConfig.mExposedPorts.EmplaceBack("8080/tcp");

    EXPECT_CALL(mResourceInfoProvider, GetResourceInfo(String("resource0"), _))
        .WillOnce(Invoke([](const String&, resourcemanager::ResourceInfo& info) {
            info.mHosts.Clear();
            info.mHosts.EmplaceBack("10.0.0.1", "host0");

            return ErrorEnum::eNone;
        }));
    EXPECT_CALL(mResourceInfoProvider, GetResourceInfo(String("resource1"), _))
        .WillOnce(Invoke([](const String&, resourcemanager::ResourceInfo& info) {
            info.mHosts.Clear();
            info.mHosts.EmplaceBack("10.0.0.2", "host1");

            return ErrorEnum::eNone;
        }));

    auto config = std::make_unique<networkmanager::InstanceNetworkConfig>();

    EXPECT_CALL(mNetworkManager, CreateInstanceNetwork(String("instanceID0"), String("owner0"), _))
        .WillOnce(DoAll(SaveArg<2>(config.get()), Return(ErrorEnum::eNone)));

    auto err = mNetwork.Create(*mInstance, *mItemConfig, *mImageConfig);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(config->mInstanceIdent, static_cast<const InstanceIdent&>(mInstance->mInfo));
    EXPECT_EQ(config->mHostname, String("hostname0"));
    EXPECT_EQ(config->mIngressKbit, 100);
    EXPECT_EQ(config->mEgressKbit, 200);
    EXPECT_EQ(config->mDownloadLimit, 300);
    EXPECT_EQ(config->mUploadLimit, 400);
    ASSERT_EQ(config->mHosts.Size(), 2);
    EXPECT_EQ(config->mHosts[0], Host("10.0.0.1", "host0"));
    EXPECT_EQ(config->mHosts[1], Host("10.0.0.2", "host1"));
    ASSERT_EQ(config->mExposedPorts.Size(), 1);
    EXPECT_EQ(config->mExposedPorts[0], String("8080/tcp"));
    ASSERT_EQ(config->mAllowedConnections.Size(), 1);
    EXPECT_EQ(config->mAllowedConnections[0], String("item1/8080/tcp"));
}

TEST_F(InstanceNetworkTest, CreateFailsIfResourceInfoNotFound)
{
    mItemConfig->mResources.EmplaceBack();
    mItemConfig->mResources.Back().mName = "resource0";

    EXPECT_CALL(mResourceInfoProvider, GetResourceInfo).WillOnce(Return(ErrorEnum::eNotFound));

    EXPECT_TRUE(mNetwork.Create(*mInstance, *mItemConfig, *mImageConfig).Is(ErrorEnum::eNotFound));
}

TEST_F(InstanceNetworkTest, CreateIgnoresExistingNetwork)
{
    EXPECT_CALL(mNetworkManager, CreateInstanceNetwork)
        .WillOnce(Return(ErrorEnum::eAlreadyExist))
        .WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(mNetwork.Create(*mInstance, *mItemConfig, *mImageConfig).IsNone());
    EXPECT_TRUE(mNetwork.Create(*mInstance, *mItemConfig, *mImageConfig).Is(ErrorEnum::eFailed));
}

TEST_F(InstanceNetworkTest, StartIgnoresStartedNetwork)
{
    EXPECT_CALL(mNetworkManager, StartInstanceNetwork(String("instanceID0"), String("owner0")))
        .WillOnce(Return(ErrorEnum::eAlreadyExist))
        .WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(mNetwork.Start(*mInstance).IsNone());
    EXPECT_TRUE(mNetwork.Start(*mInstance).Is(ErrorEnum::eFailed));
}

TEST_F(InstanceNetworkTest, StopIgnoresNotFoundNetwork)
{
    EXPECT_CALL(mNetworkManager, StopInstanceNetwork(String("instanceID0"), String("owner0")))
        .WillOnce(Return(ErrorEnum::eNotFound))
        .WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(mNetwork.Stop(*mInstance).IsNone());
    EXPECT_TRUE(mNetwork.Stop(*mInstance).Is(ErrorEnum::eFailed));
}

TEST_F(InstanceNetworkTest, ReleaseReturnsAnyError)
{
    EXPECT_CALL(mNetworkManager, ReleaseInstanceNetwork(String("instanceID0"), String("owner0")))
        .WillOnce(Return(ErrorEnum::eNone))
        .WillOnce(Return(ErrorEnum::eNotFound));

    EXPECT_TRUE(mNetwork.Release(*mInstance).IsNone());
    EXPECT_TRUE(mNetwork.Release(*mInstance).Is(ErrorEnum::eNotFound));
}

} // namespace aos::sm::launcher
