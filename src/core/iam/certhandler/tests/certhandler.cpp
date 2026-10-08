/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <core/common/tests/crypto/providers/cryptofactory.hpp>
#include <core/common/tests/crypto/softhsmenv.hpp>
#include <core/common/tests/mocks/certprovidermock.hpp>
#include <core/common/tests/stubs/testallocator.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/iam/certhandler/certhandler.hpp>
#include <core/iam/certhandler/certmodules/pkcs11/pkcs11.hpp>
#include <core/iam/tests/stubs/certhandlerstub.hpp>

namespace aos::iam::certhandler {

using namespace testing;
using aos::tests::TestAllocator;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CerthandlerTest : public Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        ASSERT_TRUE(mCryptoFactory.Init(mAllocator).IsNone());
        mCryptoProvider = &mCryptoFactory.GetCryptoProvider();

        mCertHandler = MakeShared<CertHandler>(&mAllocator, mAllocator);
        ASSERT_TRUE(mCertHandler);
        ASSERT_TRUE(mSOFTHSMEnv.Init(mAllocator, "", "certhanler-integr-tests").IsNone());
    }

    // Default parameters
    static constexpr auto cPIN = "admin";

    // Helper functions

    void RegisterPKCS11Module(const String& name, crypto::KeyType keyType = crypto::KeyTypeEnum::eRSA,
        CertModuleTypeEnum certModuleType = CertModuleTypeEnum::eCertKeyPair)
    {
        ASSERT_TRUE(mPKCS11Modules.EmplaceBack().IsNone());
        ASSERT_TRUE(mCertModules.EmplaceBack().IsNone());

        auto& pkcs11Module = mPKCS11Modules.Back();
        auto& certModule   = mCertModules.Back();

        ASSERT_TRUE(
            pkcs11Module.Init(mAllocator, name, GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
                .IsNone());
        ASSERT_TRUE(certModule
                        .Init(mAllocator, name, GetCertModuleConfig(keyType, certModuleType), *mCryptoProvider,
                            pkcs11Module, mStorage)
                        .IsNone());

        ASSERT_TRUE(mCertHandler->RegisterModule(certModule).IsNone());
    }

    ModuleConfig GetCertModuleConfig(
        crypto::KeyType keyType, CertModuleTypeEnum certModuleType = CertModuleTypeEnum::eCertKeyPair)
    {
        ModuleConfig config;

        config.mKeyType         = keyType;
        config.mMaxCertificates = 2;
        config.mExtendedKeyUsage.EmplaceBack(ExtendedKeyUsageEnum::eClientAuth);
        config.mAlternativeNames.EmplaceBack("epam.com");
        config.mAlternativeNames.EmplaceBack("www.epam.com");
        config.mSkipValidation = false;
        config.mCertType       = certModuleType;

        return config;
    }

    PKCS11ModuleConfig GetPKCS11ModuleConfig()
    {
        PKCS11ModuleConfig config;

        config.mLibrary         = SOFTHSM2_LIB;
        config.mSlotID          = mSOFTHSMEnv.GetSlotID();
        config.mUserPINPath     = CERTIFICATES_DIR "/pin.txt";
        config.mModulePathInURL = true;
        config.mUID             = 42;
        config.mGID             = 0;

        return config;
    }

    // mAllocator must be declared (and therefore destroyed) after any member that allocates from it, since
    // members are destroyed in reverse declaration order.
    HeapAllocator mAllocator;

    // Service providers
    crypto::DefaultCryptoFactory mCryptoFactory;
    crypto::CryptoProviderItf*   mCryptoProvider = nullptr;
    test::SoftHSMEnv             mSOFTHSMEnv;
    StorageStub                  mStorage;

    // Modules
    static constexpr auto                       cMaxModulesCount = 3;
    StaticArray<PKCS11Module, cMaxModulesCount> mPKCS11Modules;
    StaticArray<CertModule, cMaxModulesCount>   mCertModules;

    // Certificate handler
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

Error FindCertificates(test::SoftHSMEnv& pkcs11Env, Array<pkcs11::ObjectHandle>& objects)
{
    Error                             err;
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
    Error                             err;
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

    auto err = fs::ReadFileToString(file, pin);

    return {pin, err};
}

void ApplyCertificate(
    CertHandler& handler, crypto::x509::ProviderItf& cryptoProvider, const String& certType, const String& pin)
{
    StaticString<crypto::cCSRPEMLen> csr;
    ASSERT_TRUE(handler.CreateKey(certType, "Aos Core", pin, csr).IsNone());

    // create certificate from CSR, CA priv key, CA cert
    StaticString<crypto::cPrivKeyPEMLen> caKey;
    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.key", caKey).IsNone());

    StaticString<crypto::cCertPEMLen> caCert;
    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCert).IsNone());

    uint64_t serialNum = 0x333333;
    auto     serial    = Array<uint8_t>(reinterpret_cast<uint8_t*>(&serialNum), sizeof(serialNum));
    StaticString<crypto::cCertPEMLen> clientCertChain;

    ASSERT_TRUE(cryptoProvider.CreateClientCert(csr, caKey, caCert, serial, clientCertChain).IsNone());

    // add CA cert to the chain
    clientCertChain.Append(caCert);

    // apply client certificate
    CertInfo certInfo;

    // fs::WriteStringToFile(CERTIFICATES_DIR "/client-out.pem", clientCertChain, 0666);
    ASSERT_TRUE(handler.ApplyCertificate(certType, clientCertChain, certInfo).IsNone());
    EXPECT_EQ(certInfo.mSerial, serial);
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CerthandlerTest, GetCertTypes)
{
    RegisterPKCS11Module("iam");
    RegisterPKCS11Module("sm");

    StaticArray<StaticString<cCertTypeLen>, cMaxModulesCount> certTypes;

    ASSERT_TRUE(mCertHandler->GetCertTypes(certTypes).IsNone());

    CheckArray(certTypes, {"iam", "sm"});
}

