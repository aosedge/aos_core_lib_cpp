/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/mocks/iteminfoprovidermock.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/sm/launcher/itemconfigloader.hpp>

using namespace testing;

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ItemConfigLoaderTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        // The blob path is the blob digest, so the tests check which blob is loaded.
        ON_CALL(mItemInfoProvider, GetBlobPath).WillByDefault(Invoke([](const String& digest, String& path) {
            return path.Assign(digest);
        }));

        mManifest                  = std::make_unique<oci::ImageManifest>();
        mManifest->mConfig.mDigest = "imageConfigDigest";
        mManifest->mItemConfig.EmplaceValue();
        mManifest->mItemConfig->mDigest = "itemConfigDigest";

        mLoader.Init(mAllocator, mItemInfoProvider, mOCISpec);
    }

    HeapAllocator                                        mAllocator;
    NiceMock<common::imagemanager::ItemInfoProviderMock> mItemInfoProvider;
    StrictMock<oci::OCISpecMock>                         mOCISpec;
    std::unique_ptr<oci::ImageManifest>                  mManifest;
    ItemConfigLoader                                     mLoader;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(ItemConfigLoaderTest, LoadsConfigsReferencedByManifest)
{
    auto expectedItemConfig  = std::make_unique<oci::ItemConfig>();
    auto expectedImageConfig = std::make_unique<oci::ImageConfig>();

    expectedItemConfig->mOfflineTTL = 10 * Time::cSeconds;
    expectedImageConfig->mConfig.mEntryPoint.EmplaceBack("/bin/service");

    EXPECT_CALL(mOCISpec, LoadImageManifest(String("manifestDigest"), _))
        .WillOnce(DoAll(SetArgReferee<1>(*mManifest), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mOCISpec, LoadItemConfig(String("itemConfigDigest"), _))
        .WillOnce(DoAll(SetArgReferee<1>(*expectedItemConfig), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mOCISpec, LoadImageConfig(String("imageConfigDigest"), _))
        .WillOnce(DoAll(SetArgReferee<1>(*expectedImageConfig), Return(ErrorEnum::eNone)));

    auto itemConfig  = std::make_unique<oci::ItemConfig>();
    auto imageConfig = std::make_unique<oci::ImageConfig>();

    auto err = mLoader.Load("manifestDigest", *itemConfig, *imageConfig);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_EQ(itemConfig->mOfflineTTL, expectedItemConfig->mOfflineTTL);
    EXPECT_EQ(*imageConfig, *expectedImageConfig);
}

TEST_F(ItemConfigLoaderTest, FailsIfManifestHasNoItemConfig)
{
    mManifest->mItemConfig.Reset();

    EXPECT_CALL(mOCISpec, LoadImageManifest).WillOnce(DoAll(SetArgReferee<1>(*mManifest), Return(ErrorEnum::eNone)));

    auto itemConfig  = std::make_unique<oci::ItemConfig>();
    auto imageConfig = std::make_unique<oci::ImageConfig>();

    EXPECT_TRUE(mLoader.Load("manifestDigest", *itemConfig, *imageConfig).Is(ErrorEnum::eNotFound));
}

TEST_F(ItemConfigLoaderTest, ReturnsErrorOfFailedStep)
{
    auto itemConfig  = std::make_unique<oci::ItemConfig>();
    auto imageConfig = std::make_unique<oci::ImageConfig>();

    // The blob path of the manifest can't be resolved.
    EXPECT_CALL(mItemInfoProvider, GetBlobPath(String("manifestDigest"), _)).WillOnce(Return(ErrorEnum::eNotFound));

    EXPECT_TRUE(mLoader.Load("manifestDigest", *itemConfig, *imageConfig).Is(ErrorEnum::eNotFound));

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mItemInfoProvider));

    // The item config blob is broken.
    ON_CALL(mItemInfoProvider, GetBlobPath).WillByDefault(Invoke([](const String& digest, String& path) {
        return path.Assign(digest);
    }));

    EXPECT_CALL(mOCISpec, LoadImageManifest).WillOnce(DoAll(SetArgReferee<1>(*mManifest), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mOCISpec, LoadItemConfig).WillOnce(Return(ErrorEnum::eIntegrity));

    EXPECT_TRUE(mLoader.Load("manifestDigest", *itemConfig, *imageConfig).Is(ErrorEnum::eIntegrity));
}

} // namespace aos::sm::launcher
