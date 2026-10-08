/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include <core/common/tests/mocks/cryptomock.hpp>
#include <core/common/tests/stubs/testallocator.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/iam/certhandler/certmodule.hpp>
#include <core/iam/tests/mocks/certhandlermock.hpp>
#include <core/iam/tests/stubs/certhandlerstub.hpp>

using namespace aos;
using namespace aos::iam::certhandler;
using namespace aos::tests::utils;
using namespace testing;
using aos::tests::TestAllocator;

/***********************************************************************************************************************
 * Test helpers
 **********************************************************************************************************************/

class PrivateKeyStub : public crypto::PrivateKeyItf {
public:
    const crypto::PublicKeyItf& GetPublic() const override { return mPublic; }

    Error Sign(const Array<uint8_t>&, const crypto::SignOptions&, Array<uint8_t>&) const override
    {
        return ErrorEnum::eNone;
    }

    Error Decrypt(const Array<uint8_t>&, const crypto::DecryptionOptions&, Array<uint8_t>&) const override
    {
        return ErrorEnum::eNone;
    }

private:
    crypto::RSAPublicKey mPublic {Array<uint8_t>(), Array<uint8_t>()};
};

class FailingStorageStub : public StorageStub {
public:
    void FailAdd() { mFailAdd = true; }
    void FailGet() { mFailGet = true; }
    void FailRemove() { mFailRemove = true; }

    Error AddCertInfo(const String& certType, const CertInfo& certInfo) override
    {
        if (mFailAdd) {
            return ErrorEnum::eFailed;
        }

        return StorageStub::AddCertInfo(certType, certInfo);
    }

    Error GetCertsInfo(const String& certType, Array<CertInfo>& certsInfo) override
    {
        if (mFailGet) {
            return ErrorEnum::eFailed;
        }

        return StorageStub::GetCertsInfo(certType, certsInfo);
    }

    Error RemoveCertInfo(const String& certType, const String& certURL) override
    {
        if (mFailRemove) {
            return ErrorEnum::eFailed;
        }

        return StorageStub::RemoveCertInfo(certType, certURL);
    }

private:
    bool mFailAdd {};
    bool mFailGet {};
    bool mFailRemove {};
};

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CertModuleTest : public Test {
protected:
    // cppcheck-suppress unusedStructMember
    static constexpr auto cCertType   = "test-cert-type";
    static constexpr auto cCertIssuer = "test-cert-issuer";

    void SetUp() override
    {
        tests::utils::InitLog();

        mAllocator.Reset();
        mModuleConfig.mMaxCertificates = 2;

        mCertInfo.mIssuer   = String(cCertIssuer).AsByteArray();
        mCertInfo.mNotAfter = Time::Now();
    }

    // cppcheck-suppress unusedStructMember
    TestAllocator mAllocator;

    CertInfo mCertInfo;

    // cppcheck-suppress unusedStructMember
    ModuleConfig mModuleConfig;
    // cppcheck-suppress unusedStructMember
    crypto::x509::ProviderMock mX509Provider;
    // cppcheck-suppress unusedStructMember
    HSMMock     mHSM;
    StorageStub mStorage;
};

/***********************************************************************************************************************
 * Helpers
 **********************************************************************************************************************/

auto ReturnPEMCert(const char* issuer, const char* serial = nullptr, const char* subject = nullptr)
{
    return Invoke([=](const String&, Array<crypto::x509::Certificate>& resultCerts) {
        resultCerts.EmplaceBack();
        resultCerts[0].mIssuer = String(issuer).AsByteArray();

        if (serial) {
            resultCerts[0].mSerial = String(serial).AsByteArray();
        }

        if (subject) {
            resultCerts[0].mSubject = String(subject).AsByteArray();
        }

        return ErrorEnum::eNone;
    });
}

auto ReturnPEMCerts(size_t count)
{
    return Invoke([=](const String&, Array<crypto::x509::Certificate>& resultCerts) {
        for (size_t i = 0; i < count; ++i) {
            resultCerts.EmplaceBack();
        }

        return ErrorEnum::eNone;
    });
}