TEST_F(CerthandlerTest, GetModuleConfig)
{
    RegisterPKCS11Module("iam");
    RegisterPKCS11Module("sm", crypto::KeyTypeEnum::eRSA, CertModuleTypeEnum::eSelfSigned);

    EXPECT_EQ(mCertHandler->GetModuleConfig("iam"),
        RetWithError<ModuleConfig>(GetCertModuleConfig(crypto::KeyTypeEnum::eRSA)));
    EXPECT_EQ(mCertHandler->GetModuleConfig("sm"),
        RetWithError<ModuleConfig>(GetCertModuleConfig(crypto::KeyTypeEnum::eRSA, CertModuleTypeEnum::eSelfSigned)));
}

TEST_F(CerthandlerTest, ModuleConfigEquality)
{
    const auto base = GetCertModuleConfig(crypto::KeyTypeEnum::eRSA);

    EXPECT_EQ(base, base);
    EXPECT_FALSE(base != base);

    auto other     = base;
    other.mKeyType = crypto::KeyTypeEnum::eECDSA;
    EXPECT_NE(base, other);

    other                  = base;
    other.mMaxCertificates = base.mMaxCertificates + 1;
    EXPECT_NE(base, other);

    other = base;
    other.mExtendedKeyUsage.Clear();
    other.mExtendedKeyUsage.EmplaceBack(ExtendedKeyUsageEnum::eServerAuth);
    EXPECT_NE(base, other);

    other = base;
    other.mAlternativeNames.Clear();
    other.mAlternativeNames.EmplaceBack("other.example");
    EXPECT_NE(base, other);

    other                 = base;
    other.mSkipValidation = !base.mSkipValidation;
    EXPECT_NE(base, other);

    other           = base;
    other.mCertType = CertModuleTypeEnum::eRootCerts;
    EXPECT_NE(base, other);
}

TEST_F(CerthandlerTest, SetOwner)
{
    RegisterPKCS11Module("iam");

    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());
}

TEST_F(CerthandlerTest, CreateKey)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    StaticString<crypto::cCSRPEMLen> csr;
    ASSERT_TRUE(mCertHandler->CreateKey("iam", "Aos Core", cPIN, csr).IsNone());

    ASSERT_TRUE(mCryptoFactory.VerifyCSR(csr.CStr()));
}

