/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include <core/common/tests/mocks/certprovidermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/iam/tests/mocks/certloadermock.hpp>
#include <core/iam/tests/mocks/identmodulemock.hpp>

#include <core/iam/identhandler/identhandler.hpp>

using namespace testing;

namespace aos::iam::identhandler {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

constexpr auto cUnitModel   = "unitModel";
constexpr auto cUnitVersion = "1.0.0";
constexpr auto cSystemID    = "systemID";
constexpr auto cCertURL     = "file:///online.pem";

class SubjectsListenerStub : public iamclient::SubjectsListenerItf {
public:
    void SubjectsChanged(const Array<StaticString<cIDLen>>&) override { }
};

class FailingAllocator : public AllocatorItf {
public:
    void* Allocate(size_t) override { return nullptr; }
    void  Free(void*) override { }
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class IdentHandlerTest : public testing::Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        // Ident module lifecycle is managed by the owner, not by ident handler.
        EXPECT_CALL(mIdentModule, Start).Times(0);
        EXPECT_CALL(mIdentModule, Stop).Times(0);
        ON_CALL(mIdentModule, GetUnitModelInfo).WillByDefault(Invoke([](UnitModelInfo& info) {
            info.mUnitModel = cUnitModel;
            info.mVersion   = cUnitVersion;

            return ErrorEnum::eNone;
        }));
    }

    Error InitHandler(bool provisioningMode = false)
    {
        return mIdentHandler.Init(mIdentModule, mCertProvider, mCertLoader, mAllocator, provisioningMode);
    }

    void ExpectOnlineCert(const std::vector<std::string>& subjectURLs)
    {
        EXPECT_CALL(mCertProvider, GetCert(String("online"), _, _, _))
            .WillOnce(Invoke([](const String&, const Array<uint8_t>&, const Array<uint8_t>&, CertInfo& certInfo) {
                certInfo.mCertURL = cCertURL;

                return ErrorEnum::eNone;
            }));

        ExpectCertChain(cCertURL, subjectURLs);
    }

    void ExpectCertChain(const std::string& certURL, const std::vector<std::string>& subjectURLs)
    {
        EXPECT_CALL(mCertLoader, LoadCertsChainByURL(Truly([certURL](const String& url) {
            return certURL == url.CStr();
        }))).WillOnce(Invoke([this, subjectURLs](const String&) {
            auto chain = MakeShared<crypto::x509::CertificateChain>(&mAllocator);

            chain->EmplaceBack();

            for (const auto& url : subjectURLs) {
                chain->Back().mSubjectURLs.EmplaceBack(url.c_str());
            }

            return RetWithError<SharedPtr<crypto::x509::CertificateChain>>(chain);
        }));
    }

    void ExpectSubscribe()
    {
        EXPECT_CALL(mCertProvider, SubscribeListener(String("online"), _))
            .WillOnce(Invoke([this](const String&, iamclient::CertListenerItf& listener) {
                mCertListener = &listener;

                return ErrorEnum::eNone;
            }));
    }

    void StartSubscribed(const std::vector<std::string>& subjectURLs)
    {
        ExpectOnlineCert(subjectURLs);
        ExpectSubscribe();

        auto err = mIdentHandler.Start();
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
        ASSERT_NE(mCertListener, nullptr);
    }

    void NotifyCertChanged(const char* certURL)
    {
        CertInfo certInfo;

        certInfo.mCertURL = certURL;

        mCertListener->OnCertChanged(certInfo);
    }

    HeapAllocator               mAllocator;
    NiceMock<IdentModuleMock>   mIdentModule;
    iamclient::CertProviderMock mCertProvider;
    crypto::CertLoaderMock      mCertLoader;
    IdentHandler                mIdentHandler;
    iamclient::CertListenerItf* mCertListener {};
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(IdentHandlerTest, GetSubjectsFromIdentModule)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mIdentModule, GetSubjects).WillOnce(Invoke([](Array<StaticString<cIDLen>>& subjects) {
        subjects.EmplaceBack("subject1");
        subjects.EmplaceBack("subject2");

        return ErrorEnum::eNone;
    }));

    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> subjects;

    err = mIdentHandler.GetSubjects(subjects);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(subjects.Size(), 2);
    EXPECT_STREQ(subjects[0].CStr(), "subject1");
    EXPECT_STREQ(subjects[1].CStr(), "subject2");
}