CertInfo CreateCertInfo(const char* issuer, const char* serial, const char* certURL, const Time& notAfter = Time())
{
    CertInfo info;

    info.mIssuer   = String(issuer).AsByteArray();
    info.mSerial   = String(serial).AsByteArray();
    info.mCertURL  = certURL;
    info.mNotAfter = notAfter;

    return info;
}

auto ReturnAddCert(const char* certURL, const Time& notAfter = Time())
{
    return Invoke([=](const crypto::x509::Certificate& cert, const String&, CertInfo& resCert) {
        resCert.mIssuer   = cert.mIssuer;
        resCert.mSerial   = cert.mSerial;
        resCert.mCertURL  = certURL;
        resCert.mNotAfter = notAfter;

        return ErrorEnum::eNone;
    });
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CertModuleTest, InitSucceeds)
{
    CertModule certModule;

    auto err = certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();
}

TEST_F(CertModuleTest, InitSelfSignedModuleSucceeds)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eSelfSigned;
    mModuleConfig.mMaxCertificates = 1;

    auto err = certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();
}

TEST_F(CertModuleTest, InitFailsOnOneMaxCertsConfigValueForNonSelfSignedModule)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eCertKeyPair;
    mModuleConfig.mMaxCertificates = 1;

    auto err = certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eInvalidArgument, err) << "Invalid argument expected: " << err.StrValue();
}

TEST_F(CertModuleTest, InitFailsOnZeroMaxCertsConfigValue)
{
    CertModule certModule;

    mModuleConfig.mMaxCertificates = 0;

    auto err = certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eInvalidArgument, err) << "Invalid argument expected: " << err.StrValue();
}

TEST_F(CertModuleTest, InitFailsNoMemory)
{
    CertModule certModule;

    mModuleConfig.mMaxCertificates = cCertsPerModule + 1;

    auto err = certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNoMemory, err) << "No memory expected: " << err.StrValue();
}

TEST_F(CertModuleTest, ApplyCert)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation = true;

    auto err = certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();

    EXPECT_CALL(mX509Provider, ASN1DecodeDN).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, nullptr, cCertIssuer));

    EXPECT_CALL(mHSM, ApplyCert).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHSM, RemoveCert).Times(0);
    EXPECT_CALL(mHSM, RemoveKey).Times(0);

    String pemCert;

    err = certModule.ApplyCert(pemCert, mCertInfo);
    ASSERT_TRUE(err.IsNone()) << "ApplyCert failed: " << err.StrValue();

    StaticArray<CertInfo, 1> certInfoArray;

    err = mStorage.GetCertsInfo(cCertType, certInfoArray);
    ASSERT_TRUE(err.IsNone()) << "GetCertsInfo failed: " << err.StrValue();

    EXPECT_EQ(certInfoArray, ConvertToArray({mCertInfo})) << "CertInfo mismatch";
}

TEST_F(CertModuleTest, ApplyCertOldCertsAreTrimmed)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation = true;

    auto err = certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();

    err = mStorage.AddCertInfo(cCertType, mCertInfo);
    ASSERT_TRUE(err.IsNone()) << "AddCertInfo failed: " << err.StrValue();

    EXPECT_CALL(mX509Provider, ASN1DecodeDN).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, nullptr, cCertIssuer));

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

    auto err = certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();

    for (size_t i = 0; i < cCertsPerModule; ++i) {
        mCertInfo.mNotAfter = Time::Now().Add(Time::cHours * i);

        err = mStorage.AddCertInfo(cCertType, mCertInfo);
        ASSERT_TRUE(err.IsNone()) << "AddCertInfo failed: " << err.StrValue();
    }

    EXPECT_CALL(mX509Provider, ASN1DecodeDN).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, nullptr, cCertIssuer));

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

TEST_F(CertModuleTest, RootModuleCreateKeyNotSupported)
{
    CertModule certModule;

    mModuleConfig.mCertType       = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mSkipValidation = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    EXPECT_CALL(mHSM, CreateKey).Times(0);

    EXPECT_EQ(certModule.CreateKey("password"),
        RetWithError<SharedPtr<crypto::PrivateKeyItf>>(nullptr, ErrorEnum::eNotSupported));
}