TEST_F(CerthandlerTest, CreateKeyECDSA)
{
    StaticString<crypto::cCSRPEMLen> csr;

    RegisterPKCS11Module("iam", crypto::KeyTypeEnum::eECDSA);
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    auto key = mPKCS11Modules[0].CreateKey(cPIN, crypto::KeyTypeEnum::eECDSA);
    EXPECT_EQ(key, RetWithError<SharedPtr<crypto::PrivateKeyItf>>(key.mValue, ErrorEnum::eNone));

    ASSERT_TRUE(mCertHandler->CreateKey("iam", "Aos Core", cPIN, csr).IsNone());
    ASSERT_TRUE(mCryptoFactory.VerifyCSR(csr.CStr()));
}

TEST_F(CerthandlerTest, CreateKeyUnsupportedAlgorithm)
{
    const crypto::KeyType unsupported {static_cast<crypto::KeyTypeEnum>(0x7F)};

    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    EXPECT_EQ(mPKCS11Modules[0].CreateKey(cPIN, unsupported),
        RetWithError<SharedPtr<crypto::PrivateKeyItf>>(nullptr, ErrorEnum::eNotSupported));
}

TEST_F(CerthandlerTest, CreateKeyEvictsOldestPendingKey)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    for (size_t i = 0; i < cCertsPerModule + 1; ++i) {
        auto key = mPKCS11Modules[0].CreateKey(cPIN, crypto::KeyTypeEnum::eRSA);
        EXPECT_EQ(key, RetWithError<SharedPtr<crypto::PrivateKeyItf>>(key.mValue, ErrorEnum::eNone)) << i;
    }
}

