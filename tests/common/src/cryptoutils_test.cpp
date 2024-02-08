/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/cryptoutils.hpp"

namespace aos {
namespace cryptoutils {

TEST(CryptoutilsTest, ParseScheme)
{
    const char* url1 = "pkcs11:token=aoscore;object=diskencryption;id=2e2769b6-be2c-43ff-b16d-25985a04e6b2?module-path="
                       "/usr/lib/softhsm/libsofthsm2.so&pin-value=42hAGWdIvQr47T8X";
    const char* url2 = "file:/usr/share/.ssh/rsa.pub";
    const char* url3 = "file/usr/share/.ssh/rsa.pub";

    StaticString<30> scheme;

    ASSERT_EQ(ParseURLScheme(url1, scheme), ErrorEnum::eNone);
    EXPECT_EQ(scheme, "pkcs11");

    ASSERT_EQ(ParseURLScheme(url2, scheme), ErrorEnum::eNone);
    EXPECT_EQ(scheme, "file");

    ASSERT_EQ(ParseURLScheme(url3, scheme), ErrorEnum::eNotFound);
}

TEST(CryptoutilsTest, ParseFileURL)
{
    const char* url1 = "file:/usr/share/.ssh/rsa.pub";
    const char* url2 = "pkcs11:token=aoscore";

    StaticString<cFilePathLen> path;

    ASSERT_EQ(ParseFileURL(url1, path), ErrorEnum::eNone);
    EXPECT_EQ(path, "/usr/share/.ssh/rsa.pub");

    ASSERT_NE(ParseFileURL(url2, path), ErrorEnum::eNone);
}

TEST(CryptoutilsTest, ParsePKCS11URL_AllValues)
{
    const char* url1 = "pkcs11:token=aoscore;object=diskencryption;id=2e2769b6-be2c-43ff-b16d-25985a04e6b2?module-path="
                       "/usr/lib/softhsm/libsofthsm2.so&pin-value=42hAGWdIvQr47T8X";

    StaticString<cFilePathLen>       library;
    StaticString<pkcs11::cLabelLen>  token;
    StaticString<pkcs11::cLabelLen>  label;
    uuid::UUID                       id;
    StaticString<pkcs11::cPINLength> userPIN;

    ASSERT_EQ(ParsePKCS11URL(url1, library, token, label, id, userPIN), ErrorEnum::eNone);

    EXPECT_EQ(library, "/usr/lib/softhsm/libsofthsm2.so");
    EXPECT_EQ(token, "aoscore");
    EXPECT_EQ(label, "diskencryption");
    EXPECT_EQ(userPIN, "42hAGWdIvQr47T8X");

    auto strID = uuid::UUIDToString(id);

    EXPECT_EQ(strID.CStr(), std::string("2E2769B6-BE2C-43FF-B16D-25985A04E6B2"));
}

TEST(CryptoutilsTest, ParsePKCS11URL_RequiredValuesOnly)
{
    const char* url1 = "pkcs11:object=diskencryption;id=2e2769b6-be2c-43ff-b16d-25985a04e6b2";

    StaticString<cFilePathLen>       library;
    StaticString<pkcs11::cLabelLen>  token;
    StaticString<pkcs11::cLabelLen>  label;
    uuid::UUID                       id;
    StaticString<pkcs11::cPINLength> userPIN;

    ASSERT_EQ(ParsePKCS11URL(url1, library, token, label, id, userPIN), ErrorEnum::eNone);

    EXPECT_EQ(library, "");
    EXPECT_EQ(token, "");
    EXPECT_EQ(label, "diskencryption");
    EXPECT_EQ(userPIN, "");

    auto strID = uuid::UUIDToString(id);

    EXPECT_EQ(strID.CStr(), std::string("2E2769B6-BE2C-43FF-B16D-25985A04E6B2"));
}

} // namespace cryptoutils
} // namespace aos