TEST_F(CertModuleTest, CreateKeyFailsWhenHSMFails)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mHSM, CreateKey)
        .WillOnce(Return(RetWithError<SharedPtr<crypto::PrivateKeyItf>> {nullptr, ErrorEnum::eFailed}));

    EXPECT_EQ(
        certModule.CreateKey("password"), RetWithError<SharedPtr<crypto::PrivateKeyItf>>(nullptr, ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, CreateSelfSignedCertFailsWhenCreateKeyFails)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eSelfSigned;
    mModuleConfig.mMaxCertificates = 1;
    mModuleConfig.mSkipValidation  = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mHSM, CreateKey)
        .WillOnce(Return(RetWithError<SharedPtr<crypto::PrivateKeyItf>> {nullptr, ErrorEnum::eFailed}));

    ASSERT_TRUE(certModule.CreateSelfSignedCert("password").Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, RootModuleApplyCertNotSupported)
{
    CertModule certModule;

    mModuleConfig.mCertType       = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mSkipValidation = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    EXPECT_CALL(mHSM, ApplyCert).Times(0);

    String pemCert;

    ASSERT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eNotSupported));
}

TEST_F(CertModuleTest, RootModuleCreateSelfSignedCertNotSupported)
{
    CertModule certModule;

    mModuleConfig.mCertType       = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mSkipValidation = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    EXPECT_CALL(mHSM, CreateKey).Times(0);
    EXPECT_CALL(mHSM, ApplyCert).Times(0);

    ASSERT_TRUE(certModule.CreateSelfSignedCert("password").Is(ErrorEnum::eNotSupported));
}

TEST_F(CertModuleTest, CertModuleTypeRootToString)
{
    EXPECT_EQ(CertModuleType(CertModuleTypeEnum::eRootCerts).ToString(), "rootCerts");
}

TEST_F(CertModuleTest, GetCertificates)
{
    CertModule                             certModule;
    StaticArray<CertInfo, cCertsPerModule> infos;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    ASSERT_TRUE(certModule.GetCertificates(infos).Is(ErrorEnum::eNotFound));

    const auto cert = CreateCertInfo(cCertIssuer, "serial-1", "cert-url-1", mCertInfo.mNotAfter);

    ASSERT_TRUE(mStorage.AddCertInfo(cCertType, cert).IsNone());

    ASSERT_TRUE(certModule.GetCertificates(infos).IsNone());
    EXPECT_EQ(infos, ConvertToArray({cert}));
}

TEST_F(CertModuleTest, InitRootCallsValidateRootCertificates)
{
    CertModule certModule;

    mModuleConfig.mCertType       = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mSkipValidation = false;
    EXPECT_CALL(mHSM, ValidateRootCertificates).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHSM, ValidateCertificates).Times(0);

    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
}

TEST_F(CertModuleTest, UpdateCertsAddsNewCert)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;
    StaticArray<CertInfo, cCertsPerModule>            storedCerts;

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-new"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("new-cert-url"));

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).IsNone());
    EXPECT_EQ(infos, ConvertToArray({CreateCertInfo(cCertIssuer, "serial-new", "new-cert-url")}));

    ASSERT_TRUE(mStorage.GetCertsInfo(cCertType, storedCerts).IsNone());
    EXPECT_EQ(storedCerts, infos);
}

TEST_F(CertModuleTest, UpdateCertsReusesExisting)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;
    const auto existing = CreateCertInfo(cCertIssuer, "serial-existing", "existing-cert-url", mCertInfo.mNotAfter);

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-existing"));
    EXPECT_CALL(mHSM, AddCert).Times(0);

    ASSERT_TRUE(mStorage.AddCertInfo(cCertType, existing).IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).IsNone());
    EXPECT_EQ(infos, ConvertToArray({existing}));
}