TEST_F(CerthandlerTest, PKCS11CreateKeyBranchesDirect)
{
    PKCS11Module          module;
    const crypto::KeyType unsupported {static_cast<crypto::KeyTypeEnum>(0x7F)};

    ASSERT_TRUE(
        module.Init(mAllocator, "pkcs11-direct", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
            .IsNone());
    ASSERT_TRUE(module.SetOwner(cPIN).IsNone());

    auto ecdsaKey = module.CreateKey(cPIN, crypto::KeyTypeEnum::eECDSA);
    EXPECT_EQ(ecdsaKey, RetWithError<SharedPtr<crypto::PrivateKeyItf>>(ecdsaKey.mValue, ErrorEnum::eNone));

    EXPECT_EQ(module.CreateKey(cPIN, unsupported),
        RetWithError<SharedPtr<crypto::PrivateKeyItf>>(nullptr, ErrorEnum::eNotSupported));

    for (size_t i = 0; i < cCertsPerModule + 1; ++i) {
        auto key = module.CreateKey(cPIN, crypto::KeyTypeEnum::eRSA);
        EXPECT_EQ(key, RetWithError<SharedPtr<crypto::PrivateKeyItf>>(key.mValue, ErrorEnum::eNone)) << i;
    }
}

TEST_F(CerthandlerTest, PKCS11InitByTokenLabel)
{
    PKCS11Module module;
    auto         config = GetPKCS11ModuleConfig();

    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    config.mSlotID.Reset();
    config.mTokenLabel = "aos";

    ASSERT_TRUE(module.Init(mAllocator, "by-label", config, mSOFTHSMEnv.GetManager(), *mCryptoProvider).IsNone());
}

TEST_F(CerthandlerTest, PKCS11InitFindsFreeSlot)
{
    PKCS11Module module;
    auto         config = GetPKCS11ModuleConfig();

    config.mSlotID.Reset();

    ASSERT_TRUE(module.Init(mAllocator, "free-slot", config, mSOFTHSMEnv.GetManager(), *mCryptoProvider).IsNone());
}

TEST_F(CerthandlerTest, PKCS11InitByTokenLabelFailsNoMemory)
{
    TestAllocator alloc;
    PKCS11Module  module;
    auto          config = GetPKCS11ModuleConfig();

    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    alloc.FailAfter(0);
    config.mSlotID.Reset();
    config.mTokenLabel = "aos";

    ASSERT_TRUE(module.Init(alloc, "by-label-oom", config, mSOFTHSMEnv.GetManager(), *mCryptoProvider)
                    .Is(ErrorEnum::eNoMemory));
}

TEST_F(CerthandlerTest, PKCS11InitRejectsMultipleSlotSelectors)
{
    PKCS11Module module;
    auto         config = GetPKCS11ModuleConfig();

    config.mTokenLabel = "aos";

    ASSERT_TRUE(module.Init(mAllocator, "bad-config", config, mSOFTHSMEnv.GetManager(), *mCryptoProvider)
                    .Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CerthandlerTest, PKCS11InitBySlotIndex)
{
    PKCS11Module module;
    auto         config = GetPKCS11ModuleConfig();

    config.mSlotID.Reset();
    config.mSlotIndex.SetValue(0);

    ASSERT_TRUE(module.Init(mAllocator, "by-index", config, mSOFTHSMEnv.GetManager(), *mCryptoProvider).IsNone());
}

TEST_F(CerthandlerTest, PKCS11InitRejectsInvalidSlotIndex)
{
    PKCS11Module module;
    auto         config = GetPKCS11ModuleConfig();

    config.mSlotID.Reset();
    config.mSlotIndex.SetValue(9999);

    ASSERT_TRUE(module.Init(mAllocator, "bad-index", config, mSOFTHSMEnv.GetManager(), *mCryptoProvider)
                    .Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CerthandlerTest, PKCS11ClearFailsWithoutMemory)
{
    TestAllocator alloc;
    PKCS11Module  module;
    bool          hitClearOOM = false;

    ASSERT_TRUE(
        module.Init(alloc, "oom-clear", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider).IsNone());
    ASSERT_TRUE(module.SetOwner(cPIN).IsNone());

    alloc.FailAfter(0);

    ASSERT_TRUE(module.Clear().Is(ErrorEnum::eNoMemory));

    for (size_t n = 1; n <= 4; ++n) {
        alloc.FailAfter(n);

        if (module.Clear().Is(ErrorEnum::eNoMemory)) {
            hitClearOOM = true;
            break;
        }
    }

    ASSERT_TRUE(hitClearOOM);
}

TEST_F(CerthandlerTest, PKCS11AddAndRemoveCert)
{
    PKCS11Module                              module;
    StaticString<crypto::cCertPEMLen>         caCertPem;
    StaticArray<crypto::x509::Certificate, 1> certs;
    CertInfo                                  info;

    ASSERT_TRUE(module.Init(mAllocator, "add-cert", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
                    .IsNone());
    ASSERT_TRUE(module.SetOwner(cPIN).IsNone());

    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCertPem).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(caCertPem, certs).IsNone());
    ASSERT_EQ(certs.Size(), 1);

    ASSERT_TRUE(module.AddCert(certs[0], "", info).IsNone());
    EXPECT_FALSE(info.mCertURL.IsEmpty());

    ASSERT_TRUE(module.RemoveCert(info.mCertURL, "").IsNone());
}

TEST_F(CerthandlerTest, PKCS11AddCertFailsWhenNotOwned)
{
    PKCS11Module                              module;
    StaticString<crypto::cCertPEMLen>         caCertPem;
    StaticArray<crypto::x509::Certificate, 1> certs;
    CertInfo                                  info;

    ASSERT_TRUE(
        module.Init(mAllocator, "add-unowned", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
            .IsNone());

    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCertPem).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(caCertPem, certs).IsNone());

    ASSERT_FALSE(module.AddCert(certs[0], "", info).IsNone());
}

TEST_F(CerthandlerTest, PKCS11AddCertFailsCreateURLOOM)
{
    TestAllocator                             alloc;
    PKCS11Module                              module;
    StaticString<crypto::cCertPEMLen>         caCertPem;
    StaticArray<crypto::x509::Certificate, 1> certs;
    bool                                      hitOOM = false;

    ASSERT_TRUE(
        module.Init(alloc, "add-oom", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider).IsNone());
    ASSERT_TRUE(module.SetOwner(cPIN).IsNone());

    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCertPem).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(caCertPem, certs).IsNone());

    for (size_t n = 0; n <= 8; ++n) {
        alloc.FailAfter(n);

        CertInfo info;
        if (module.AddCert(certs[0], "", info).Is(ErrorEnum::eNoMemory)) {
            hitOOM = true;
            break;
        }
    }

    ASSERT_TRUE(hitOOM);
}

