/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/common/crypto/mbedtls/cryptoprovider.hpp"
#include "aos/iam/certhandler.hpp"
#include "aos/iam/certmodules/pkcs11/pkcs11.hpp"
#include "log.hpp"
#include "mbedtls/pk.h"
#include "softhsmenv.hpp"
#include "stubs/storagestub.hpp"

namespace aos {
namespace iam {
namespace certhandler {

using namespace testing;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class IAMTest : public Test {
protected:
    void SetUp() override
    {
        InitLogs();

        mCertHandler = MakeShared<CertHandler>(&mAllocator);
        ASSERT_TRUE(mCryptoProvider.Init().IsNone());
        ASSERT_TRUE(mSOFTHSMEnv.Init("", "certhanler-integr-tests").IsNone());
    }

    // Default parameters
    static constexpr auto cPIN = "admin";

    // Helper functions

    void RegisterPKCS11Module(const String& name, crypto::KeyType keyType = crypto::KeyTypeEnum::eRSA)
    {
        ASSERT_TRUE(mPKCS11Modules.EmplaceBack().IsNone());
        ASSERT_TRUE(mCertModules.EmplaceBack().IsNone());

        auto& pkcs11Module = mPKCS11Modules.Back().mValue;
        auto& certModule   = mCertModules.Back().mValue;

        ASSERT_TRUE(
            pkcs11Module.Init(name, GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), mCryptoProvider).IsNone());
        ASSERT_TRUE(
            certModule.Init(name, GetCertModuleConfig(keyType), mCryptoProvider, pkcs11Module, mStorage).IsNone());

        ASSERT_TRUE(mCertHandler->RegisterModule(certModule).IsNone());
    }

    ModuleConfig GetCertModuleConfig(crypto::KeyType keyType)
    {
        ModuleConfig config;

        config.mKeyType         = keyType;
        config.mMaxCertificates = 2;
        config.mExtendedKeyUsage.EmplaceBack(ExtendedKeyUsageEnum::eClientAuth);
        config.mAlternativeNames.EmplaceBack("epam.com");
        config.mAlternativeNames.EmplaceBack("www.epam.com");
        config.mSkipValidation = false;

        return config;
    }

    PKCS11ModuleConfig GetPKCS11ModuleConfig()
    {
        PKCS11ModuleConfig config;

        config.mLibrary         = SOFTHSM2_LIB;
        config.mSlotID          = mSOFTHSMEnv.GetSlotID();
        config.mUserPINPath     = CERTIFICATES_DIR "/pin.txt";
        config.mModulePathInURL = true;

        return config;
    }

    // Service providers
    crypto::MbedTLSCryptoProvider mCryptoProvider;
    test::SoftHSMEnv              mSOFTHSMEnv;
    StorageStub                   mStorage;

    // Modules
    static constexpr auto                       cMaxModulesCount = 3;
    StaticArray<PKCS11Module, cMaxModulesCount> mPKCS11Modules;
    StaticArray<CertModule, cMaxModulesCount>   mCertModules;

    // Certificate handler
    StaticAllocator<sizeof(CertHandler) * 2 + pkcs11::cPrivateKeyMaxSize + pkcs11::Utils::cLocalObjectsMaxSize>
                           mAllocator;
    SharedPtr<CertHandler> mCertHandler;
};

/***********************************************************************************************************************
 * Statics
 **********************************************************************************************************************/

template <typename T, typename U>
void CheckArray(const Array<T>& actual, const std::initializer_list<U>& expected)
{
    EXPECT_THAT(std::vector<T>(actual.begin(), actual.end()), ElementsAreArray(expected));
}

void ParseDN(const mbedtls_x509_name& dn, String& result)
{
    result.Resize(result.MaxSize());

    int ret = mbedtls_x509_dn_gets(result.Get(), result.Size(), &dn);
    ASSERT_TRUE(ret > 0);
    result.Resize(ret);
}

void ParsePrivateKey(const String& pemCAKey, mbedtls_pk_context& privKey)
{
    mbedtls_ctr_drbg_context ctrDrbg;
    mbedtls_entropy_context  entropy;

    mbedtls_ctr_drbg_init(&ctrDrbg);
    mbedtls_entropy_init(&entropy);

    const char* pers = "test";

    int ret = mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy, (const uint8_t*)pers, strlen(pers));
    ASSERT_EQ(ret, 0);

