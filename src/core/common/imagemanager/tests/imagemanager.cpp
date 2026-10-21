/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/tests/mocks/fileinfoprovidermock.hpp>
#include <core/common/tests/mocks/fsmock.hpp>
#include <core/common/tests/mocks/itemstatuslistenermock.hpp>
#include <core/common/tests/mocks/ocispecmock.hpp>
#include <core/common/tests/mocks/spaceallocatormock.hpp>
#include <core/common/tests/stubs/imagemanagerstoragestub.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>

#include <core/common/imagemanager/imagemanager.hpp>

using namespace testing;

namespace aos::common::imagemanager {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cTestImagePath = "/tmp/common_imagemanager_test/images";

/***********************************************************************************************************************
 * Test image manager
 **********************************************************************************************************************/

class TestImageManager : public ImageManager {
public:
    using ImageManager::Init;
    using ImageManager::InstallItemInfo;

    Error Install(const InstallItemInfo& itemInfo) { return InstallItem(itemInfo); }
    Error Remove(const String& itemID, const String& version) { return RemoveItem(itemID, version); }

    MOCK_METHOD(Error, DownloadBlob, (const String& digest, const String& path), (override));
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CommonImageManagerTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    HeapAllocator                                mAllocator;
    StorageStub                                  mStorage;
    NiceMock<spaceallocator::SpaceAllocatorMock> mSpaceAllocator;
    NiceMock<fs::FileInfoProviderMock>           mFileInfoProvider;
    NiceMock<oci::OCISpecMock>                   mOCISpec;
    NiceMock<FSPlatformMock>                     mFSPlatform;
    NiceMock<ItemStatusListenerMock>             mListener;
    NiceMock<TestImageManager>                   mImageManager;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CommonImageManagerTest, Init)
{
    Config config {cTestImagePath, 0, 10 * Time::cSeconds, 5 * Time::cSeconds};

    auto err
        = mImageManager.Init(config, mStorage, mSpaceAllocator, mFileInfoProvider, mOCISpec, mFSPlatform, mAllocator);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

} // namespace aos::common::imagemanager