TEST_F(CerthandlerTest, PKCS11ValidateRootWhenNotOwned)
{
    PKCS11Module                           module;
    StaticArray<CertInfo, cCertsPerModule> validCerts;

    ASSERT_TRUE(
        module.Init(mAllocator, "root-unowned", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
            .IsNone());

    ASSERT_TRUE(module.ValidateRootCertificates(validCerts).IsNone());
    EXPECT_TRUE(validCerts.IsEmpty());
}

TEST_F(CerthandlerTest, PKCS11ValidateRootFailsWrongPIN)
{
    PKCS11Module                           owner;
    PKCS11Module                           module;
    StaticArray<CertInfo, cCertsPerModule> validCerts;

    ASSERT_TRUE(owner.Init(mAllocator, "root-pin", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
                    .IsNone());
    ASSERT_TRUE(owner.SetOwner(cPIN).IsNone());

    ASSERT_TRUE(fs::WriteStringToFile(GetPKCS11ModuleConfig().mUserPINPath, "wrong-pin", 0600).IsNone());

    ASSERT_TRUE(module.Init(mAllocator, "root-pin", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
                    .IsNone());

    ASSERT_FALSE(module.ValidateRootCertificates(validCerts).IsNone());
}

TEST_F(CerthandlerTest, PKCS11ValidateRootFailsWithoutMemory)
{
    TestAllocator                             alloc;
    PKCS11Module                              module;
    StaticString<crypto::cCertPEMLen>         caCertPem;
    StaticArray<crypto::x509::Certificate, 1> certs;
    CertInfo                                  info;
    bool                                      hitOOM = false;

    ASSERT_TRUE(
        module.Init(alloc, "root-oom", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider).IsNone());
    ASSERT_TRUE(module.SetOwner(cPIN).IsNone());

    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCertPem).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(caCertPem, certs).IsNone());

    ASSERT_TRUE(module.AddCert(certs[0], "", info).IsNone());

    for (size_t n = 0; n <= 8; ++n) {
        alloc.FailAfter(n);

        StaticArray<CertInfo, cCertsPerModule> validCerts;
        if (module.ValidateRootCertificates(validCerts).Is(ErrorEnum::eNoMemory)) {
            hitOOM = true;
            break;
        }
    }

    ASSERT_TRUE(hitOOM);
}

TEST_F(CerthandlerTest, PKCS11ValidateRootFailsWhenResultFull)
{
    PKCS11Module                              module;
    StaticString<crypto::cCertPEMLen>         caCertPem;
    StaticArray<crypto::x509::Certificate, 1> certs;
    CertInfo                                  info;
    StaticArray<CertInfo, 1>                  validCerts;

    ASSERT_TRUE(
        module.Init(mAllocator, "root-full", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
            .IsNone());
    ASSERT_TRUE(module.SetOwner(cPIN).IsNone());

    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCertPem).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(caCertPem, certs).IsNone());

    ASSERT_TRUE(module.AddCert(certs[0], "", info).IsNone());
    ASSERT_TRUE(validCerts.EmplaceBack().IsNone());

    ASSERT_TRUE(module.ValidateRootCertificates(validCerts).Is(ErrorEnum::eNoMemory));
}

TEST_F(CerthandlerTest, PKCS11ApplyCertWithoutPendingKey)
{
    PKCS11Module                              module;
    StaticString<crypto::cCertPEMLen>         caCertPem;
    StaticArray<crypto::x509::Certificate, 1> certs;
    CertInfo                                  info;
    StaticString<pkcs11::cPINLen>             password;

    ASSERT_TRUE(
        module.Init(mAllocator, "apply-nokey", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
            .IsNone());
    ASSERT_TRUE(module.SetOwner(cPIN).IsNone());

    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCertPem).IsNone());
    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(caCertPem, certs).IsNone());

    ASSERT_TRUE(module.ApplyCert(certs, info, password).Is(ErrorEnum::eNotFound));
}