    ret = mbedtls_pk_parse_key(&privKey, reinterpret_cast<const uint8_t*>(pemCAKey.Get()), pemCAKey.Size() + 1, nullptr,
        0, mbedtls_ctr_drbg_random, &ctrDrbg);
    ASSERT_EQ(ret, 0);

    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctrDrbg);
}

RetWithError<StaticString<100>> ConvertToString(const Time& time)
{
    StaticString<100> result;

    result.Resize(result.MaxSize());

    int  day = 0, month = 0, year = 0, hour = 0, min = 0, sec = 0;
    auto err = time.GetDate(&day, &month, &year);
    if (!err.IsNone()) {
        return {"", err};
    }

    err = time.GetTime(&hour, &min, &sec);
    if (!err.IsNone()) {
        return {"", err};
    }

    snprintf(result.Get(), result.Size(), "%04d%02d%02d%02d%02d%02d", year, month, day, hour, min, sec);

    result.Resize(strlen(result.CStr()));

    return {result, ErrorEnum::eNone};
}

// Implementation is based on https://github.com/Mbed-TLS/mbedtls/blob/development/programs/x509/cert_write.c
void CreateClientCert(const mbedtls_x509_csr& csr, const mbedtls_pk_context& caKey, const mbedtls_x509_crt& caCert,
    const Array<uint8_t>& serial, String& pemClientCert)
{
    mbedtls_x509write_cert clientCert;

    mbedtls_x509write_crt_init(&clientCert);

    mbedtls_x509write_crt_set_md_alg(&clientCert, mbedtls_md_get_type(mbedtls_md_info_from_string("SHA256")));

    // set CSR properties
    StaticString<crypto::cCertSubjSize> subject;

    ParseDN(csr.subject, subject);

    int ret = mbedtls_x509write_crt_set_subject_name(&clientCert, subject.Get());
    ASSERT_EQ(ret, 0);

    mbedtls_x509write_crt_set_subject_key(&clientCert, const_cast<mbedtls_pk_context*>(&csr.pk));

    // set CA certificate properties
    StaticString<crypto::cCertIssuerSize> issuer;

    ParseDN(caCert.subject, issuer);

    ret = mbedtls_x509write_crt_set_issuer_name(&clientCert, issuer.Get());
    ASSERT_EQ(ret, 0);

    // set CA key
    mbedtls_x509write_crt_set_issuer_key(&clientCert, const_cast<mbedtls_pk_context*>(&caKey));

    // set additional properties
    ret = mbedtls_x509write_crt_set_serial_raw(&clientCert, const_cast<uint8_t*>(serial.Get()), serial.Size());
    ASSERT_EQ(ret, 0);

    Error             err = ErrorEnum::eNone;
    StaticString<100> notBefore, notAfter;

    Tie(notBefore, err) = ConvertToString(Time::Now());
    ASSERT_TRUE(err.IsNone());

    Tie(notAfter, err) = ConvertToString(Time::Now().Add(Years(1)));
    ASSERT_TRUE(err.IsNone());

    ret = mbedtls_x509write_crt_set_validity(&clientCert, notBefore.CStr(), notAfter.CStr());
    ASSERT_EQ(ret, 0);

    // write client certificate to the buffer
    pemClientCert.Resize(pemClientCert.MaxSize());

    ret = mbedtls_x509write_crt_pem(&clientCert, reinterpret_cast<uint8_t*>(pemClientCert.Get()),
        pemClientCert.Size() + 1, mbedtls_ctr_drbg_random, nullptr);
    ASSERT_EQ(ret, 0);

    pemClientCert.Resize(strlen(pemClientCert.Get()));

    // free
    mbedtls_x509write_crt_free(&clientCert);
}

