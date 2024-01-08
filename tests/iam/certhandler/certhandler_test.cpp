/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "aos/iam/certhandler.hpp"

#include "mocks/hsm.hpp"
#include "mocks/privatekey.hpp"
#include "mocks/storage.hpp"
#include "mocks/x509provider.hpp"

using namespace aos;
using namespace aos::iam::certhandler;
using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CertHandlerTest : public Test {
protected:
    static const ModuleConfig cDefaultConfig;

    static const String cPKCS11Type;
    static const String cSWType;
    static const String cTPMType;

    static const String         cPassword;
    static const Array<uint8_t> cIssuer;
    static const String         cSubject;

    void SetUp() override
    {
        cPrivateKey = MakeShared<crypto::PrivateKeyMock>(&mAllocator, crypto::RSAPublicKey({}, {}));

        EXPECT_CALL(mX509Provider, ASN1DecodeDN(_, _))
            .WillRepeatedly(Invoke([](const Array<uint8_t>& dn, String& result) {
                result = reinterpret_cast<const char*>(dn.begin());
                return ErrorEnum::eNone;
            }));
    }

    void ApplyCert(const Array<crypto::x509::Certificate>& certChain, const CertInfo& devCertInfo)
    {
        EXPECT_CALL(mX509Provider, PEMToX509Certs(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(certChain), Return(ErrorEnum::eNone)));
        EXPECT_CALL(mPKCS11, ApplyCert(_, _, _))
            .WillOnce(DoAll(SetArgReferee<1>(devCertInfo), SetArgReferee<2>(cPassword), Return(ErrorEnum::eNone)));
        EXPECT_CALL(mStorage, AddCertInfo(cPKCS11Type, devCertInfo)).WillOnce(Return(ErrorEnum::eNone));

        StaticArray<uint8_t, 1> pemCert;
        CertInfo                certInfo;

        EXPECT_EQ(ErrorEnum::eNone, mCertHandler.ApplyCertificate(cPKCS11Type, pemCert, certInfo));
    }

    // certmodule class dependendcies
    StaticAllocator<sizeof(crypto::PrivateKeyMock)> mAllocator;

    MockHSM                    mPKCS11, mSW, mTPM;
    MockStorage                mStorage;
    crypto::x509::MockProvider mX509Provider;

    SharedPtr<crypto::PrivateKeyItf> cPrivateKey;

    CertHandler mCertHandler;
};

const Array<uint8_t> StringToByteArray(const char* str)
{
    return Array<uint8_t>(reinterpret_cast<const uint8_t*>(str), strlen(str) + 1);
}

const Array<uint8_t> StringToDN(const char* str)
{
    return Array<uint8_t>(reinterpret_cast<const uint8_t*>(str), strlen(str) + 1);
}

const String ASN1DecodeDN(const Array<uint8_t>& array)
{
    return reinterpret_cast<const char*>(array.Get());
}

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

const ModuleConfig CertHandlerTest::cDefaultConfig = {crypto::KeyTypeEnum::eRSA, 1U, {}, {}, false};

const String CertHandlerTest::cPKCS11Type = "pkcs11";
const String CertHandlerTest::cSWType     = "sw";
const String CertHandlerTest::cTPMType    = "tpm";

const String CertHandlerTest::cPassword = "1234";

const Array<uint8_t> CertHandlerTest::cIssuer
    = StringToDN("C = UA, OU = My Digest Company, CN = Developer Relations Cert");
