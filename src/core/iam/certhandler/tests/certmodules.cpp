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
    void FailRemoveAll() { mFailRemoveAll = true; }
    void FailGetCert() { mFailGetCert = true; }
    void ReturnEmptyCerts() { mReturnEmpty = true; }

    Error AddCertInfo(const String& storageID, const CertInfo& certInfo) override
    {
        if (mFailAdd) {
            return ErrorEnum::eFailed;
        }

        return StorageStub::AddCertInfo(storageID, certInfo);
    }

    Error GetCertsInfo(const String& storageID, Array<CertInfo>& certsInfo) override
    {
        if (mFailGet) {
            return ErrorEnum::eFailed;
        }

        if (mReturnEmpty) {
            certsInfo.Clear();

            return ErrorEnum::eNone;
        }

        return StorageStub::GetCertsInfo(storageID, certsInfo);
    }

    Error RemoveCertInfo(const String& storageID, const String& certURL) override
    {
        if (mFailRemove) {
            return ErrorEnum::eFailed;
        }

        return StorageStub::RemoveCertInfo(storageID, certURL);
    }

    Error RemoveAllCertsInfo(const String& storageID) override
    {
        if (mFailRemoveAll) {
            return ErrorEnum::eFailed;
        }

        return StorageStub::RemoveAllCertsInfo(storageID);
    }

    Error GetCertInfo(const Array<uint8_t>& issuer, const Array<uint8_t>& serial, CertInfo& cert) override
    {
        if (mFailGetCert) {
            return ErrorEnum::eFailed;
        }

        return StorageStub::GetCertInfo(issuer, serial, cert);
    }

private:
    bool mFailAdd {};
    bool mFailGet {};
    bool mFailRemove {};
    bool mFailRemoveAll {};
    bool mFailGetCert {};
    bool mReturnEmpty {};
};

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CertModuleTest : public Test {
protected:
    // cppcheck-suppress unusedStructMember
    static constexpr auto cStorageID  = "test-cert-type";
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

    auto err = certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();
}

TEST_F(CertModuleTest, InitSelfSignedModuleSucceeds)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eSelfSigned;
    mModuleConfig.mMaxCertificates = 1;

    auto err = certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();
}

TEST_F(CertModuleTest, InitFailsOnOneMaxCertsConfigValueForNonSelfSignedModule)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eCertKeyPair;
    mModuleConfig.mMaxCertificates = 1;

    auto err = certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eInvalidArgument, err) << "Invalid argument expected: " << err.StrValue();
}

TEST_F(CertModuleTest, InitFailsOnZeroMaxCertsConfigValue)
{
    CertModule certModule;

    mModuleConfig.mMaxCertificates = 0;

    auto err = certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eInvalidArgument, err) << "Invalid argument expected: " << err.StrValue();
}

TEST_F(CertModuleTest, InitFailsNoMemory)
{
    CertModule certModule;

    mModuleConfig.mMaxCertificates = cCertsPerModule + 1;

    auto err = certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNoMemory, err) << "No memory expected: " << err.StrValue();
}

TEST_F(CertModuleTest, ApplyCert)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation = true;

    auto err = certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage);
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

    err = mStorage.GetCertsInfo(cStorageID, certInfoArray);
    ASSERT_TRUE(err.IsNone()) << "GetCertsInfo failed: " << err.StrValue();

    EXPECT_EQ(certInfoArray, ConvertToArray({mCertInfo})) << "CertInfo mismatch";
}

TEST_F(CertModuleTest, ApplyCertOldCertsAreTrimmed)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation = true;

    auto err = certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();

    err = mStorage.AddCertInfo(cStorageID, mCertInfo);
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

    auto err = certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage);
    ASSERT_EQ(ErrorEnum::eNone, err) << "Init failed: " << err.StrValue();

    for (size_t i = 0; i < cCertsPerModule; ++i) {
        mCertInfo.mNotAfter = Time::Now().Add(Time::cHours * i);

        err = mStorage.AddCertInfo(cStorageID, mCertInfo);
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

    err = mStorage.GetCertsInfo(cStorageID, certInfoArray);
    ASSERT_TRUE(err.IsNone()) << "GetCertsInfo failed: " << err.StrValue();

    ASSERT_EQ(cCertsPerModule, certInfoArray.Size());
}

