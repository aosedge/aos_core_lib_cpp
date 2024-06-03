/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <aos/iam/provisionmanager.hpp>

#include "mocks/certhandlermock.hpp"
#include "mocks/provisioningcallbackmock.hpp"

using namespace testing;
using namespace aos::iam;
using namespace aos::iam::provisionmanager;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class ProvisionManagerTest : public testing::Test {
protected:
    void SetUp() override { mProvisionManager.Init(mCallback, mCertHandler); }

    ProvisionManagerCallbackMock mCallback;
    CertHandlerItfMock           mCertHandler;
    ProvisionManager             mProvisionManager;
};

/***********************************************************************************************************************
 * Tests
 * **********************************************************************************************************************/

TEST_F(ProvisionManagerTest, StartProvisioningSucceeds)
{
    CertTypes gneratedCertTypes;
    gneratedCertTypes.EmplaceBack("certType1");
    gneratedCertTypes.EmplaceBack("certType2");
    gneratedCertTypes.EmplaceBack("certType3");
    gneratedCertTypes.EmplaceBack("diskEncryption");

    EXPECT_CALL(mCallback, OnStartProvisioning).Times(1);
    EXPECT_CALL(mCertHandler, GetCertTypes)
        .Times(1)
        .WillOnce(DoAll(SetArgReferee<0>(gneratedCertTypes), Return(aos::ErrorEnum::eNone)));

    certhandler::ModuleConfig moduleConfig1;
    certhandler::ModuleConfig moduleConfig2;
    certhandler::ModuleConfig moduleConfig3;
    certhandler::ModuleConfig moduleConfig4;

    moduleConfig1.mIsSelfSigned = false;
    moduleConfig2.mIsSelfSigned = false;
    moduleConfig3.mIsSelfSigned = true;
    moduleConfig4.mIsSelfSigned = true;

    EXPECT_CALL(mCertHandler, Clear).Times(4);
    EXPECT_CALL(mCertHandler, SetOwner).Times(4);
    EXPECT_CALL(mCertHandler, GetModuleConfig)
        .Times(4)
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig1)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig2)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig3)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig4)));

    EXPECT_CALL(mCertHandler, CreateSelfSignedCert).Times(2);
    EXPECT_CALL(mCallback, OnEncryptDisk).Times(1);

    const auto err = mProvisionManager.StartProvisioning("password");
    EXPECT_TRUE(err.IsNone()) << err.Message();
}