TEST_F(IdentHandlerTest, GetSubjectsFails)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mIdentModule, GetSubjects).WillOnce(Return(ErrorEnum::eFailed));

    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> subjects;

    err = mIdentHandler.GetSubjects(subjects);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);
}

TEST_F(IdentHandlerTest, SubjectsListenerIsForwardedToIdentModule)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    SubjectsListenerStub listener;

    EXPECT_CALL(mIdentModule, SubscribeListener(Ref(listener))).WillOnce(Return(ErrorEnum::eNone));

    err = mIdentHandler.SubscribeListener(listener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mIdentModule, UnsubscribeListener(Ref(listener))).WillOnce(Return(ErrorEnum::eNone));

    err = mIdentHandler.UnsubscribeListener(listener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(IdentHandlerTest, StartInProvisioningModeDoesNotSubscribe)
{
    auto err = InitHandler(true);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mCertProvider, GetCert).Times(0);
    EXPECT_CALL(mCertProvider, SubscribeListener).Times(0);
    EXPECT_CALL(mCertProvider, UnsubscribeListener).Times(0);

    err = mIdentHandler.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mIdentHandler.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(IdentHandlerTest, StartReadsSystemIDFromOnlineCert)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StartSubscribed({"https://example.com", std::string("urn:aos:unit:") + cSystemID});

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cSystemID);
    EXPECT_STREQ(systemInfo->mUnitModel.CStr(), cUnitModel);
    EXPECT_STREQ(systemInfo->mVersion.CStr(), cUnitVersion);

    EXPECT_CALL(mCertProvider, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));

    err = mIdentHandler.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(IdentHandlerTest, StartFailsOnSubscribeError)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mCertProvider, GetCert).WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_CALL(mCertProvider, SubscribeListener).WillOnce(Return(ErrorEnum::eNotFound));

    err = mIdentHandler.Start();
    EXPECT_TRUE(err.Is(ErrorEnum::eNotFound)) << tests::utils::ErrorToStr(err);

    // Not subscribed: stop must not unsubscribe.
    EXPECT_CALL(mCertProvider, UnsubscribeListener).Times(0);

    err = mIdentHandler.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(IdentHandlerTest, StartSucceedsWhenOnlineCertIsNotAvailable)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mCertProvider, GetCert).WillOnce(Return(ErrorEnum::eNotFound));
    ExpectSubscribe();

    err = mIdentHandler.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // Subscribed: System ID comes from cert change notifications, cert is not requested again.
    EXPECT_CALL(mCertProvider, GetCert).Times(0);

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(systemInfo->mSystemID.IsEmpty());
    EXPECT_STREQ(systemInfo->mUnitModel.CStr(), cUnitModel);
    EXPECT_STREQ(systemInfo->mVersion.CStr(), cUnitVersion);
}

TEST_F(IdentHandlerTest, GetSystemInfoFailsOnUnitModelInfoError)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mIdentModule, GetUnitModelInfo).WillOnce(Return(ErrorEnum::eFailed));

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);
}

TEST_F(IdentHandlerTest, GetSystemInfoReadsSystemIDWhenEmpty)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectOnlineCert({std::string("urn:aos:unit:") + cSystemID});

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cSystemID);

    // System ID is cached, cert is not requested again.
    EXPECT_CALL(mCertProvider, GetCert).Times(0);

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cSystemID);
}

TEST_F(IdentHandlerTest, GetSystemInfoReadsSystemIDInProvisioningMode)
{
    auto err = InitHandler(true);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mIdentHandler.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // Online cert is not available until provisioning applies it.
    EXPECT_CALL(mCertProvider, GetCert).WillOnce(Return(ErrorEnum::eNotFound));

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(systemInfo->mSystemID.IsEmpty());

    ExpectOnlineCert({std::string("urn:aos:unit:") + cSystemID});

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cSystemID);
}

TEST_F(IdentHandlerTest, SystemIDIsEmptyWhenCertHasNoSystemIDURL)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectOnlineCert({"https://example.com", "urn:aos:unit:"});

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(systemInfo->mSystemID.IsEmpty());
}