const String CertHandlerTest::cSubject = "Aos core";

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CertHandlerTest, GetCertTypes)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};
    CertModule swModule     = {cSWType, cDefaultConfig};
    CertModule tpmModule    = {cTPMType, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    EXPECT_CALL(mSW, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, swModule.Init(mX509Provider, mSW, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(swModule));

    EXPECT_CALL(mTPM, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, tpmModule.Init(mX509Provider, mTPM, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(tpmModule));

    const std::vector<StaticString<cCertTypeLen>> expected = {cPKCS11Type, cSWType, cTPMType};
    StaticArray<StaticString<cCertTypeLen>, 3>    current;

    mCertHandler.GetCertTypes(current);

    EXPECT_EQ(current, Array<StaticString<cCertTypeLen>>(expected.data(), expected.size()));
}

TEST_F(CertHandlerTest, SetOwner)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillOnce(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    EXPECT_CALL(mPKCS11, SetOwner(cPassword)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.SetOwner(cPKCS11Type, cPassword));
}

TEST_F(CertHandlerTest, SetOwnerModuleNotFound)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillOnce(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    EXPECT_CALL(mPKCS11, SetOwner(_)).Times(0);
    ASSERT_EQ(ErrorEnum::eNotFound, mCertHandler.SetOwner(cSWType, cPassword));
}

TEST_F(CertHandlerTest, Clear)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillOnce(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    EXPECT_CALL(mStorage, RemoveAllCertsInfo(cPKCS11Type)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mPKCS11, Clear()).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.Clear(cPKCS11Type));
}

TEST_F(CertHandlerTest, ClearModuleNotFound)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillOnce(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    EXPECT_CALL(mStorage, RemoveAllCertsInfo(_)).Times(0);
    EXPECT_CALL(mPKCS11, Clear()).Times(0);
    ASSERT_EQ(ErrorEnum::eNotFound, mCertHandler.Clear(cSWType));
}

TEST_F(CertHandlerTest, CreateKey)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillOnce(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    RetWithError<SharedPtr<crypto::PrivateKeyItf>> privateKeyRes = {cPrivateKey, ErrorEnum::eNone};

    EXPECT_CALL(mPKCS11, CreateKey(cPassword, cDefaultConfig.mKeyType)).WillOnce(Return(privateKeyRes));
    EXPECT_CALL(mX509Provider, CreateCSR(_, _, _)).WillOnce(Return(ErrorEnum::eNone));

    StaticArray<uint8_t, 1> csr;

    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.CreateKey(cPKCS11Type, cSubject, cPassword, csr));
}

TEST_F(CertHandlerTest, CreateKeyModuleNotFound)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillOnce(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    StaticArray<uint8_t, 1> csr;

    ASSERT_EQ(ErrorEnum::eNotFound, mCertHandler.CreateKey(cSWType, cSubject, cPassword, csr));
}

TEST_F(CertHandlerTest, CreateKeyKeyGenError)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillOnce(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    RetWithError<SharedPtr<crypto::PrivateKeyItf>> privateKeyRes = {cPrivateKey, ErrorEnum::eFailed};

    EXPECT_CALL(mPKCS11, CreateKey(cPassword, cDefaultConfig.mKeyType)).WillOnce(Return(privateKeyRes));
    EXPECT_CALL(mX509Provider, CreateCSR(_, _, _)).Times(0);

    StaticArray<uint8_t, 1> csr;

    ASSERT_EQ(ErrorEnum::eFailed, mCertHandler.CreateKey(cPKCS11Type, cSubject, cPassword, csr));
}

TEST_F(CertHandlerTest, CreateCSR)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mX509Provider, ASN1EncodeDN(_, _))
        .WillRepeatedly(Invoke([](const String& commonName, Array<uint8_t>& result) {
            result = ::StringToDN(commonName.CStr());
            return ErrorEnum::eNone;
        }));

    ExtendedKeyUsage keyUsages[]               = {ExtendedKeyUsageEnum::eClientAuth, ExtendedKeyUsageEnum::eServerAuth};
    StaticString<crypto::cDNSNameLen> altDNS[] = {"epam1.com", "epam2.com"};

    ModuleConfig moduleConfig;

    moduleConfig.mKeyType          = crypto::KeyTypeEnum::eRSA;
    moduleConfig.mMaxCertificates  = 1;
    moduleConfig.mExtendedKeyUsage = Array<ExtendedKeyUsage>(keyUsages, 2);
    moduleConfig.mAlternativeNames = Array<StaticString<crypto::cDNSNameLen>>(altDNS, 2);
    moduleConfig.mSkipValidation   = false;

    CertModule pkcs11Module = {cPKCS11Type, moduleConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    RetWithError<SharedPtr<crypto::PrivateKeyItf>> privateKeyRes = {cPrivateKey, ErrorEnum::eNone};

    EXPECT_CALL(mPKCS11, CreateKey(cPassword, cDefaultConfig.mKeyType)).WillOnce(Return(privateKeyRes));

    using crypto::asn1::Extension;
    using crypto::x509::CSR;

    Extension                 extension  = {"2.5.29.37", {}};
    StaticArray<Extension, 1> extensions = Array<Extension>(&extension, 1);

    auto templ = AllOf(Field(&CSR::mSubject, cSubject), Field(&CSR::mDNSNames, moduleConfig.mAlternativeNames),
        Field(&CSR::mExtraExtensions, extensions));
    EXPECT_CALL(mX509Provider, CreateCSR(templ, _, _)).WillOnce(Return(ErrorEnum::eNone));

    StaticArray<uint8_t, 1> csr;
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.CreateKey(cPKCS11Type, cSubject, cPassword, csr));
}

