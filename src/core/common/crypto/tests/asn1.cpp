/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <core/common/crypto/itf/asn1.hpp>
#include <core/common/crypto/itf/hash.hpp>
#include <core/common/crypto/itf/x509.hpp>

namespace aos::crypto::asn1 {

TEST(ASN1Test, ConvertTimeToASN1Str)
{
    auto t = Time::Unix(1706702400);

    Error                     err;
    StaticString<cTimeStrLen> str;

    Tie(str, err) = ConvertTimeToASN1Str(t);

    EXPECT_TRUE(err.IsNone());
    EXPECT_EQ(str, "20240131120000Z");
}

TEST(ASN1Test, CompareASN1Values)
{
    const uint8_t content[] = {0x01, 0x02};
    const uint8_t other[]   = {0x01, 0x03};

    const ASN1Value value(0, 4, false, Array<uint8_t>(content, sizeof(content)));
    const ASN1Value copy(value);

    EXPECT_EQ(value.mTagNumber, 4);
    EXPECT_EQ(value, copy);
    EXPECT_NE(value, ASN1Value(0, 4, false, Array<uint8_t>(other, sizeof(other))));
    EXPECT_NE(value, ASN1Value(0, 4, true, Array<uint8_t>(content, sizeof(content))));
    EXPECT_NE(value, ASN1Value(0, 5, false, Array<uint8_t>(content, sizeof(content))));
    EXPECT_NE(value, ASN1Value(1, 4, false, Array<uint8_t>(content, sizeof(content))));
}

TEST(ASN1Test, AlgorithmNamesToString)
{
    EXPECT_STREQ(Hash(HashEnum::eSHA512_224).ToString().CStr(), "SHA512-224");
    EXPECT_STREQ(Hash(HashEnum::eNone).ToString().CStr(), "NONE");
    EXPECT_STREQ(x509::Padding(x509::PaddingEnum::ePKCS1v1_5).ToString().CStr(), "PKCS1v1_5");
}

} // namespace aos::crypto::asn1