void CreateClientCert(const String& pemCSR, const String& pemCAKey, const String& pemCACert,
    const Array<uint8_t>& serial, String& pemClientCert)
{
    // parse CSR
    mbedtls_x509_csr csr;

    mbedtls_x509_csr_init(&csr);
    auto ret = mbedtls_x509_csr_parse(&csr, reinterpret_cast<const uint8_t*>(pemCSR.Get()), pemCSR.Size() + 1);
    ASSERT_EQ(ret, 0);

    // parse CA key
    mbedtls_pk_context caKey;

    mbedtls_pk_init(&caKey);
    ParsePrivateKey(pemCAKey, caKey);

    // parse CA cert
    mbedtls_x509_crt caCrt;

    mbedtls_x509_crt_init(&caCrt);
    ret = mbedtls_x509_crt_parse(&caCrt, reinterpret_cast<const uint8_t*>(pemCACert.CStr()), pemCACert.Size() + 1);
    ASSERT_EQ(ret, 0);

    // create client certificate
    CreateClientCert(csr, caKey, caCrt, serial, pemClientCert);

    // free
    mbedtls_x509_crt_free(&caCrt);
    mbedtls_pk_free(&caKey);
    mbedtls_x509_csr_free(&csr);
}

void CheckCSRValid(const String& pemCSR)
{
    mbedtls_x509_csr csr;
    mbedtls_x509_csr_init(&csr);

    auto ret = mbedtls_x509_csr_parse(&csr, reinterpret_cast<const uint8_t*>(pemCSR.Get()), pemCSR.Size() + 1);
    ASSERT_EQ(ret, 0);

    // Unfortunately mbedtls_x509_csr_parse doesn't check the signature.
    // and it looks like mbedtls doesn't provide API to check CSR signature at all.
    // Just print CSR info as a final step of verification.
    StaticString<512> csrInfo;

    csrInfo.Resize(csrInfo.MaxSize());

    ret = mbedtls_x509_csr_info(csrInfo.Get(), csrInfo.Size(), "   ", &csr);
    ASSERT_TRUE(ret > 0);
    LOG_INF() << "CSR info: \n" << csrInfo;

    mbedtls_x509_csr_free(&csr);
}

Error FindCertificates(test::SoftHSMEnv& pkcs11Env, Array<pkcs11::ObjectHandle>& objects)
{
    Error                             err = ErrorEnum::eNone;
    SharedPtr<pkcs11::SessionContext> session;

    Tie(session, err) = pkcs11Env.OpenUserSession("", false);
    if (!err.IsNone()) {
        return err;
    }

    CK_OBJECT_CLASS                         certClass = CKO_CERTIFICATE;
    StaticArray<pkcs11::ObjectAttribute, 1> attr;

    attr.EmplaceBack(CKA_CLASS, Array<uint8_t>(reinterpret_cast<uint8_t*>(&certClass), sizeof(certClass)));

    return session->FindObjects(attr, objects);
}

Error FindAllObjects(test::SoftHSMEnv& pkcs11Env, Array<pkcs11::ObjectHandle>& objects)
{
    Error                             err = ErrorEnum::eNone;
    SharedPtr<pkcs11::SessionContext> session;

    Tie(session, err) = pkcs11Env.OpenUserSession("", false);
    if (!err.IsNone()) {
        return err;
    }

    auto empty = Array<pkcs11::ObjectAttribute>(nullptr, 0);

    return session->FindObjects(empty, objects);
}

RetWithError<StaticString<pkcs11::cPINLen>> ReadPIN(const String& file)
{
    StaticString<pkcs11::cPINLen> pin;

    auto err = FS::ReadFileToString(file, pin);

    return {pin, err};
}

