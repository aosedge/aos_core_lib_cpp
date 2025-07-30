/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/common/crypto/crypto.hpp"

namespace aos::crypto::asn1 {

TEST(CryptoTest, ConvertTimeToASN1Str)
{
    auto t = Time::Unix(1706702400);

    Error                     err;
    StaticString<cTimeStrLen> str;

    Tie(str, err) = ConvertTimeToASN1Str(t);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(str, "20240131120000Z");
}

} // namespace aos::crypto::asn1