TEST_F(CertHandlerTest, ApplyCert)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    crypto::x509::Certificate root;

    root.mSubject = root.mIssuer = StringToDN("ca.epam.com");
    root.mAuthorityKeyId = root.mSubjectKeyId = StringToDN("1.1.1.1");
    root.mSerial                              = StringToByteArray("1.1.1.1");
    root.mNotBefore                           = Time::Now();
    root.mNotAfter                            = root.mNotBefore.Add(Years(100));

    crypto::x509::Certificate dev;

    dev.mSubject        = StringToDN("device certificate");
    dev.mIssuer         = StringToDN("ca.epam.com");
    dev.mAuthorityKeyId = StringToDN("1.1.1.1");
    dev.mSubjectKeyId   = StringToDN("1.1.1.2");
    dev.mSerial         = StringToByteArray("1.1.1.2");
    dev.mNotBefore      = Time::Now();
    dev.mNotAfter       = dev.mNotBefore.Add(Years(100));

    StaticArray<crypto::x509::Certificate, 2> certChain;

    certChain.PushBack(dev);
    certChain.PushBack(root);

    CertInfo devCertInfo;

    devCertInfo.mCertURL  = "file://local-storage/certs/dev.pem";
    devCertInfo.mKeyURL   = "file://local-storage/keys/dev.priv";
    devCertInfo.mNotAfter = Time::Now().Add(Years(100));
    devCertInfo.mIssuer   = StringToDN("ca.epam.com");
    devCertInfo.mSerial   = StringToByteArray("1.1.1.2");

    ApplyCert(certChain, devCertInfo);
}

TEST_F(CertHandlerTest, CreateSelfSignedCert)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillRepeatedly(Return(ErrorEnum::eNone));

    EXPECT_CALL(mX509Provider, ASN1EncodeDN(_, _))
        .WillRepeatedly(Invoke([](const String& commonName, Array<uint8_t>& result) {
            result = ::StringToDN(commonName.CStr());
            return ErrorEnum::eNone;
        }));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    crypto::x509::Certificate cert;

    cert.mSubject = cert.mIssuer = StringToDN("Aos Core");
    cert.mAuthorityKeyId = cert.mSubjectKeyId = StringToDN("1.1.1.1");
    cert.mSerial                              = StringToByteArray("1.1.1.1");
    cert.mNotBefore                           = Time::Now();
    cert.mNotAfter                            = cert.mNotBefore.Add(Years(100));

    CertInfo devCertInfo;

    devCertInfo.mCertURL  = "file://local-storage/certs/dev.pem";
    devCertInfo.mKeyURL   = "file://local-storage/keys/dev.priv";
    devCertInfo.mNotAfter = cert.mNotAfter;
    devCertInfo.mIssuer   = cert.mIssuer;
    devCertInfo.mSerial   = cert.mSerial;

    StaticArray<crypto::x509::Certificate, 2> certChain;

    certChain.PushBack(cert);

    RetWithError<SharedPtr<crypto::PrivateKeyItf>> privateKeyRes = {cPrivateKey, ErrorEnum::eNone};
    EXPECT_CALL(mPKCS11, CreateKey(cPassword, cDefaultConfig.mKeyType)).WillOnce(Return(privateKeyRes));

    auto selfSigned = Truly([](const crypto::x509::Certificate& cert) { return cert.mIssuer == cert.mSubject; });

    EXPECT_CALL(mX509Provider, CreateCertificate(selfSigned, _, _, _)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mX509Provider, PEMToX509Certs(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(certChain), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mPKCS11, ApplyCert(_, _, _))
        .WillOnce(DoAll(SetArgReferee<1>(devCertInfo), SetArgReferee<2>(cPassword), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mStorage, AddCertInfo(cPKCS11Type, devCertInfo)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_EQ(ErrorEnum::eNone, mCertHandler.CreateSelfSignedCert(cPKCS11Type, cPassword));
}

TEST_F(CertHandlerTest, GetCertificate)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillOnce(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _)).WillOnce(Return(ErrorEnum::eNone));
    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    const auto serial = StringToDN("1.1.1.1");
    EXPECT_CALL(mStorage, GetCertInfo(cIssuer, serial, _)).WillOnce(Return(ErrorEnum::eNone));

    CertInfo cert;
    EXPECT_EQ(ErrorEnum::eNone, mCertHandler.GetCertificate(cPKCS11Type, cIssuer, serial, cert));
}