TEST_F(CertModuleTest, RootModuleCreateKeyNotSupported)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mMaxCertificates = 2 * crypto::cMaxNumRootCerts;
    mModuleConfig.mSkipValidation  = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    EXPECT_CALL(mHSM, CreateKey).Times(0);

    EXPECT_EQ(certModule.CreateKey("password"),
        RetWithError<SharedPtr<crypto::PrivateKeyItf>>(nullptr, ErrorEnum::eNotSupported));
}

TEST_F(CertModuleTest, CreateKeyFailsWhenHSMFails)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
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
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mHSM, CreateKey)
        .WillOnce(Return(RetWithError<SharedPtr<crypto::PrivateKeyItf>> {nullptr, ErrorEnum::eFailed}));

    ASSERT_TRUE(certModule.CreateSelfSignedCert("password").Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, RootModuleApplyCertNotSupported)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mMaxCertificates = 2 * crypto::cMaxNumRootCerts;
    mModuleConfig.mSkipValidation  = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    EXPECT_CALL(mHSM, ApplyCert).Times(0);

    String pemCert;

    ASSERT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eNotSupported));
}

TEST_F(CertModuleTest, RootModuleCreateSelfSignedCertNotSupported)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mMaxCertificates = 2 * crypto::cMaxNumRootCerts;
    mModuleConfig.mSkipValidation  = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

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
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    ASSERT_TRUE(certModule.GetCertificates(infos).Is(ErrorEnum::eNotFound));

    const auto cert = CreateCertInfo(cCertIssuer, "serial-1", "cert-url-1", mCertInfo.mNotAfter);

    ASSERT_TRUE(mStorage.AddCertInfo(cStorageID, cert).IsNone());

    ASSERT_TRUE(certModule.GetCertificates(infos).IsNone());
    EXPECT_EQ(infos, ConvertToArray({cert}));
}

TEST_F(CertModuleTest, InitRootCallsValidateRootCertificates)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mMaxCertificates = 2 * crypto::cMaxNumRootCerts;
    mModuleConfig.mSkipValidation  = false;
    EXPECT_CALL(mHSM, ValidateRootCertificates).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHSM, ValidateKeysCertificates).Times(0);

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
}

TEST_F(CertModuleTest, UpdateCertsAddsNewCert)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;
    StaticArray<CertInfo, cCertsPerModule>            storedCerts;

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-new"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("new-cert-url"));

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).IsNone());
    EXPECT_EQ(infos, ConvertToArray({CreateCertInfo(cCertIssuer, "serial-new", "new-cert-url")}));

    ASSERT_TRUE(mStorage.GetCertsInfo(cStorageID, storedCerts).IsNone());
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
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-existing"));
    EXPECT_CALL(mHSM, AddCert).Times(0);

    ASSERT_TRUE(mStorage.AddCertInfo(cStorageID, existing).IsNone());
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
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-b"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("new-cert-url"));
    EXPECT_CALL(mHSM, RemoveCert(String("old-cert-url"), _)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_TRUE(
        mStorage.AddCertInfo(cStorageID, CreateCertInfo(cCertIssuer, "serial-a", "old-cert-url", mCertInfo.mNotAfter))
            .IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).IsNone());
    EXPECT_EQ(infos, ConvertToArray({CreateCertInfo(cCertIssuer, "serial-b", "new-cert-url")}));

    ASSERT_TRUE(mStorage.GetCertsInfo(cStorageID, storedCerts).IsNone());
    EXPECT_EQ(storedCerts, infos);
}