void ApplyCertificate(CertHandler& handler, const String& certType, const String& pin)
{
    StaticString<crypto::cCSRPEMLen> csr;
    ASSERT_TRUE(handler.CreateKey(certType, "Aos Core", pin, csr).IsNone());

    // create certificate from CSR, CA priv key, CA cert
    StaticString<crypto::cPrivKeyPEMLen> caKey;
    ASSERT_TRUE(FS::ReadFileToString(CERTIFICATES_DIR "/ca.key", caKey).IsNone());

    StaticString<crypto::cCertPEMLen> caCert;
    ASSERT_TRUE(FS::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCert).IsNone());

    uint64_t serialNum = 0x333333;
    auto     serial    = Array<uint8_t>(reinterpret_cast<uint8_t*>(&serialNum), sizeof(serialNum));
    StaticString<crypto::cCertPEMLen> clientCertChain;

    CreateClientCert(csr, caKey, caCert, serial, clientCertChain);

    // add CA cert to the chain
    clientCertChain.Append(caCert);

    // apply client certificate
    CertInfo certInfo;

    // FS::WriteStringToFile(CERTIFICATES_DIR "/client-out.pem", clientCertChain, 0666);
    ASSERT_TRUE(handler.ApplyCertificate(certType, clientCertChain, certInfo).IsNone());
    EXPECT_EQ(certInfo.mSerial, serial);
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(IAMTest, GetCertTypes)
{
    RegisterPKCS11Module("iam");
    RegisterPKCS11Module("sm");

    StaticArray<StaticString<cCertTypeLen>, cMaxModulesCount> certTypes;

    ASSERT_TRUE(mCertHandler->GetCertTypes(certTypes).IsNone());

    CheckArray(certTypes, {"iam", "sm"});
}

TEST_F(IAMTest, SetOwner)
{
    RegisterPKCS11Module("iam");

    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());
}

TEST_F(IAMTest, CreateKey)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    StaticString<crypto::cCSRPEMLen> csr;
    ASSERT_TRUE(mCertHandler->CreateKey("iam", "Aos Core", cPIN, csr).IsNone());

    CheckCSRValid(csr);
}

TEST_F(IAMTest, ApplyCertificate)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    StaticString<crypto::cCSRPEMLen> csr;
    ASSERT_TRUE(mCertHandler->CreateKey("iam", "Aos Core", cPIN, csr).IsNone());

    // create certificate from CSR, CA priv key, CA cert
    StaticString<crypto::cPrivKeyPEMLen> caKey;
    ASSERT_TRUE(FS::ReadFileToString(CERTIFICATES_DIR "/ca.key", caKey).IsNone());

    StaticString<crypto::cCertPEMLen> caCert;
    ASSERT_TRUE(FS::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCert).IsNone());

    uint64_t serialNum = 0x333333;
    auto     serial    = Array<uint8_t>(reinterpret_cast<uint8_t*>(&serialNum), sizeof(serialNum));
    StaticString<crypto::cCertPEMLen> clientCertChain;

    CreateClientCert(csr, caKey, caCert, serial, clientCertChain);

    // add CA cert to the chain
    clientCertChain.Append(caCert);

    // apply client certificate
    CertInfo certInfo;

    // FS::WriteStringToFile(CERTIFICATES_DIR "/client-out.pem", clientCertChain, 0666);
    ASSERT_TRUE(mCertHandler->ApplyCertificate("iam", clientCertChain, certInfo).IsNone());
    EXPECT_EQ(certInfo.mSerial, serial);

    // check storage
    StaticArray<CertInfo, 1> certificates;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", certificates).IsNone());
    ASSERT_EQ(certificates.Size(), 1);
    ASSERT_EQ(certificates[0], certInfo);
}

TEST_F(IAMTest, CreateSelfSignedCert)
{
    RegisterPKCS11Module("iam");

    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    StaticArray<CertInfo, 1> certificates;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", certificates).IsNone());
    ASSERT_EQ(certificates.Size(), 1);
}

TEST_F(IAMTest, GetCertificate)
{
    RegisterPKCS11Module("iam");

    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    StaticArray<CertInfo, 1> storageCerts;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", storageCerts).IsNone());
    ASSERT_EQ(storageCerts.Size(), 1);

    CertInfo certInfo;

    ASSERT_TRUE(
        mCertHandler->GetCertificate("iam", storageCerts[0].mIssuer, storageCerts[0].mSerial, certInfo).IsNone());
    ASSERT_EQ(certInfo, storageCerts[0]);
}

TEST_F(IAMTest, GetCertificateEmptySerial)
{
    RegisterPKCS11Module("iam");

    // create 2 certificates
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    sleep(1); // sleep 1 sec to update validity time

    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    // check storage is updated
    StaticArray<CertInfo, 2> storageCerts;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", storageCerts).IsNone());
    ASSERT_EQ(storageCerts.Size(), 2);

    // check GetCertificate returns certificate with minimal mNotAfter
    CertInfo certInfo;

    const auto empty = Array<uint8_t>(nullptr, 0);

    ASSERT_TRUE(mCertHandler->GetCertificate("iam", empty, empty, certInfo).IsNone());
    ASSERT_EQ(certInfo, storageCerts[0]);
}

