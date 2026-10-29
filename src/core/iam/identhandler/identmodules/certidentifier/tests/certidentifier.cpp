/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>
#include <vector>

#include <gmock/gmock.h>

#include <core/common/tests/mocks/certprovidermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/fs.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/iam/tests/mocks/certloadermock.hpp>

#include <core/iam/identhandler/identmodules/certidentifier/certidentifier.hpp>

using namespace testing;

namespace aos::iam::identhandler {

namespace {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

constexpr auto cUnitModelPath = "unitModel";
constexpr auto cSubjectsPath  = "subjects";
constexpr auto cUnitModel     = "unitModel";
constexpr auto cUnitVersion   = "1.0.0";
constexpr auto cSystemID      = "systemID";
constexpr auto cCertURL       = "file:///online.pem";
constexpr auto cSubjects      = R"(subject1
subject2
subject3)";

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CertIdentifierTest : public testing::Test {
protected:
    void SetUp() override
    {
        tests::utils::InitLog();

        if (std::ofstream f(cUnitModelPath); f) {
            f << cUnitModel << ";" << cUnitVersion;
        }

        if (std::ofstream f(cSubjectsPath); f) {
            f << cSubjects;
        }

        mConfig.mUnitModelPath = cUnitModelPath;
        mConfig.mSubjectsPath  = cSubjectsPath;
    }

    Error InitIdentifier(bool provisioningMode = false)
    {
        return mIdentifier.Init(mConfig, mCertProvider, mCertLoader, mAllocator, provisioningMode);
    }

    void ExpectOnlineCert(const std::vector<std::string>& subjectURLs)
    {
        EXPECT_CALL(mCertProvider, GetCert(String("online"), _, _, _))
            .WillOnce(Invoke([](const String&, const Array<uint8_t>&, const Array<uint8_t>&, CertInfo& certInfo) {
                certInfo.mCertURL = cCertURL;

                return ErrorEnum::eNone;
            }));

        EXPECT_CALL(mCertLoader, LoadCertsChainByURL(String(cCertURL)))
            .WillOnce(Invoke([this, subjectURLs](const String&) {
                auto chain = MakeShared<crypto::x509::CertificateChain>(&mAllocator);

                chain->EmplaceBack();

                for (const auto& url : subjectURLs) {
                    chain->Back().mSubjectURLs.EmplaceBack(url.c_str());
                }

                return RetWithError<SharedPtr<crypto::x509::CertificateChain>>(chain);
            }));
    }

    HeapAllocator               mAllocator;
    CertIdentifierConfig        mConfig;
    iamclient::CertProviderMock mCertProvider;
    crypto::CertLoaderMock      mCertLoader;
    CertIdentifier              mIdentifier;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(CertIdentifierTest, InitFailsOnEmptyConfig)
{
    mConfig = CertIdentifierConfig {};

    const auto err = InitIdentifier();
    ASSERT_FALSE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CertIdentifierTest, InitFailsOnUnitVersionFileError)
{
    const std::vector<std::string> cUnitModelFileContents = {
        {std::string("")},
        {std::string("noVersion")},
        {std::string("unitModel;").append(std::string(cVersionLen + 1, 'a'))},
    };

    for (const auto& fileContents : cUnitModelFileContents) {
        if (std::ofstream f(cUnitModelPath); f) {
            f << fileContents;
        }

        const auto err = InitIdentifier();
        EXPECT_FALSE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }
}

TEST_F(CertIdentifierTest, InitFailsOnUnitModelFileMissing)
{
    fs::Remove(cUnitModelPath);

    auto err = InitIdentifier();
    ASSERT_EQ(err.Value(), ErrorEnum::eRuntime) << tests::utils::ErrorToStr(err);
}

TEST_F(CertIdentifierTest, InitSucceedsOnSubjectsFileMissing)
{
    fs::Remove(cSubjectsPath);

    auto err = InitIdentifier();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> subjects;

    err = mIdentifier.GetSubjects(subjects);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(subjects.IsEmpty());
}

TEST_F(CertIdentifierTest, GetSubjects)
{
    auto err = InitIdentifier();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    StaticArray<StaticString<cIDLen>, cMaxNumSubjects> subjects;

    err = mIdentifier.GetSubjects(subjects);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ASSERT_EQ(subjects.Size(), 3);
    EXPECT_STREQ(subjects[0].CStr(), "subject1");
    EXPECT_STREQ(subjects[1].CStr(), "subject2");
    EXPECT_STREQ(subjects[2].CStr(), "subject3");
}

TEST_F(CertIdentifierTest, StartInProvisioningModeDoesNotSubscribe)
{
    auto err = InitIdentifier(true);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mCertProvider, GetCert).Times(0);
    EXPECT_CALL(mCertProvider, SubscribeListener).Times(0);
    EXPECT_CALL(mCertProvider, UnsubscribeListener).Times(0);

    err = mIdentifier.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mIdentifier.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CertIdentifierTest, StartReadsSystemIDFromOnlineCert)
{
    auto err = InitIdentifier();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectOnlineCert({"https://example.com", std::string("urn:aos:unit:") + cSystemID});

    EXPECT_CALL(mCertProvider, SubscribeListener(String("online"), Ref(mIdentifier)))
        .WillOnce(Return(ErrorEnum::eNone));

    err = mIdentifier.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentifier.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cSystemID);
    EXPECT_STREQ(systemInfo->mUnitModel.CStr(), cUnitModel);
    EXPECT_STREQ(systemInfo->mVersion.CStr(), cUnitVersion);