TEST_F(CertModuleTest, UpdateCertsRejectsMultiCertPEM)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
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
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, UpdateCertsExceedsMaxRootCerts)
{
    CertModule                                                                   certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, crypto::cMaxNumRootCerts + 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>                                       infos;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2 * crypto::cMaxNumRootCerts;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).Times(0);

    for (size_t i = 0; i < crypto::cMaxNumRootCerts + 1; ++i) {
        ASSERT_TRUE(pemCerts.EmplaceBack("pem").IsNone());
    }

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CertModuleTest, InitRootFailsWhenNoRoomForRotation)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2 * crypto::cMaxNumRootCerts - 1;

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage)
                    .Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CertModuleTest, UpdateCertsAllowsTemporaryGrowthWithinMax)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;
    StaticArray<CertInfo, cCertsPerModule>            storedCerts;

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 3;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, "serial-new"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("new-cert-url"));
    EXPECT_CALL(mHSM, RemoveCert(String("url-1"), _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHSM, RemoveCert(String("url-2"), _)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_TRUE(mStorage.AddCertInfo(cStorageID, CreateCertInfo(cCertIssuer, "serial-1", "url-1", mCertInfo.mNotAfter))
                    .IsNone());
    ASSERT_TRUE(mStorage.AddCertInfo(cStorageID, CreateCertInfo(cCertIssuer, "serial-2", "url-2", mCertInfo.mNotAfter))
                    .IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).IsNone());
    EXPECT_EQ(infos, ConvertToArray({CreateCertInfo(cCertIssuer, "serial-new", "new-cert-url")}));

    ASSERT_TRUE(mStorage.GetCertsInfo(cStorageID, storedCerts).IsNone());
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
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    EXPECT_CALL(mX509Provider, PEMToX509Certs)
        .WillOnce(ReturnPEMCert("issuer", "serial-1"))
        .WillOnce(ReturnPEMCert("issuer", "serial-2"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("added-cert-url")).WillOnce(Return(ErrorEnum::eFailed));
    EXPECT_CALL(mHSM, RemoveCert(String("added-cert-url"), _)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-1").IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-2").IsNone());

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eFailed));

    ASSERT_TRUE(mStorage.GetCertsInfo(cStorageID, storedCerts).IsNone());
    EXPECT_TRUE(storedCerts.IsEmpty()) << "Storage should be empty after rollback";
}

TEST_F(CertModuleTest, InitFailsOnMaxCertsConfigValueExceedingLimit)
{
    CertModule certModule;

    mModuleConfig.mMaxCertificates = cCertsPerModule + 1;

    ASSERT_TRUE(
        certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, InitRootFailsWhenValidateRootCertificatesFails)
{
    CertModule certModule;

    mModuleConfig.mCertType        = CertModuleTypeEnum::eRootCerts;
    mModuleConfig.mMaxCertificates = 2 * crypto::cMaxNumRootCerts;
    mModuleConfig.mSkipValidation  = false;

    EXPECT_CALL(mHSM, ValidateRootCertificates).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(
        certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, UpdateCertsFailsOnStorageError)
{
    CertModule                                        certModule;
    FailingStorageStub                                storage;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

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

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    for (size_t i = 0; i < cCertsPerModule; ++i) {
        const auto serial = "serial-" + std::to_string(i);
        const auto url    = "url-" + std::to_string(i);
        const auto info   = CreateCertInfo(cCertIssuer, serial.c_str(), url.c_str(), mCertInfo.mNotAfter);

        ASSERT_TRUE(mStorage.AddCertInfo(cStorageID, info).IsNone());
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

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-1").IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-2").IsNone());

    EXPECT_CALL(mX509Provider, PEMToX509Certs).Times(2).WillRepeatedly(ReturnPEMCert(cCertIssuer, "serial-dup"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("dup-cert-url"));

    ASSERT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).IsNone());

    ASSERT_TRUE(mStorage.GetCertsInfo(cStorageID, storedCerts).IsNone());
    EXPECT_EQ(storedCerts, ConvertToArray({CreateCertInfo(cCertIssuer, "serial-dup", "dup-cert-url")}));
}