TEST_F(CerthandlerTest, PKCS11RemoveCertBadURL)
{
    PKCS11Module module;

    ASSERT_TRUE(
        module.Init(mAllocator, "rm-bad-url", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
            .IsNone());
    ASSERT_TRUE(module.SetOwner(cPIN).IsNone());

    ASSERT_FALSE(module.RemoveCert("not-a-pkcs11-url", "").IsNone());
    ASSERT_FALSE(module.RemoveKey("not-a-pkcs11-url", "").IsNone());
}

TEST_F(CerthandlerTest, PKCS11InitRejectsEmptyUserPINPath)
{
    PKCS11Module module;
    auto         config = GetPKCS11ModuleConfig();

    config.mUserPINPath.Clear();

    ASSERT_TRUE(module.Init(mAllocator, "no-pin-path", config, mSOFTHSMEnv.GetManager(), *mCryptoProvider)
                    .Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CerthandlerTest, PKCS11InitRejectsBadLibrary)
{
    PKCS11Module module;
    auto         config = GetPKCS11ModuleConfig();

    config.mLibrary = "/nonexistent/libpkcs11.so";

    ASSERT_TRUE(module.Init(mAllocator, "bad-lib", config, mSOFTHSMEnv.GetManager(), *mCryptoProvider)
                    .Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CerthandlerTest, PKCS11ClearWhenNotOwned)
{
    PKCS11Module module;

    ASSERT_TRUE(
        module.Init(mAllocator, "not-owned", GetPKCS11ModuleConfig(), mSOFTHSMEnv.GetManager(), *mCryptoProvider)
            .IsNone());

    ASSERT_TRUE(module.Clear().IsNone());
}

TEST_F(CerthandlerTest, ApplyCertificate)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    StaticString<crypto::cCSRPEMLen> csr;
    ASSERT_TRUE(mCertHandler->CreateKey("iam", "Aos Core", cPIN, csr).IsNone());

    // create certificate from CSR, CA priv key, CA cert
    StaticString<crypto::cPrivKeyPEMLen> caKey;
    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.key", caKey).IsNone());

    StaticString<crypto::cCertPEMLen> caCert;
    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCert).IsNone());

    uint64_t serialNum = 0x333333;
    auto     serial    = Array<uint8_t>(reinterpret_cast<uint8_t*>(&serialNum), sizeof(serialNum));
    StaticString<crypto::cCertPEMLen> clientCertChain;

    ASSERT_TRUE(mCryptoProvider->CreateClientCert(csr, caKey, caCert, serial, clientCertChain).IsNone());

    // add CA cert to the chain
    clientCertChain.Append(caCert);

    // apply client certificate
    CertInfo certInfo;

    // fs::WriteStringToFile(CERTIFICATES_DIR "/client-out.pem", clientCertChain, 0666);
    ASSERT_TRUE(mCertHandler->ApplyCertificate("iam", clientCertChain, certInfo).IsNone());
    EXPECT_EQ(certInfo.mSerial, serial);

    // check storage
    StaticArray<CertInfo, 1> certificates;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", certificates).IsNone());
    ASSERT_EQ(certificates.Size(), 1);
    ASSERT_EQ(certificates[0], certInfo);
}

TEST_F(CerthandlerTest, CreateSelfSignedCert)
{
    RegisterPKCS11Module("iam");

    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    StaticArray<CertInfo, 1> certificates;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", certificates).IsNone());
    ASSERT_EQ(certificates.Size(), 1);
}

TEST_F(CerthandlerTest, GetCertificate)
{
    RegisterPKCS11Module("iam");

    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    StaticArray<CertInfo, 1> storageCerts;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", storageCerts).IsNone());
    ASSERT_EQ(storageCerts.Size(), 1);

    CertInfo certInfo;

    ASSERT_TRUE(mCertHandler->GetCert("iam", storageCerts[0].mIssuer, storageCerts[0].mSerial, certInfo).IsNone());
    ASSERT_EQ(certInfo, storageCerts[0]);
}

TEST_F(CerthandlerTest, GetCertificateEmptySerial)
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

    // check GetCertificate returns certificate with max mNotAfter
    CertInfo certInfo;

    const auto empty = Array<uint8_t>(nullptr, 0);

    ASSERT_TRUE(mCertHandler->GetCert("iam", empty, empty, certInfo).IsNone());
    ASSERT_EQ(certInfo, storageCerts[1]);
}

TEST_F(CerthandlerTest, SubscribeCertChanged)
{
    RegisterPKCS11Module("iam");

    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    StaticArray<CertInfo, 2> storageCerts;

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", storageCerts).IsNone());
    ASSERT_EQ(storageCerts.Size(), 1);

    iamclient::CertListenerMock certListener;

    EXPECT_CALL(certListener, OnCertChanged(_));
    ASSERT_TRUE(mCertHandler->SubscribeListener("iam", certListener).IsNone());
    sleep(1);
    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());
    ASSERT_TRUE(mCertHandler->UnsubscribeListener(certListener).IsNone());
}

TEST_F(CerthandlerTest, Clear)
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