TEST_F(IdentHandlerTest, OnCertChangedUpdatesSystemID)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StartSubscribed({std::string("urn:aos:unit:") + cSystemID});

    constexpr auto cNewCertURL  = "file:///online-new.pem";
    constexpr auto cNewSystemID = "newSystemID";

    // Cert info from the notification is used, cert provider is not called back.
    EXPECT_CALL(mCertProvider, GetCert).Times(0);

    ExpectCertChain(cNewCertURL, {std::string("urn:aos:unit:") + cNewSystemID});

    NotifyCertChanged(cNewCertURL);

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cNewSystemID);
}

TEST_F(IdentHandlerTest, OnCertChangedKeepsSystemIDWhenNewCertHasNoSystemID)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StartSubscribed({std::string("urn:aos:unit:") + cSystemID});

    constexpr auto cNewCertURL = "file:///online-new.pem";

    ExpectCertChain(cNewCertURL, {"https://example.com"});

    NotifyCertChanged(cNewCertURL);

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cSystemID);
}

TEST_F(IdentHandlerTest, StopFailsOnUnsubscribeError)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StartSubscribed({std::string("urn:aos:unit:") + cSystemID});

    EXPECT_CALL(mCertProvider, UnsubscribeListener).WillOnce(Return(ErrorEnum::eFailed));

    err = mIdentHandler.Stop();
    EXPECT_TRUE(err.Is(ErrorEnum::eFailed)) << tests::utils::ErrorToStr(err);

    // Subscription flag is reset: second stop must not unsubscribe again.
    EXPECT_CALL(mCertProvider, UnsubscribeListener).Times(0);

    err = mIdentHandler.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(IdentHandlerTest, GetSystemInfoReturnsEmptySystemIDOnNoMemory)
{
    FailingAllocator allocator;

    auto err = mIdentHandler.Init(mIdentModule, mCertProvider, mCertLoader, allocator, false);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mCertProvider, GetCert).Times(0);

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(systemInfo->mSystemID.IsEmpty());
    EXPECT_STREQ(systemInfo->mUnitModel.CStr(), cUnitModel);
}

TEST_F(IdentHandlerTest, GetSystemInfoReturnsEmptySystemIDOnInvalidCertChain)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mCertProvider, GetCert(String("online"), _, _, _))
        .Times(3)
        .WillRepeatedly(Invoke([](const String&, const Array<uint8_t>&, const Array<uint8_t>&, CertInfo& certInfo) {
            certInfo.mCertURL = cCertURL;

            return ErrorEnum::eNone;
        }));

    EXPECT_CALL(mCertLoader, LoadCertsChainByURL)
        // Chain load error.
        .WillOnce(Return(RetWithError<SharedPtr<crypto::x509::CertificateChain>>(nullptr, ErrorEnum::eFailed)))
        // No chain.
        .WillOnce(Return(RetWithError<SharedPtr<crypto::x509::CertificateChain>>(nullptr)))
        // Empty chain.
        .WillOnce(Invoke([this](const String&) {
            return RetWithError<SharedPtr<crypto::x509::CertificateChain>>(
                MakeShared<crypto::x509::CertificateChain>(&mAllocator));
        }));

    for (int i = 0; i < 3; ++i) {
        auto systemInfo = std::make_unique<SystemInfo>();

        err = mIdentHandler.GetSystemInfo(*systemInfo);
        ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

        EXPECT_TRUE(systemInfo->mSystemID.IsEmpty()) << "iteration " << i;
    }
}

TEST_F(IdentHandlerTest, OnCertChangedKeepsSystemIDOnInvalidCert)
{
    auto err = InitHandler();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StartSubscribed({std::string("urn:aos:unit:") + cSystemID});

    EXPECT_CALL(mCertLoader, LoadCertsChainByURL)
        .WillOnce(Return(RetWithError<SharedPtr<crypto::x509::CertificateChain>>(nullptr, ErrorEnum::eFailed)));

    NotifyCertChanged("file:///broken.pem");

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentHandler.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cSystemID);
}

} // namespace aos::iam::identhandler