TEST_F(CertModuleTest, UpdateCertsRemovesHSMCertOnStorageAddFailure)
{
    CertModule                                        certModule;
    FailingStorageStub                                storage;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

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

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    ASSERT_TRUE(
        mStorage.AddCertInfo(cStorageID, CreateCertInfo(cCertIssuer, "serial-a", "old-cert-url", mCertInfo.mNotAfter))
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

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

    ASSERT_TRUE(
        storage.AddCertInfo(cStorageID, CreateCertInfo(cCertIssuer, "serial-a", "old-cert-url", mCertInfo.mNotAfter))
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
    EXPECT_CALL(mHSM, ValidateKeysCertificates).Times(0);
    EXPECT_CALL(mHSM, ValidateRootCertificates).Times(0);

    ASSERT_TRUE(
        certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, GetCertificateFailsNoMemory)
{
    CertModule certModule;
    CertInfo   resCert;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    mAllocator.FailAfter(0);

    ASSERT_TRUE(certModule.GetCertificate(Array<uint8_t>(), Array<uint8_t>(), resCert).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, GetCertificateFailsNoMemoryAllocatingClearCertInfo)
{
    CertModule certModule;
    CertInfo   resCert;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    ASSERT_TRUE(mStorage.AddCertInfo(cStorageID, mCertInfo).IsNone());

    mAllocator.FailAfter(1);

    ASSERT_TRUE(certModule.GetCertificate(Array<uint8_t>(), Array<uint8_t>(), resCert).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, ApplyCertFailsNoMemory)
{
    CertModule certModule;
    String     pemCert;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    mAllocator.FailAfter(0);

    ASSERT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, CreateCSRFailsNoMemory)
{
    CertModule                       certModule;
    PrivateKeyStub                   key;
    StaticString<crypto::cCSRPEMLen> pemCSR;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    mAllocator.FailAfter(0);

    ASSERT_TRUE(certModule.CreateCSR("subject", key, pemCSR).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, UpdateCertsFailsNoMemory)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

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
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());
    ASSERT_TRUE(key);
    EXPECT_CALL(mHSM, CreateKey)
        .WillOnce(Return(RetWithError<SharedPtr<crypto::PrivateKeyItf>> {key, ErrorEnum::eNone}));

    mAllocator.FailAfter(0);

    ASSERT_TRUE(certModule.CreateSelfSignedCert("password").Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, InitFailsWhenValidateKeysCertificatesFails)
{
    CertModule certModule;

    mModuleConfig.mSkipValidation = false;

    EXPECT_CALL(mHSM, ValidateKeysCertificates).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(
        certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, InitSyncValidCertsErrors)
{
    const auto validCert  = CreateCertInfo(cCertIssuer, "serial-valid", "valid-url", mCertInfo.mNotAfter);
    const auto storedCert = CreateCertInfo(cCertIssuer, "serial-stored", "stored-url", mCertInfo.mNotAfter);

    mModuleConfig.mSkipValidation = false;

    {
        CertModule certModule;

        EXPECT_CALL(mHSM, ValidateKeysCertificates).WillOnce(Return(ErrorEnum::eNone));

        mAllocator.FailAfter(1);

        ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage)
                        .Is(ErrorEnum::eNoMemory));

        mAllocator.Reset();
    }

    {
        CertModule         certModule;
        FailingStorageStub storage;

        storage.FailGet();

        EXPECT_CALL(mHSM, ValidateKeysCertificates).WillOnce(Return(ErrorEnum::eNone));

        ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage)
                        .Is(ErrorEnum::eFailed));
    }

    {
        CertModule         certModule;
        FailingStorageStub storage;

        storage.FailAdd();

        EXPECT_CALL(mHSM, ValidateKeysCertificates)
            .WillOnce(Invoke([&validCert](Array<StaticString<cURLLen>>&, Array<StaticString<cURLLen>>&,
                                 Array<CertInfo>& validCerts) { return validCerts.PushBack(validCert); }));

        ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage)
                        .Is(ErrorEnum::eFailed));
    }

    {
        CertModule         certModule;
        FailingStorageStub storage;

        ASSERT_TRUE(storage.AddCertInfo(cStorageID, storedCert).IsNone());
        storage.FailRemove();

        EXPECT_CALL(mHSM, ValidateKeysCertificates).WillOnce(Return(ErrorEnum::eNone));

        ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage)
                        .Is(ErrorEnum::eFailed));
    }
}

