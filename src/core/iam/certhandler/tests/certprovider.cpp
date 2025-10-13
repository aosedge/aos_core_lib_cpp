/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <core/common/tests/mocks/certprovidermock.hpp>
#include <core/iam/certhandler/certprovider.hpp>
#include <core/iam/tests/mocks/certhandlermock.hpp>

using namespace testing;
using namespace aos::iam;
using namespace aos::iam::certhandler;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CertProviderTest : public testing::Test {
protected:
    void SetUp() override { mCertProvider.Init(mCertHandler); }

    certhandler::CertHandlerMock mCertHandler;
    CertProvider                 mCertProvider;
};

/***********************************************************************************************************************
 * Tests
 * **********************************************************************************************************************/

TEST_F(CertProviderTest, GetCert)
{
    auto convertByteArrayToAosArray = [](const char* data, size_t size) -> aos::Array<uint8_t> {
        return {reinterpret_cast<const uint8_t*>(data), size};
    };

    int64_t       nowSec  = static_cast<int64_t>(time(nullptr));
    int64_t       nowNSec = 0;
    aos::CertInfo certInfo;

    certInfo.mIssuer   = convertByteArrayToAosArray("issuer", strlen("issuer"));
    certInfo.mSerial   = convertByteArrayToAosArray("serial", strlen("serial"));
    certInfo.mCertURL  = "certURL";
    certInfo.mKeyURL   = "keyURL";
    certInfo.mNotAfter = aos::Time::Unix(nowSec, nowNSec).Add(aos::Time::cYear);

    EXPECT_CALL(mCertHandler, GetCertificate)
        .WillOnce(DoAll(SetArgReferee<3>(certInfo), Return(aos::ErrorEnum::eNone)));

    aos::CertInfo result;
    ASSERT_TRUE(mCertProvider.GetCert("certType", certInfo.mIssuer, certInfo.mSerial, result).IsNone());
    EXPECT_EQ(result, certInfo);
}

TEST_F(CertProviderTest, SubscribeCertChanged)
{
    const aos::String certType = "iam";

    aos::iamclient::CertListenerMock certListener;

    EXPECT_CALL(mCertHandler, SubscribeListener(certType, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    ASSERT_TRUE(mCertHandler.SubscribeListener(certType, certListener).IsNone());

    EXPECT_CALL(mCertHandler, UnsubscribeListener(Ref(certListener))).WillOnce(Return(aos::ErrorEnum::eNone));
    ASSERT_TRUE(mCertHandler.UnsubscribeListener(certListener).IsNone());
}