TEST_F(CertHandlerTest, GetCertificateEmptySerial)
{
    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    StaticArray<uint8_t, 1> serial;

    CertInfo cert1;
    CertInfo cert2;
    CertInfo cert3;

    cert1.mSerial   = StringToByteArray("1.1.1.1");
    cert1.mNotAfter = Time::Now().Add(Years(1));
    cert2.mSerial   = StringToByteArray("1.1.1.2");
    cert2.mNotAfter = Time::Now().Add(Years(2));
    cert3.mSerial   = StringToByteArray("1.1.1.3");
    cert3.mNotAfter = Time::Now().Add(Years(3));

    StaticArray<CertInfo, cCertsPerModule> certsInfo;

    certsInfo.PushBack(cert3);
    certsInfo.PushBack(cert2);
    certsInfo.PushBack(cert1);

    EXPECT_CALL(mStorage, GetCertsInfo(cPKCS11Type, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(certsInfo), Return(ErrorEnum::eNone)));

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(certsInfo), Return(ErrorEnum::eNone)));

    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    CertInfo   resCert;
    const auto err = mCertHandler.GetCertificate(cPKCS11Type, cIssuer, serial, resCert);
    EXPECT_EQ(err, ErrorEnum::eNone);
    EXPECT_EQ(resCert, cert1);
}

TEST_F(CertHandlerTest, RemoveInvalidCert)
{
    EXPECT_CALL(mStorage, GetCertsInfo(_, _)).WillOnce(Return(ErrorEnum::eNone));

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    StaticArray<StaticString<cURLLen>, 3> invalidCerts;
    StaticArray<StaticString<cURLLen>, 3> invalidKeys;

    invalidCerts.PushBack("file://local-storage/certs/old1.pem");
    invalidCerts.PushBack("file://local-storage/certs/old2.pem");
    invalidKeys.PushBack("file://local-storage/keys/old1.priv");
    invalidKeys.PushBack("file://local-storage/keys/old2.priv");

    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _))
        .WillOnce(DoAll(SetArgReferee<0>(invalidCerts), SetArgReferee<1>(invalidKeys), Return(ErrorEnum::eNone)));

    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    RetWithError<SharedPtr<crypto::PrivateKeyItf>> privateKeyRes = {cPrivateKey, ErrorEnum::eNone};

    EXPECT_CALL(mPKCS11, RemoveCert(invalidCerts[0], cPassword)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mPKCS11, RemoveCert(invalidCerts[1], cPassword)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mPKCS11, RemoveKey(invalidKeys[0], cPassword)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mPKCS11, RemoveKey(invalidKeys[1], cPassword)).WillOnce(Return(ErrorEnum::eNone));

    EXPECT_CALL(mPKCS11, CreateKey(cPassword, cDefaultConfig.mKeyType)).WillOnce(Return(privateKeyRes));
    EXPECT_CALL(mX509Provider, CreateCSR(_, _, _)).WillOnce(Return(ErrorEnum::eNone));

    StaticArray<uint8_t, 1> csr;

    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.CreateKey(cPKCS11Type, cSubject, cPassword, csr));
}