TEST_F(CerthandlerTest, TrimCertificates)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    // create maximum number of certificates
    auto                     maxCertificates = GetCertModuleConfig(crypto::KeyTypeEnum::eRSA).mMaxCertificates;
    StaticArray<CertInfo, 1> oldCertificates;

    ASSERT_TRUE(mCertHandler->CreateSelfSignedCert("iam", cPIN).IsNone());

    ASSERT_TRUE(mStorage.GetCertsInfo("iam", oldCertificates).IsNone());
    ASSERT_EQ(oldCertificates.Size(), 1);

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
    EXPECT_THAT(std::vector<CertInfo>(storageCerts.begin(), storageCerts.end()), Not(Contains(oldCertificates[0])));

    // ensure PKCS11 storage contains exactly allowed number of certificates
    StaticArray<pkcs11::ObjectHandle, cCertsPerModule> handles;

    ASSERT_TRUE(FindCertificates(mSOFTHSMEnv, handles).IsNone());
    EXPECT_EQ(handles.Size(), maxCertificates);
}

TEST_F(CerthandlerTest, ValidateCertificates)
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
    mCertHandler = MakeShared<CertHandler>(&mAllocator, mAllocator);
    ASSERT_TRUE(mCertHandler);
    RegisterPKCS11Module("iam");

    // Check Storage is restored.
    ASSERT_TRUE(mStorage.GetCertsInfo("iam", storageCerts).IsNone());
    ASSERT_EQ(storageCerts.Size(), 2);

    ASSERT_EQ(storageCerts[0], validCert);
    ASSERT_EQ(storageCerts[1], removedCert);
}

TEST_F(CerthandlerTest, RemoveInvalidPKCS11Objects)
{
    // init certhandler & certhandler will init PKCS11 storage
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    // open session
    Error                             err;
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

    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.pem", pemCert).IsNone());

    ASSERT_TRUE(mCryptoProvider->PEMToX509Certs(pemCert, caCert).IsNone());

    err = pkcs11::Utils(mAllocator, session, *mCryptoProvider).ImportCertificate(certId, "iam", caCert[0]);
    ASSERT_TRUE(err.IsNone());

    // generate invalid key pair
    pkcs11::PrivateKey privKey;
    uuid::UUID         keyId;

    Tie(keyId, err) = uuid::StringToUUID("08080808-0404-0404-0404-000000000000");
    ASSERT_TRUE(err.IsNone());

    Tie(privKey, err)
        = pkcs11::Utils(mAllocator, session, *mCryptoProvider).GenerateRSAKeyPairWithLabel(keyId, "iam", 2048);
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
    mCertHandler = MakeShared<CertHandler>(&mAllocator, mAllocator);
    ASSERT_TRUE(mCertHandler);
    RegisterPKCS11Module("iam");

    // create key, because certmodule updates PKCS11 storage after that only
    StaticString<crypto::cCSRPEMLen> csr;

    ASSERT_TRUE(mCertHandler->CreateKey("iam", "Aos Core", cPIN, csr).IsNone());

    // check invalid certificate is removed
    ASSERT_TRUE(FindAllObjects(mSOFTHSMEnv, handles).IsNone());

    EXPECT_THAT(
        std::vector<pkcs11::ObjectHandle>(handles.begin(), handles.end()), Not(Contains(AnyOfArray(badObjects))));
}

TEST_F(CerthandlerTest, RenewCertificate)
{
    RegisterPKCS11Module("iam");
    ASSERT_TRUE(mCertHandler->SetOwner("iam", cPIN).IsNone());

    ApplyCertificate(*mCertHandler, *mCryptoProvider, "iam", cPIN);

    RegisterPKCS11Module("sm");
    ApplyCertificate(*mCertHandler, *mCryptoProvider, "sm", cPIN);

    mCertModules.Clear();
    mPKCS11Modules.Clear();
    mCertHandler.Reset();

    // check certificate number
    StaticArray<pkcs11::ObjectHandle, pkcs11::cKeysPerToken> handles;

    ASSERT_TRUE(FindCertificates(mSOFTHSMEnv, handles).IsNone());
    ASSERT_EQ(handles.Size(), 3); // 1 root certificate + 2 generated

    // reinit certhandler to sync certificates/keys with PKCS11 storage
    mCertHandler = MakeShared<CertHandler>(&mAllocator, mAllocator);
    ASSERT_TRUE(mCertHandler);
    RegisterPKCS11Module("iam");
    RegisterPKCS11Module("sm");

    // create key, because certmodule updates PKCS11 storage afterwards only
    StaticString<crypto::cCSRPEMLen> csr;

    ASSERT_TRUE(mCertHandler->CreateKey("iam", "Aos Core", cPIN, csr).IsNone());

    // check certificate number is not changed
    ASSERT_TRUE(FindCertificates(mSOFTHSMEnv, handles).IsNone());
    ASSERT_EQ(handles.Size(), 3); // 1 root certificate + 2 generated
}