TEST_F(CertModuleTest, UpdateCertsRemovesObsolete)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;
    StaticArray<CertInfo, cCertsPerModule>            storedCerts;

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-b"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("new-cert-url"));
    EXPECT_CALL(mHSM, RemoveCert(String("old-cert-url"), _)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_TRUE(
        mStorage.AddCertInfo(cCertType, CreateCertInfo(cCertIssuer, "serial-a", "old-cert-url", mCertInfo.mNotAfter))
            .IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).IsNone());
    EXPECT_EQ(infos, ConvertToArray({CreateCertInfo(cCertIssuer, "serial-b", "new-cert-url")}));

    ASSERT_TRUE(mStorage.GetCertsInfo(cCertType, storedCerts).IsNone());
    EXPECT_EQ(storedCerts, infos);
}

TEST_F(CertModuleTest, UpdateCertsRejectsMultiCertPEM)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCerts(2));

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CertModuleTest, UpdateCertsRejectsPEMParseError)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, UpdateCertsExceedsMaxCertificates)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 2> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 1;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs)
        .WillOnce(ReturnPEMCert("issuer", "serial-1"))
        .WillOnce(ReturnPEMCert("issuer", "serial-2"));

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-1").IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-2").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, UpdateCertsAllowsTemporaryGrowthBeyondMax)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;
    StaticArray<CertInfo, cCertsPerModule>            storedCerts;

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-new"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("new-cert-url"));
    EXPECT_CALL(mHSM, RemoveCert(String("url-1"), _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHSM, RemoveCert(String("url-2"), _)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_TRUE(mStorage.AddCertInfo(cCertType, CreateCertInfo(cCertIssuer, "serial-1", "url-1", mCertInfo.mNotAfter))
                    .IsNone());
    ASSERT_TRUE(mStorage.AddCertInfo(cCertType, CreateCertInfo(cCertIssuer, "serial-2", "url-2", mCertInfo.mNotAfter))
                    .IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).IsNone());
    EXPECT_EQ(infos, ConvertToArray({CreateCertInfo(cCertIssuer, "serial-new", "new-cert-url")}));

    ASSERT_TRUE(mStorage.GetCertsInfo(cCertType, storedCerts).IsNone());
    EXPECT_EQ(storedCerts, infos);
}

TEST_F(CertModuleTest, UpdateCertsRollsBackOnAddFailure)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 2> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;
    StaticArray<CertInfo, cCertsPerModule>            storedCerts;

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs)
        .WillOnce(ReturnPEMCert("issuer", "serial-1"))
        .WillOnce(ReturnPEMCert("issuer", "serial-2"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("added-cert-url")).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mHSM, RemoveCert(String("added-cert-url"), _)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-1").IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-2").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eFailed));

    ASSERT_TRUE(mStorage.GetCertsInfo(cCertType, storedCerts).IsNone());
    EXPECT_TRUE(storedCerts.IsEmpty()) << "Storage should be empty after rollback";
}

TEST_F(CertModuleTest, InitFailsOnMaxCertsConfigValueExceedingLimit)
{
    CertModule certModule;

    mModuleConfig.mMaxCertificates = cCertsPerModule + 1;

    ASSERT_TRUE(
        certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, InitRootFailsWhenValidateRootCertificatesFails)
{
    CertModule certModule;

    mModuleConfig.mCertType       = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mSkipValidation = false;

    EXPECT_CALL(mHSM, ValidateRootCertificates).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(
        certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, UpdateCertsFailsOnStorageError)
{
    CertModule                                        certModule;
    FailingStorageStub                                storage;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    EXPECT_CALL(mX509Provider, PEMToX509Certs).Times(0);

    storage.FailGet();

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, UpdateCertsExceedsTemporaryModuleLimit)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2;

    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    for (size_t i = 0; i < cCertsPerModule; ++i) {
        const auto serial = "serial-" + std::to_string(i);
        const auto url    = "url-" + std::to_string(i);
        const auto info   = CreateCertInfo(cCertIssuer, serial.c_str(), url.c_str(), mCertInfo.mNotAfter);

        ASSERT_TRUE(mStorage.AddCertInfo(cCertType, info).IsNone());
    }

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-new"));
    EXPECT_CALL(mHSM, AddCert).Times(0);

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, UpdateCertsAddsDuplicateCertOnce)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 2> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;
    StaticArray<CertInfo, cCertsPerModule>            storedCerts;

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2;

    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-1").IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-2").IsNone());

    EXPECT_CALL(mX509Provider, PEMToX509Certs).Times(2).WillRepeatedly(ReturnPEMCert(cCertIssuer, "serial-dup"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("dup-cert-url"));

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).IsNone());

    ASSERT_TRUE(mStorage.GetCertsInfo(cCertType, storedCerts).IsNone());
    EXPECT_EQ(storedCerts, ConvertToArray({CreateCertInfo(cCertIssuer, "serial-dup", "dup-cert-url")}));
}