TEST_F(CertHandlerTest, SyncValidCerts)
{
    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    CertInfo cert1;
    CertInfo cert2;
    CertInfo cert3;
    CertInfo cert4;

    cert1.mCertURL = "file://local-storage/certs/cert1.pem";
    cert2.mCertURL = "file://local-storage/certs/cert2.pem";
    cert3.mCertURL = "file://local-storage/certs/cert3.pem";
    cert4.mCertURL = "file://local-storage/certs/cert4.pem";

    StaticArray<CertInfo, cCertsPerModule> validCerts;

    validCerts.PushBack(cert3);
    validCerts.PushBack(cert2);
    validCerts.PushBack(cert1);

    StaticArray<CertInfo, cCertsPerModule> certsInStorage;

    certsInStorage.PushBack(cert4);
    certsInStorage.PushBack(cert2);
    certsInStorage.PushBack(cert1);

    EXPECT_CALL(mStorage, GetCertsInfo(cPKCS11Type, _))
        .WillOnce(DoAll(SetArgReferee<1>(certsInStorage), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(validCerts), Return(ErrorEnum::eNone)));

    EXPECT_CALL(mStorage, RemoveCertInfo(cPKCS11Type, cert4.mCertURL)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mStorage, AddCertInfo(cPKCS11Type, cert3)).WillOnce(Return(ErrorEnum::eNone));

    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));
}

TEST_F(CertHandlerTest, TrimCerts)
{
    auto now = Time::Now();

    CertModule pkcs11Module = {cPKCS11Type, cDefaultConfig};

    CertInfo cert1;
    CertInfo cert2;

    cert1.mCertURL  = "file://local-storage/certs/cert1.pem";
    cert1.mNotAfter = now.Add(Years(3));
    cert2.mCertURL  = "file://local-storage/certs/cert2.pem";
    cert2.mKeyURL   = "file://local-storage/certs/key2.pem";
    cert2.mNotAfter = now.Add(Years(1));

    crypto::x509::Certificate root;

    root.mSubject = root.mIssuer = StringToDN("ca.epam.com");
    root.mAuthorityKeyId = root.mSubjectKeyId = StringToDN("1.1.1.1");
    root.mSerial                              = StringToByteArray("1.1.1.1");
    root.mNotBefore                           = now;
    root.mNotAfter                            = now.Add(Years(3));

    StaticArray<CertInfo, cCertsPerModule> initCerts;
    StaticArray<CertInfo, cCertsPerModule> appliedCerts;

    initCerts.PushBack(cert1);
    appliedCerts.PushBack(cert1);
    appliedCerts.PushBack(cert2);

    EXPECT_CALL(mStorage, GetCertsInfo(cPKCS11Type, _))
        .WillOnce(DoAll(SetArgReferee<1>(initCerts), Return(ErrorEnum::eNone)))
        .WillOnce(DoAll(SetArgReferee<1>(appliedCerts), Return(ErrorEnum::eNone)));
    EXPECT_CALL(mPKCS11, ValidateCertificates(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>(initCerts), Return(ErrorEnum::eNone)));

    ASSERT_EQ(ErrorEnum::eNone, pkcs11Module.Init(mX509Provider, mPKCS11, mStorage));
    ASSERT_EQ(ErrorEnum::eNone, mCertHandler.RegisterModule(pkcs11Module));

    StaticArray<crypto::x509::Certificate, 2> certChain;
    certChain.PushBack(root);

    // Trim
    EXPECT_CALL(mPKCS11, RemoveCert(cert2.mCertURL, cPassword)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mPKCS11, RemoveKey(cert2.mKeyURL, cPassword)).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(mStorage, RemoveCertInfo(cPKCS11Type, cert2.mCertURL)).WillOnce(Return(ErrorEnum::eNone));

    ApplyCert(certChain, cert2);
}