TEST_F(ProvisionManagerTest, StartProvisioningFails)
{
    CertTypes gneratedCertTypes;
    gneratedCertTypes.EmplaceBack("certType1");
    gneratedCertTypes.EmplaceBack("certType2");
    gneratedCertTypes.EmplaceBack("certType3");
    gneratedCertTypes.EmplaceBack("diskEncryption");

    EXPECT_CALL(mCallback, OnStartProvisioning).Times(1);
    EXPECT_CALL(mCertHandler, GetCertTypes)
        .Times(1)
        .WillOnce(DoAll(SetArgReferee<0>(gneratedCertTypes), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mCertHandler, Clear).WillOnce(Return(aos::ErrorEnum::eFailed));

    auto err = mProvisionManager.StartProvisioning("password");

    EXPECT_TRUE(!err.IsNone()) << err.Message();

    EXPECT_CALL(mCallback, OnStartProvisioning).Times(1);
    EXPECT_CALL(mCertHandler, GetCertTypes)
        .Times(1)
        .WillOnce(DoAll(SetArgReferee<0>(gneratedCertTypes), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mCertHandler, Clear).Times(1);
    EXPECT_CALL(mCertHandler, SetOwner).WillOnce(Return(aos::ErrorEnum::eFailed));

    err = mProvisionManager.StartProvisioning("password");

    EXPECT_TRUE(!err.IsNone()) << err.Message();
}

TEST_F(ProvisionManagerTest, StartProvisioningDiscEncryptionFails)
{
    CertTypes gneratedCertTypes;
    gneratedCertTypes.EmplaceBack("certType1");
    gneratedCertTypes.EmplaceBack("certType2");
    gneratedCertTypes.EmplaceBack("certType3");
    gneratedCertTypes.EmplaceBack("diskEncryption");

    EXPECT_CALL(mCallback, OnStartProvisioning).Times(1);
    EXPECT_CALL(mCertHandler, GetCertTypes)
        .Times(1)
        .WillOnce(DoAll(SetArgReferee<0>(gneratedCertTypes), Return(aos::ErrorEnum::eNone)));

    EXPECT_CALL(mCertHandler, Clear).Times(4);
    EXPECT_CALL(mCertHandler, SetOwner).Times(4);

    certhandler::ModuleConfig moduleConfig1;
    certhandler::ModuleConfig moduleConfig2;
    certhandler::ModuleConfig moduleConfig3;
    certhandler::ModuleConfig moduleConfig4;

    moduleConfig1.mIsSelfSigned = false;
    moduleConfig2.mIsSelfSigned = false;
    moduleConfig3.mIsSelfSigned = true;
    moduleConfig4.mIsSelfSigned = true;

    EXPECT_CALL(mCertHandler, GetModuleConfig)
        .Times(4)
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig1)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig2)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig3)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig4)));
    EXPECT_CALL(mCertHandler, CreateSelfSignedCert)
        .WillOnce(Return(aos::ErrorEnum::eNone))
        .WillOnce(Return(aos::ErrorEnum::eFailed));

    auto err = mProvisionManager.StartProvisioning("password");

    EXPECT_TRUE(!err.IsNone()) << err.Message();

    EXPECT_CALL(mCallback, OnStartProvisioning).Times(1);
    EXPECT_CALL(mCertHandler, GetCertTypes)
        .Times(1)
        .WillOnce(DoAll(SetArgReferee<0>(gneratedCertTypes), Return(aos::ErrorEnum::eNone)));
    EXPECT_CALL(mCertHandler, Clear).Times(4);
    EXPECT_CALL(mCertHandler, SetOwner).Times(4);
    EXPECT_CALL(mCertHandler, GetModuleConfig)
        .Times(4)
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig1)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig2)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig3)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig4)));
    EXPECT_CALL(mCertHandler, CreateSelfSignedCert).Times(2);
    EXPECT_CALL(mCallback, OnEncryptDisk).WillOnce(Return(aos::ErrorEnum::eFailed));

    err = mProvisionManager.StartProvisioning("password");

    EXPECT_TRUE(!err.IsNone()) << err.Message();
}

TEST_F(ProvisionManagerTest, GetCertTypes)
{
    CertTypes gneratedCertTypes;
    gneratedCertTypes.EmplaceBack("certType1");
    gneratedCertTypes.EmplaceBack("certType2");
    gneratedCertTypes.EmplaceBack("certType3");
    gneratedCertTypes.EmplaceBack("diskEncryption");

    EXPECT_CALL(mCertHandler, GetCertTypes)
        .Times(1)
        .WillOnce(DoAll(SetArgReferee<0>(gneratedCertTypes), Return(aos::ErrorEnum::eNone)));

    certhandler::ModuleConfig moduleConfig1;
    certhandler::ModuleConfig moduleConfig2;
    certhandler::ModuleConfig moduleConfig3;
    certhandler::ModuleConfig moduleConfig4;

    moduleConfig1.mIsSelfSigned = false;
    moduleConfig2.mIsSelfSigned = false;
    moduleConfig3.mIsSelfSigned = true;
    moduleConfig4.mIsSelfSigned = true;

    EXPECT_CALL(mCertHandler, GetModuleConfig)
        .Times(4)
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig1)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig2)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig3)))
        .WillOnce(Return(aos::RetWithError<certhandler::ModuleConfig>(moduleConfig4)));

    auto certTypes = mProvisionManager.GetCertTypes();

    EXPECT_TRUE(certTypes.mError.IsNone());
    EXPECT_EQ(certTypes.mValue.Size(), 2);
    EXPECT_EQ(certTypes.mValue[0], "certType1");
    EXPECT_EQ(certTypes.mValue[1], "certType2");
}