TEST_F(IAMTest, Clear)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    // create 2 certificates
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    // ensure storage contains certificates
    StaticArray<CertInfo, 2> storageCerts;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", storageCerts).IsNone());
    ASSERT_EQ(storageCerts.Size(), 2);

    // ensure PKCS11 storage contains two certificates
    StaticArray<pkcs11::ObjectHandle, 3> handles;

    ASSERT_TRUE(FindCertificates(mSOFTHSMEnv, handles).IsNone());
    EXPECT_EQ(handles.Size(), 2);

    // call Clear
    ASSERT_TRUE(mCertHandler->Clear("iam").IsNone());

    // check there is no CertInfo in the storage
    ASSERT_TRUE(mStorage.GetCertsInfo("iam", storageCerts).Is(ErrorEnum::eNotFound));

    // check there is no certificates in PKCS11 storage
    ASSERT_TRUE(FindCertificates(mSOFTHSMEnv, handles).Is(ErrorEnum::eNotFound));
}

TEST_F(IAMTest, TrimCertificates)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    // create maximum number of certificates
    auto       maxCertificates = GetCertModuleConfig(crypto::KeyTypeEnum::eRSA).mMaxCertificates;
    const auto empty           = Array<uint8_t>(nullptr, 0);
    CertInfo   oldCertificate;

    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());
    ASSERT_TRUE(mCertHandler->GetCertificate("iam", empty, empty, oldCertificate).IsNone());

    sleep(1);

    for (size_t i = 1; i < maxCertificates; i++) {
        ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());
    }

    // create +1
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    // ensure storage contains exactly allowed number of certificates
    StaticArray<CertInfo, cCertsPerModule> storageCerts;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", storageCerts).IsNone());
    ASSERT_EQ(storageCerts.Size(), maxCertificates);
    // and old certificate is removed
    EXPECT_THAT(std::vector<CertInfo>(storageCerts.begin(), storageCerts.end()), Not(Contains(oldCertificate)));

    // ensure PKCS11 storage contains exactly allowed number of certificates
    StaticArray<pkcs11::ObjectHandle, cCertsPerModule> handles;

    ASSERT_TRUE(FindCertificates(mSOFTHSMEnv, handles).IsNone());
    EXPECT_EQ(handles.Size(), maxCertificates);
}

TEST_F(IAMTest, ValidateCertificates)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    // create 2 certificates
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    StaticArray<CertInfo, cCertsPerModule> storageCerts;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", storageCerts).IsNone());
    ASSERT_EQ(storageCerts.Size(), 2);

    // Close CertHandler
    mCertModules.Clear();
    mPKCS11Modules.Clear();
    mCertHandler.Reset();

    // remove one CertInfo from storage
    CertInfo validCert = storageCerts[0];

    CertInfo removedCert = storageCerts[1];
    ASSERT_TRUE(mStorage.RemoveCertInfo("iam", removedCert.mCertURL).IsNone());

    // add bad CertInfo to storage
    CertInfo badCert = removedCert;
    badCert.mCertURL = "broken URL";

    ASSERT_TRUE(mStorage.AddCertInfo("iam", badCert).IsNone());

    // Create CertHandler
    mCertHandler = MakeShared<CertHandler>(&mAllocator);
    RegisterPKCS11Module("iam");

    // Check Storage is restored.
    ASSERT_TRUE(mStorage.GetCertsInfo("iam", storageCerts).IsNone());
    ASSERT_EQ(storageCerts.Size(), 2);

    ASSERT_EQ(storageCerts[0], validCert);
    ASSERT_EQ(storageCerts[1], removedCert);
}