TEST_F(CertModuleTest, GetCertificateErrors)
{
    CertModule         certModule;
    FailingStorageStub storage;
    CertInfo           resCert;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

    storage.ReturnEmptyCerts();

    ASSERT_TRUE(certModule.GetCertificate(Array<uint8_t>(), Array<uint8_t>(), resCert).Is(ErrorEnum::eNotFound));

    storage.FailGet();

    ASSERT_TRUE(certModule.GetCertificate(Array<uint8_t>(), Array<uint8_t>(), resCert).Is(ErrorEnum::eFailed));

    storage.FailGetCert();

    ASSERT_TRUE(certModule.GetCertificate(String(cCertIssuer).AsByteArray(), String("serial").AsByteArray(), resCert)
                    .Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, SetOwnerAndClearErrors)
{
    CertModule         certModule;
    FailingStorageStub storage;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

    EXPECT_CALL(mHSM, SetOwner).WillOnce(Return(ErrorEnum::eFailed));

    ASSERT_TRUE(certModule.SetOwner("password").Is(ErrorEnum::eFailed));

    EXPECT_CALL(mHSM, Clear).WillOnce(Return(ErrorEnum::eFailed)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_TRUE(certModule.Clear().Is(ErrorEnum::eFailed));

    storage.FailRemoveAll();

    ASSERT_TRUE(certModule.Clear().Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, CreateKeyFailsToRemoveInvalidObjects)
{
    const auto fillInvalid = [](bool certs, bool keys) {
        return Invoke([certs, keys](Array<StaticString<cURLLen>>& invalidCerts,
                          Array<StaticString<cURLLen>>&           invalidKeys, Array<CertInfo>&) {
            if (certs) {
                EXPECT_TRUE(invalidCerts.EmplaceBack("invalid-cert-url").IsNone());
            }

            if (keys) {
                EXPECT_TRUE(invalidKeys.EmplaceBack("invalid-key-url").IsNone());
            }

            return ErrorEnum::eNone;
        });
    };

    mModuleConfig.mSkipValidation = false;

    {
        CertModule certModule;

        EXPECT_CALL(mHSM, ValidateKeysCertificates).WillOnce(fillInvalid(true, false));
        ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

        EXPECT_CALL(mHSM, RemoveCert(String("invalid-cert-url"), _)).WillOnce(Return(ErrorEnum::eFailed));
        EXPECT_CALL(mHSM, CreateKey).Times(0);

        EXPECT_TRUE(certModule.CreateKey("password").mError.Is(ErrorEnum::eFailed));
    }

    {
        CertModule certModule;

        EXPECT_CALL(mHSM, ValidateKeysCertificates).WillOnce(fillInvalid(false, true));
        ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

        EXPECT_CALL(mHSM, RemoveKey(String("invalid-key-url"), _)).WillOnce(Return(ErrorEnum::eFailed));
        EXPECT_CALL(mHSM, CreateKey).Times(0);

        EXPECT_TRUE(certModule.CreateKey("password").mError.Is(ErrorEnum::eFailed));
    }
}

TEST_F(CertModuleTest, CreateCSRErrors)
{
    CertModule                       certModule;
    PrivateKeyStub                   key;
    StaticString<crypto::cCSRPEMLen> pemCSR;

    mModuleConfig.mSkipValidation = true;
    mModuleConfig.mExtendedKeyUsage.Clear();
    ASSERT_TRUE(mModuleConfig.mExtendedKeyUsage.EmplaceBack(ExtendedKeyUsageEnum::eServerAuth).IsNone());
    ASSERT_TRUE(mModuleConfig.mExtendedKeyUsage.EmplaceBack(static_cast<ExtendedKeyUsageEnum>(0x7F)).IsNone());

    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    const std::string longSubject(4096, 'a');

    EXPECT_CALL(mX509Provider, ASN1EncodeDN).Times(0);

    EXPECT_FALSE(certModule.CreateCSR(longSubject.c_str(), key, pemCSR).IsNone());

    EXPECT_CALL(mX509Provider, ASN1EncodeDN).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(certModule.CreateCSR("subject", key, pemCSR).Is(ErrorEnum::eFailed));

    EXPECT_CALL(mX509Provider, ASN1EncodeDN).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mX509Provider, ASN1EncodeObjectIds)
        .WillOnce(Invoke([](const Array<crypto::asn1::ObjectIdentifier>& oids, Array<uint8_t>&) {
            // only server auth is encoded, unknown usage is skipped
            EXPECT_EQ(oids.Size(), 1U);

            return ErrorEnum::eFailed;
        }))
        .WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_TRUE(certModule.CreateCSR("subject", key, pemCSR).Is(ErrorEnum::eFailed));

    EXPECT_CALL(mX509Provider, CreateCSR).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(certModule.CreateCSR("subject", key, pemCSR).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, ApplyCertErrors)
{
    CertModule         certModule;
    FailingStorageStub storage;
    String             pemCert;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

    EXPECT_CALL(mX509Provider, ASN1DecodeDN).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eFailed));

    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eNotFound));

    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillOnce(ReturnPEMCert(cCertIssuer, nullptr, "subject"));

    EXPECT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eNotFound));

    EXPECT_CALL(mX509Provider, PEMToX509Certs)
        .WillOnce(Invoke([](const String&, Array<crypto::x509::Certificate>& certs) {
            EXPECT_TRUE(certs.Resize(2).IsNone());

            certs[0].mIssuer  = String("parent").AsByteArray();
            certs[0].mSubject = String("child").AsByteArray();
            certs[1].mIssuer  = String("root").AsByteArray();
            certs[1].mSubject = String("parent").AsByteArray();

            return ErrorEnum::eNone;
        }));

    EXPECT_FALSE(certModule.ApplyCert(pemCert, mCertInfo).IsNone());

    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillRepeatedly(ReturnPEMCert(cCertIssuer, nullptr, cCertIssuer));

    storage.FailGet();

    EXPECT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eFailed));
}

