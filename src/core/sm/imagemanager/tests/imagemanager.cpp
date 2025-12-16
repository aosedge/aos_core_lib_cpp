/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

#include <core/sm/imagemanager/imagemanager.hpp>

using namespace testing;

namespace aos::sm::imagemanager {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ImageManagerTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        LOG_INF() << "Image manager size" << Log::Field("size", sizeof(ImageManager));
    }
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

} // namespace aos::sm::imagemanager
