/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/iam/certmodules/certmodule.hpp"

#include "log.hpp"
#include "mocks/hsmmock.hpp"
#include "mocks/x509providermock.hpp"
#include "storagestub.hpp"

using namespace aos;
using namespace aos::iam::certhandler;
using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CertModuleTest : public Test {
protected:
    static constexpr auto cCertType   = "test-cert-type";
    static constexpr auto cCertIssuer = "test-cert-issuer";

    void SetUp() override
    {
        aos::InitLog();

        mModuleConfig.mMaxCertificates = 2;

        mCertInfo.mIssuer   = String(cCertIssuer).AsByteArray();
        mCertInfo.mNotAfter = Time::Now();
    }

    CertInfo mCertInfo;

    ModuleConfig    mModuleConfig;
    ProviderItfMock mX509Provider;
    HSMItfMock      mHSM;
    StorageStub     mStorage;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CertModuleTest, InitSucceeds)
{
    CertModule certModule;

    auto err = certModule.Init(cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();
}

TEST_F(CertModuleTest, InitSelfSignedModuleSucceeds)
{
    CertModule certModule;

    mModuleConfig.mIsSelfSigned    = true;
    mModuleConfig.mMaxCertificates = 1;

    auto err = certModule.Init(cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();
}

TEST_F(CertModuleTest, InitFailsOnOneMaxCertsConfigValueForNonSelfSignedModule)
{
    CertModule certModule;

    mModuleConfig.mIsSelfSigned    = false;
    mModuleConfig.mMaxCertificates = 1;

    auto err = certModule.Init(cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eInvalidArgument, err) << "Invalid argument expected: " << err.StrValue();
}

TEST_F(CertModuleTest, InitFailsOnZeroMaxCertsConfigValue)
{
    CertModule certModule;

    mModuleConfig.mMaxCertificates = 0;

    auto err = certModule.Init(cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eInvalidArgument, err) << "Invalid argument expected: " << err.StrValue();
}

TEST_F(CertModuleTest, InitFailsNoMemory)
{
    CertModule certModule;

    mModuleConfig.mMaxCertificates = cCertsPerModule + 1;

    auto err = certModule.Init(cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNoMemory, err) << "No memory expected: " << err.StrValue();
}

TEST_F(CertModuleTest, ApplyCert)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation = true;

    auto err = certModule.Init(cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();

    EXPECT_CALL(mX509Provider, ASN1DecodeDN).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mX509Provider, PEMToX509Certs)
        .WillOnce(Invoke([&](const String& pemBlob, Array<crypto::x509::Certificate>& resultCerts) {
            (void)pemBlob;
            (void)resultCerts;

            resultCerts.EmplaceBack();
            resultCerts[0].mIssuer  = mCertInfo.mIssuer;
            resultCerts[0].mSubject = mCertInfo.mIssuer;

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mHSM, ApplyCert).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHSM, RemoveCert).Times(0);
    EXPECT_CALL(mHSM, RemoveKey).Times(0);

    String pemCert;

    err = certModule.ApplyCert(pemCert, mCertInfo);
    ASSERT_TRUE(err.IsNone()) << "ApplyCert failed: " << err.StrValue();

    StaticArray<CertInfo, 1> certInfoArray;

    err = mStorage.GetCertsInfo(cCertType, certInfoArray);
    ASSERT_TRUE(err.IsNone()) << "GetCertsInfo failed: " << err.StrValue();

    ASSERT_EQ(1, certInfoArray.Size());
    EXPECT_EQ(mCertInfo, certInfoArray[0]) << "CertInfo mismatch";
}

TEST_F(CertModuleTest, ApplyCertOldCertsAreTrimmed)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation = true;

    auto err = certModule.Init(cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();

    err = mStorage.AddCertInfo(cCertType, mCertInfo);
    ASSERT_TRUE(err.IsNone()) << "AddCertInfo failed: " << err.StrValue();

    EXPECT_CALL(mX509Provider, ASN1DecodeDN).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mX509Provider, PEMToX509Certs)
        .WillOnce(Invoke([&](const String& pemBlob, Array<crypto::x509::Certificate>& resultCerts) {
            (void)pemBlob;
            (void)resultCerts;

            resultCerts.EmplaceBack();
            resultCerts[0].mIssuer  = mCertInfo.mIssuer;
            resultCerts[0].mSubject = mCertInfo.mIssuer;

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mHSM, ApplyCert).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mHSM, RemoveCert).Times(0);
    EXPECT_CALL(mHSM, RemoveKey).Times(0);

    String pemCert;

    mCertInfo.mNotAfter = Time::Now();

    err = certModule.ApplyCert(pemCert, mCertInfo);
    ASSERT_TRUE(err.IsNone()) << "ApplyCert failed: " << err.StrValue();
}

TEST_F(CertModuleTest, ApplyCertOldCertsAreTrimmedOnMaxCertsLimitReached)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = cCertsPerModule;

    auto err = certModule.Init(cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();

    for (size_t i = 0; i < cCertsPerModule; ++i) {
        mCertInfo.mNotAfter = Time::Now().Add(Time::cHours * i);

        err = mStorage.AddCertInfo(cCertType, mCertInfo);
        ASSERT_TRUE(err.IsNone()) << "AddCertInfo failed: " << err.StrValue();
    }

    EXPECT_CALL(mX509Provider, ASN1DecodeDN).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mX509Provider, PEMToX509Certs)
        .WillOnce(Invoke([&](const String& pemBlob, Array<crypto::x509::Certificate>& resultCerts) {
            (void)pemBlob;
            (void)resultCerts;

            resultCerts.EmplaceBack();
            resultCerts[0].mIssuer  = mCertInfo.mIssuer;
            resultCerts[0].mSubject = mCertInfo.mIssuer;

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mHSM, ApplyCert).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mHSM, RemoveCert).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHSM, RemoveKey).WillOnce(Return(ErrorEnum::eNone));

    String pemCert;

    mCertInfo.mNotAfter = Time::Now().Add(Time::cHours * cCertsPerModule);

    err = certModule.ApplyCert(pemCert, mCertInfo);
    ASSERT_TRUE(err.IsNone()) << "ApplyCert failed: " << err.StrValue();

    StaticArray<CertInfo, cCertsPerModule> certInfoArray;

    err = mStorage.GetCertsInfo(cCertType, certInfoArray);
    ASSERT_TRUE(err.IsNone()) << "GetCertsInfo failed: " << err.StrValue();

    ASSERT_EQ(cCertsPerModule, certInfoArray.Size());
}