TEST_F(CertModuleTest, ApplyCertHSMAndStorageErrors)
{
    CertModule         certModule;
    FailingStorageStub storage;
    String             pemCert;

    mModuleConfig.mSkipValidation = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

    EXPECT_CALL(mX509Provider, ASN1DecodeDN).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillRepeatedly(ReturnPEMCert(cCertIssuer, nullptr, cCertIssuer));

    EXPECT_CALL(mHSM, ApplyCert).WillOnce(Return(ErrorEnum::eFailed)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eFailed));

    storage.FailAdd();

    EXPECT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eFailed));

    mAllocator.FailAfter(1);

    EXPECT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, ApplyCertTrimErrors)
{
    String pemCert;

    mModuleConfig.mSkipValidation = true;

    EXPECT_CALL(mX509Provider, ASN1DecodeDN).WillRepeatedly(Return(ErrorEnum::eNone));
    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillRepeatedly(ReturnPEMCert(cCertIssuer, nullptr, cCertIssuer));
    EXPECT_CALL(mHSM, ApplyCert).Times(0);

    const auto fillStorage = [this](StorageItf& storage) {
        for (size_t i = 0; i < mModuleConfig.mMaxCertificates; ++i) {
            const auto serial = "serial-" + std::to_string(i);
            const auto url    = "url-" + std::to_string(i);

            ASSERT_TRUE(
                storage.AddCertInfo(cStorageID, CreateCertInfo(cCertIssuer, serial.c_str(), url.c_str())).IsNone());
        }
    };

    {
        CertModule  certModule;
        StorageStub storage;

        fillStorage(storage);
        ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

        EXPECT_CALL(mHSM, RemoveCert).WillOnce(Return(ErrorEnum::eFailed));

        EXPECT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eFailed));
    }

    {
        CertModule  certModule;
        StorageStub storage;

        fillStorage(storage);
        ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

        EXPECT_CALL(mHSM, RemoveCert).WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mHSM, RemoveKey).WillOnce(Return(ErrorEnum::eFailed));

        EXPECT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eFailed));
    }

    {
        CertModule         certModule;
        FailingStorageStub storage;

        fillStorage(storage);
        storage.FailRemove();
        ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, storage).IsNone());

        EXPECT_CALL(mHSM, RemoveCert).WillOnce(Return(ErrorEnum::eNone));
        EXPECT_CALL(mHSM, RemoveKey).WillOnce(Return(ErrorEnum::eNone));

        EXPECT_TRUE(certModule.ApplyCert(pemCert, mCertInfo).Is(ErrorEnum::eFailed));
    }
}

