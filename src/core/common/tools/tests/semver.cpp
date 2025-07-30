/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/semver.hpp"

namespace aos::semver {

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST(SemverTest, ValidateSemver)
{
    struct TestData {
        String mVersion;
        Error  err;
    };

    std::vector<TestData> testData = {
        {"1.0.0", ErrorEnum::eNone},
        {"2.0.0-alpha.2", ErrorEnum::eNone},
        {"2.0.0-beta.3", ErrorEnum::eNone},
        {"1.0.0-beta+exp.sha.5114f85", ErrorEnum::eNone},
        {"01.0.0", ErrorEnum::eInvalidArgument},
        {"01.0.0.12", ErrorEnum::eInvalidArgument},
        {"1.0.0-01.2.3", ErrorEnum::eInvalidArgument},
    };

    for (const auto& data : testData) {
        EXPECT_EQ(ValidateSemver(data.mVersion), data.err);
    }
}

TEST(SemverTest, CompareSemver)
{
    struct TestData {
        String            mVersion1;
        String            mVersion2;
        RetWithError<int> mResult;
    };

    std::vector<TestData> testData = {
        {"1.0.0", "1.0.0", 0},
        {"1.0.1", "1.0.0", 1},
        {"1.0.0", "1.0.1", -1},
        {"1.0.0", "1.0.0-beta", 1},
        {"1.0.0-alpha", "1.0.0", -1},
        {"1.0.0-beta", "1.0.0-beta", 0},
        {"1.0.0-alpha", "1.0.0-beta", -1},
        {"1.0.0-alpha", "1.0.0-alpha.1", -1},
        {"1.0.0-alpha.1", "1.0.0-alpha.beta", -1},
        {"1.0.0-alpha.beta+0123", "1.0.0-alpha.beta+fdda", 0},
    };

    for (const auto& data : testData) {
        EXPECT_EQ(CompareSemver(data.mVersion1, data.mVersion2), data.mResult);
    }
}

} // namespace aos::semver