TEST_F(IAMTest, RemoveInvalidPKCS11Objects)
{
    // init certhandler & certhandler will init PKCS11 storage
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    // open session
    Error                             err = ErrorEnum::eNone;
    SharedPtr<pkcs11::SessionContext> session;
    StaticString<pkcs11::cPINLen>     userPIN;

    Tie(userPIN, err) = ReadPIN(GetPKCS11ModuleConfig().mUserPINPath);
    ASSERT_TRUE(err.IsNone());

    Tie(session, err) = mSOFTHSMEnv.OpenUserSession(userPIN, true);
    ASSERT_TRUE(err.IsNone());

    // import invalid cert
    uuid::UUID                                certId;
    StaticString<crypto::cCertPEMLen>         pemCert;
    StaticArray<crypto::x509::Certificate, 1> caCert;

    Tie(certId, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    ASSERT_TRUE(FS::ReadFileToString(CERTIFICATES_DIR "/ca.pem", pemCert).IsNone());

    ASSERT_TRUE(mCryptoProvider.PEMToX509Certs(pemCert, caCert).IsNone());

    err = pkcs11::Utils(session, mCryptoProvider, mAllocator).ImportCertificate(certId, "iam", caCert[0]);
    ASSERT_TRUE(err.IsNone());

    // generate invalid key pair
    pkcs11::PrivateKey privKey;
    uuid::UUID         keyId;

    Tie(keyId, err) = uuid::StringToUUID("08080808-0404-0404-0404-000000000000");
    ASSERT_TRUE(err.IsNone());

    Tie(privKey, err)
        = pkcs11::Utils(session, mCryptoProvider, mAllocator).GenerateRSAKeyPairWithLabel(keyId, "iam", 2048);
    ASSERT_TRUE(err.IsNone());

    // find invalid object handles
    StaticArray<pkcs11::ObjectHandle, 10> handles;

    ASSERT_TRUE(FindAllObjects(mSOFTHSMEnv, handles).IsNone());
    EXPECT_EQ(handles.Size(), 3);

    std::vector<pkcs11::ObjectHandle> badObjects {handles.begin(), handles.end()};

    // close current certificate module
    mCertModules.Clear();
    mPKCS11Modules.Clear();
    mCertHandler.Reset();

    // reinit certhandler to sync certificates/keys with PKCS11 storage
    mCertHandler = MakeShared<CertHandler>(&mAllocator);
    RegisterPKCS11Module("iam");

    // create key, because certmodule updates PKCS11 storage after that only
    StaticString<crypto::cCSRPEMLen> csr;

    ASSERT_TRUE(mCertHandler->CreateKey("iam", "Aos Core", cPIN, csr).IsNone());

    // check invalid certificate is removed
    ASSERT_TRUE(FindAllObjects(mSOFTHSMEnv, handles).IsNone());

    EXPECT_THAT(
        std::vector<pkcs11::ObjectHandle>(handles.begin(), handles.end()), Not(Contains(AnyOfArray(badObjects))));
}

TEST_F(IAMTest, RenewCertificate)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    ApplyCertificate(*mCertHandler, "iam", cPIN);

    RegisterPKCS11Module("sm");
    ApplyCertificate(*mCertHandler, "sm", cPIN);

    mCertModules.Clear();
    mPKCS11Modules.Clear();
    mCertHandler.Reset();

    // check certificate number
    StaticArray<pkcs11::ObjectHandle, pkcs11::cKeysPerToken> handles;

    ASSERT_TRUE(FindCertificates(mSOFTHSMEnv, handles).IsNone());
    ASSERT_EQ(handles.Size(), 3); // 1 root certificate + 2 generated

    // reinit certhandler to sync certificates/keys with PKCS11 storage
    mCertHandler = MakeShared<CertHandler>(&mAllocator);
    RegisterPKCS11Module("iam");
    RegisterPKCS11Module("sm");

    // create key, because certmodule updates PKCS11 storage afterwards only
    StaticString<crypto::cCSRPEMLen> csr;

    ASSERT_TRUE(mCertHandler->CreateKey("iam", "Aos Core", cPIN, csr).IsNone());

    // check certificate number is not changed
    ASSERT_TRUE(FindCertificates(mSOFTHSMEnv, handles).IsNone());
    ASSERT_EQ(handles.Size(), 3); // 1 root certificate + 2 generated
}

} // namespace certhandler
} // namespace iam
} // namespace aos