TEST_F(CertModuleTest, UpdateCertsFailsNoMemoryOnEachAllocation)
{
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;

    mModuleConfig.mSkipValidation = true;

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-cert").IsNone());

    EXPECT_CALL(mX509Provider, PEMToX509Certs).WillRepeatedly(ReturnPEMCert(cCertIssuer, "serial-new"));
    EXPECT_CALL(mHSM, AddCert).Times(0);

    // allocations: existing certs, parsed certs, PEM chain, added info (UpdateCerts), added info (AddCert)
    for (size_t n = 1; n <= 4; ++n) {
        CertModule                             certModule;
        StaticArray<CertInfo, cCertsPerModule> infos;

        ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

        mAllocator.FailAfter(n);

        EXPECT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eNoMemory)) << n;

        mAllocator.Reset();
    }
}

TEST_F(CertModuleTest, UpdateCertsFailsWhenResultIsFull)
{
    CertModule                                        certModule;
    StaticArray<StaticString<crypto::cCertPEMLen>, 2> pemCerts;
    StaticArray<CertInfo, 1>                          infos;

    mModuleConfig.mSkipValidation  = true;
    mModuleConfig.mMaxCertificates = 2;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    ASSERT_TRUE(pemCerts.EmplaceBack("pem-1").IsNone());
    ASSERT_TRUE(pemCerts.EmplaceBack("pem-2").IsNone());

    EXPECT_CALL(mX509Provider, PEMToX509Certs)
        .WillOnce(ReturnPEMCert(cCertIssuer, "serial-1"))
        .WillOnce(ReturnPEMCert(cCertIssuer, "serial-2"));
    EXPECT_CALL(mHSM, AddCert).WillOnce(ReturnAddCert("url-1")).WillOnce(ReturnAddCert("url-2"));
    EXPECT_CALL(mHSM, RemoveCert(String("url-2"), _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mHSM, RemoveCert(String("url-1"), _)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_TRUE(certModule.UpdateCerts(pemCerts, "password", infos).Is(ErrorEnum::eNoMemory));
}

TEST_F(CertModuleTest, CreateSelfSignedCertErrors)
{
    CertModule certModule;
    auto       key = MakeShared<PrivateKeyStub>(&mAllocator);

    ASSERT_TRUE(key);

    mModuleConfig.mCertType        = CertModuleTypeEnum::eSelfSigned;
    mModuleConfig.mMaxCertificates = 1;
    mModuleConfig.mSkipValidation  = true;
    ASSERT_TRUE(certModule.Init(mAllocator, cStorageID, mModuleConfig, mX509Provider, mHSM, mStorage).IsNone());

    EXPECT_CALL(mHSM, CreateKey)
        .WillRepeatedly(Return(RetWithError<SharedPtr<crypto::PrivateKeyItf>> {key, ErrorEnum::eNone}));

    EXPECT_CALL(mX509Provider, ASN1EncodeDN).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(certModule.CreateSelfSignedCert("password").Is(ErrorEnum::eFailed));

    EXPECT_CALL(mX509Provider, ASN1EncodeDN).WillOnce(Return(ErrorEnum::eNone)).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(certModule.CreateSelfSignedCert("password").Is(ErrorEnum::eFailed));

    EXPECT_CALL(mX509Provider, ASN1EncodeDN).WillRepeatedly(Return(ErrorEnum::eNone));

    for (size_t n = 1; n <= 2; ++n) {
        mAllocator.FailAfter(n);

        EXPECT_TRUE(certModule.CreateSelfSignedCert("password").Is(ErrorEnum::eNoMemory)) << n;

        mAllocator.Reset();
    }

    EXPECT_CALL(mX509Provider, CreateCertificate).WillOnce(Return(ErrorEnum::eFailed));

    EXPECT_TRUE(certModule.CreateSelfSignedCert("password").Is(ErrorEnum::eFailed));
}