    EXPECT_CALL(mCertProvider, UnsubscribeListener(Ref(mIdentifier))).WillOnce(Return(ErrorEnum::eNone));

    err = mIdentifier.Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(CertIdentifierTest, StartSucceedsWhenOnlineCertIsNotAvailable)
{
    auto err = InitIdentifier();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(mCertProvider, GetCert).WillOnce(Return(ErrorEnum::eNotFound));
    EXPECT_CALL(mCertProvider, SubscribeListener).WillOnce(Return(ErrorEnum::eNone));

    err = mIdentifier.Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    // System ID is requested again on GetSystemInfo while it is empty.
    EXPECT_CALL(mCertProvider, GetCert).WillOnce(Return(ErrorEnum::eNotFound));

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentifier.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(systemInfo->mSystemID.IsEmpty());
    EXPECT_STREQ(systemInfo->mUnitModel.CStr(), cUnitModel);
    EXPECT_STREQ(systemInfo->mVersion.CStr(), cUnitVersion);
}

TEST_F(CertIdentifierTest, GetSystemInfoReadsSystemIDWhenEmpty)
{
    auto err = InitIdentifier();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectOnlineCert({std::string("urn:aos:unit:") + cSystemID});

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentifier.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cSystemID);

    // System ID is cached, cert is not requested again.
    EXPECT_CALL(mCertProvider, GetCert).Times(0);

    err = mIdentifier.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cSystemID);
}

TEST_F(CertIdentifierTest, SystemIDIsEmptyWhenCertHasNoSystemIDURL)
{
    auto err = InitIdentifier();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectOnlineCert({"https://example.com", "urn:aos:unit:"});

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentifier.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_TRUE(systemInfo->mSystemID.IsEmpty());
}

TEST_F(CertIdentifierTest, OnCertChangedUpdatesSystemID)
{
    auto err = InitIdentifier();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    ExpectOnlineCert({std::string("urn:aos:unit:") + cSystemID});

    mIdentifier.OnCertChanged(CertInfo {});

    constexpr auto cNewSystemID = "newSystemID";

    ExpectOnlineCert({std::string("urn:aos:unit:") + cNewSystemID});

    mIdentifier.OnCertChanged(CertInfo {});

    auto systemInfo = std::make_unique<SystemInfo>();

    err = mIdentifier.GetSystemInfo(*systemInfo);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_STREQ(systemInfo->mSystemID.CStr(), cNewSystemID);
}

} // namespace aos::iam::identhandler