TEST_F(CerthandlerTest, UpdateRootCertsAndGetAllCerts)
{
    StaticString<crypto::cCertPEMLen>                 caCert;
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;
    StaticArray<CertInfo, cCertsPerModule>            allCerts;

    RegisterPKCS11Module("rootcerts", crypto::KeyTypeEnum::eRSA, CertModuleTypeEnum::eRootCerts);
    ASSERT_TRUE(mCertHandler->SetOwner("rootcerts", cPIN).IsNone());

    ASSERT_TRUE(fs::ReadFileToString(CERTIFICATES_DIR "/ca.pem", caCert).IsNone());
    ASSERT_TRUE(pemCerts.PushBack(caCert).IsNone());

    ASSERT_TRUE(mCertHandler->UpdateCerts("rootcerts", pemCerts, "", infos).IsNone());
    ASSERT_FALSE(infos.IsEmpty());

    ASSERT_TRUE(mCertHandler->GetAllCerts("rootcerts", allCerts).IsNone());
    EXPECT_EQ(allCerts, infos);

    mCertModules.Clear();
    mPKCS11Modules.Clear();
    mCertHandler.Reset();
    allCerts.Clear();

    mCertHandler = MakeShared<CertHandler>(&mAllocator, mAllocator);

    RegisterPKCS11Module("rootcerts", crypto::KeyTypeEnum::eRSA, CertModuleTypeEnum::eRootCerts);

    ASSERT_TRUE(mCertHandler->GetAllCerts("rootcerts", allCerts).IsNone());
    EXPECT_EQ(allCerts, infos);
}

TEST_F(CerthandlerTest, GetAllCertsUnknownType)
{
    StaticArray<CertInfo, cCertsPerModule> allCerts;

    ASSERT_TRUE(mCertHandler->GetAllCerts("unknown", allCerts).Is(ErrorEnum::eNotFound));
}

TEST_F(CerthandlerTest, UpdateCertsUnknownType)
{
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    ASSERT_TRUE(pemCerts.EmplaceBack("pem").IsNone());

    ASSERT_TRUE(mCertHandler->UpdateCerts("unknown", pemCerts, "", infos).Is(ErrorEnum::eNotFound));
}

TEST_F(CerthandlerTest, UpdateCertsRejectsEmptySet)
{
    StaticArray<StaticString<crypto::cCertPEMLen>, 1> pemCerts;
    StaticArray<CertInfo, cCertsPerModule>            infos;

    RegisterPKCS11Module("rootcerts", crypto::KeyTypeEnum::eRSA, CertModuleTypeEnum::eRootCerts);
    ASSERT_TRUE(mCertHandler->SetOwner("rootcerts", cPIN).IsNone());

    ASSERT_TRUE(mCertHandler->UpdateCerts("rootcerts", pemCerts, "", infos).Is(ErrorEnum::eInvalidArgument));
}

TEST_F(CerthandlerTest, GetRootCertType)
{
    StaticString<cCertTypeLen> certType;

    RegisterPKCS11Module("iam");
    RegisterPKCS11Module("rootcerts", crypto::KeyTypeEnum::eRSA, CertModuleTypeEnum::eRootCerts);

    ASSERT_TRUE(mCertHandler->GetRootCertType(certType).IsNone());
    EXPECT_EQ(certType, "rootcerts");
}

TEST_F(CerthandlerTest, GetRootCertTypeNotFound)
{
    StaticString<cCertTypeLen> certType;

    RegisterPKCS11Module("iam");

    ASSERT_TRUE(mCertHandler->GetRootCertType(certType).Is(ErrorEnum::eNotFound));
}

} // namespace aos::iam::certhandler
