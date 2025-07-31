/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "aos/sm/image/imageparts.hpp"

#include "aos/test/log.hpp"

namespace aos::sm::image {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ImageTest : public ::testing::Test {
protected:
    void SetUp() override { test::InitLog(); }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(ImageTest, GetImageParts)
{
    auto imageParts = std::make_unique<ImageParts>();
    auto manifest   = std::make_unique<oci::ImageManifest>();

    manifest->mConfig.mDigest = "sha256:11111111";
    manifest->mAosService.EmplaceValue("", "sha256:22222222", 0);
    manifest->mLayers.EmplaceBack("", "sha256:33333333", 0);
    manifest->mLayers.EmplaceBack("", "sha256:44444444", 0);
    manifest->mLayers.EmplaceBack("", "sha256:55555555", 0);

    auto err = GetImagePartsFromManifest(*manifest, *imageParts);

    EXPECT_TRUE(err.IsNone());

    EXPECT_EQ(imageParts->mImageConfigPath, String("sha256/11111111"));
    EXPECT_EQ(imageParts->mServiceConfigPath, String("sha256/22222222"));
    EXPECT_EQ(imageParts->mServiceFSPath, String("sha256/33333333"));

    ASSERT_EQ(imageParts->mLayerDigests.Size(), 2);
    EXPECT_EQ(imageParts->mLayerDigests[0], String("sha256:44444444"));
    EXPECT_EQ(imageParts->mLayerDigests[1], String("sha256:55555555"));
}

} // namespace aos::sm::image
