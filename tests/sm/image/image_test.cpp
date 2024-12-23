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
    auto manifest = std::make_unique<oci::ImageManifest>();

    manifest->mConfig.mDigest = "sha256:11111111";
    manifest->mAosService.EmplaceValue("", "sha256:22222222", 0);
    manifest->mLayers.EmplaceBack("", "sha256:33333333", 0);

    auto [imageParts, err] = GetImagePartsFromManifest(*manifest);

    EXPECT_TRUE(err.IsNone());

    EXPECT_EQ(imageParts.mImageConfigPath, String("sha256/11111111"));
    EXPECT_EQ(imageParts.mServiceConfigPath, String("sha256/22222222"));
    EXPECT_EQ(imageParts.mServiceFSPath, String("sha256/33333333"));
}

} // namespace aos::sm::image