TEST_F(ProvisionManagerTest, FinishProvisioning)
{
    EXPECT_CALL(mCallback, OnFinishProvisioning).WillOnce(Return(aos::ErrorEnum::eNone));

    auto err = mProvisionManager.FinishProvisioning("password");

    EXPECT_TRUE(err.IsNone()) << err.Message();

    EXPECT_CALL(mCallback, OnFinishProvisioning).WillOnce(Return(aos::ErrorEnum::eFailed));

    err = mProvisionManager.FinishProvisioning("password");

    EXPECT_TRUE(!err.IsNone()) << err.Message();
}

TEST_F(ProvisionManagerTest, CreateKey)
{
    aos::String generatedCsr {"csr"};
    EXPECT_CALL(mCertHandler, CreateKey)
        .Times(1)
        .WillOnce(DoAll(SetArgReferee<3>(generatedCsr), Return(aos::ErrorEnum::eNone)));

    aos::StaticString<aos::crypto::cCSRPEMLen> csr;

    auto err = mProvisionManager.CreateKey("certType", "subject", "password", csr);

    EXPECT_TRUE(err.IsNone()) << err.Message();
    EXPECT_EQ(csr, generatedCsr);
}

TEST_F(ProvisionManagerTest, CreateKeyFails)
{
    EXPECT_CALL(mCertHandler, CreateKey).WillOnce(Return(aos::ErrorEnum::eFailed));

    aos::StaticString<aos::crypto::cCSRPEMLen> csr;

    auto err = mProvisionManager.CreateKey("certType", "subject", "password", csr);

    EXPECT_TRUE(!err.IsNone()) << err.Message();
}

TEST_F(ProvisionManagerTest, ApplyCert)
{
    auto convertByteArrayToAosArray = [](const char* data, size_t size) -> aos::Array<uint8_t> {
        return {reinterpret_cast<const uint8_t*>(data), size};
    };

    int64_t now_sec  = static_cast<int64_t>(time(nullptr));
    int64_t now_nsec = 0;

    aos::iam::certhandler::CertInfo generatedCertInfo;
    generatedCertInfo.mIssuer  = convertByteArrayToAosArray("issuer", strlen("issuer")),
    generatedCertInfo.mSerial  = convertByteArrayToAosArray("serial", strlen("serial")),
    generatedCertInfo.mCertURL = "certURL", generatedCertInfo.mKeyURL = "keyURL",
    generatedCertInfo.mNotAfter = aos::Time::Unix(now_sec, now_nsec).Add(aos::Time::cYear),

    EXPECT_CALL(mCertHandler, ApplyCertificate)
        .Times(1)
        .WillOnce(DoAll(SetArgReferee<2>(generatedCertInfo), Return(aos::ErrorEnum::eNone)));

    aos::iam::certhandler::CertInfo certInfo;
    auto                            err = mProvisionManager.ApplyCert("certType", "pemCert", certInfo);

    EXPECT_TRUE(err.IsNone()) << err.Message();
    EXPECT_EQ(certInfo, generatedCertInfo);
}

TEST_F(ProvisionManagerTest, Deprovision)
{
    EXPECT_CALL(mCallback, OnDeprovision).WillOnce(Return(aos::ErrorEnum::eNone));

    auto err = mProvisionManager.Deprovision("password");

    EXPECT_TRUE(err.IsNone()) << err.Message();

    EXPECT_CALL(mCallback, OnDeprovision).WillOnce(Return(aos::ErrorEnum::eFailed));

    err = mProvisionManager.Deprovision("password");

    EXPECT_TRUE(!err.IsNone()) << err.Message();
}
