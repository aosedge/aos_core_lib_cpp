/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include <aos/iam/certprovider.hpp>

#include "mocks/certhandlermock.hpp"
#include "mocks/certreceivermock.hpp"

using namespace testing;
using namespace aos::iam;
using namespace aos::iam::certprovider;

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class CertProviderTest : public testing::Test {
protected:
    void SetUp() override { mCertProvider.Init(mCertHandler); }

    CertHandlerItfMock mCertHandler;
    CertProvider       mCertProvider;
};

/***********************************************************************************************************************
 * Tests
 * **********************************************************************************************************************/

TEST_F(CertProviderTest, GetCert)
{
    auto convertByteArrayToAosArray = [](const char* data, size_t size) -> aos::Array<uint8_t> {
        return {reinterpret_cast<const uint8_t*>(data), size};
    };

    int64_t                         nowSec  = static_cast<int64_t>(time(nullptr));
    int64_t                         nowNSec = 0;
    aos::iam::certhandler::CertInfo certInfo;

    certInfo.mIssuer   = convertByteArrayToAosArray("issuer", strlen("issuer"));
    certInfo.mSerial   = convertByteArrayToAosArray("serial", strlen("serial"));
    certInfo.mCertURL  = "certURL";
    certInfo.mKeyURL   = "keyURL";
    certInfo.mNotAfter = aos::Time::Unix(nowSec, nowNSec).Add(aos::Time::cYear);

    EXPECT_CALL(mCertHandler, GetCertificate)
        .WillOnce(DoAll(SetArgReferee<3>(certInfo), Return(aos::ErrorEnum::eNone)));

    aos::iam::certhandler::CertInfo result;
    ASSERT_TRUE(mCertProvider.GetCert("certType", certInfo.mIssuer, certInfo.mSerial, result).IsNone());
    EXPECT_EQ(result, certInfo);
}

TEST_F(CertProviderTest, SubscribeCertChanged)
{
    const aos::String certType = "iam";

    CertReceiverItfMock certReceiver;

    EXPECT_CALL(mCertHandler, SubscribeCertChanged(certType, _)).WillOnce(Return(aos::ErrorEnum::eNone));
    ASSERT_TRUE(mCertHandler.SubscribeCertChanged(certType, certReceiver).IsNone());

    EXPECT_CALL(mCertHandler, UnsubscribeCertChanged(Ref(certReceiver))).WillOnce(Return(aos::ErrorEnum::eNone));
    ASSERT_TRUE(mCertHandler.UnsubscribeCertChanged(certReceiver).IsNone());
}