TEST_F(CertModuleTest, UpdateCertsRemovesHSMCertOnStorageAddFailure)
{
    CertModule                                        certModule;
    FailingStorageStub                                storage;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-new"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("new-cert-url"));
    EXPECT_CALL(mHSM, RemoveCert(String("new-cert-url"), _)).WillOnce(Return(ErrorEnum::eNone));

    storage.FailAdd();

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, UpdateCertsFailsWhenHSMRemoveObsoleteFails)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    ASSERT_TRUE(
        mStorage.AddCertInfo(cCertType, CreateCertInfo(cCertIssuer, "serial-a", "old-cert-url", mCertInfo.mNotAfter))
            .IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-b"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("new-cert-url"));
    EXPECT_CALL(mHSM, RemoveCert(String("old-cert-url"), _)).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, UpdateCertsFailsWhenStorageRemoveObsoleteFails)
{
    CertModule                                        certModule;
    FailingStorageStub                                storage;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

    ASSERT_TRUE(
        storage.AddCertInfo(cCertType, CreateCertInfo(cCertIssuer, "serial-a", "old-cert-url", mCertInfo.mNotAfter))
            .IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-b"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("new-cert-url"));
    EXPECT_CALL(mHSM, RemoveCert(String("old-cert-url"), _)).WillOnce(Return(ErrorEnum::eNone));

    storage.FailRemove();

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, InitFailsNoMemoryAllocatingValidCerts)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation = false;
    mAllocator.FailAfter(0);
    EXPECT_CALL(mHSM, ValidateCertificates).Times(0);
    EXPECT_CALL(mHSM, ValidateRootCertificates).Times(0);

    ASSERT_TRUE(
        certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, GetCertificateFailsNoMemory)
{
    CertModule certModule;
    CertInfo   resCert;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    mAllocator.FailAfter(0);

    ASSERT_TRUE(certModule.GetCertificate(Array<uint8_t>(), Array<uint8_t>(), resCert).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, GetCertificateFailsNoMemoryAllocatingClearCertInfo)
{
    CertModule certModule;
    CertInfo   resCert;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    ASSERT_TRUE(mStorage.AddCertInfo(cCertType, mCertInfo).IsNone());

    mAllocator.FailAfter(1);

    ASSERT_TRUE(certModule.GetCertificate(Array<uint8_t>(), Array<uint8_t>(), resCert).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, ApplyCertFailsNoMemory)
{
    CertModule certModule;
    String     pemCert;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    mAllocator.FailAfter(0);

    ASSERT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, CreateCSRFailsNoMemory)
{
    CertModule                       certModule;
    PrivateKeyStub                   key;
    StaticString<crypto::cCSRPEMLen> pemCSR;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    mAllocator.FailAfter(0);

    ASSERT_TRUE(certModule.CreateCSR("subject", key, pemCSR).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, UpdateCertsFailsNoMemory)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    mAllocator.FailAfter(0);

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, CreateSelfSignedCertFailsNoMemory)
{
    CertModule certModule;
    auto       key = MakeShared<PrivateKeyStub>(&mAllocator);

    mModuleConfig.mCertType        = CertModuleTypeEnum::eSelfSigned;
    mModuleConfig.mMaxCertificates = 1;
    mModuleConfig.mSkipValidation  = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cCertType, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    ASSERT_TRUE(key);
    EXPECT_CALL(mHSM, CreateKey)
        .WillOnce(Return(RetWithError<SharedPtr<crypto::PrivateKeyItf>> {key, ErrorEnum::eNone}));

    mAllocator.FailAfter(0);

    ASSERT_TRUE(certModule.CreateSelfSignedCert("password").Is(ErrorEnum::eNoMemory));
}
