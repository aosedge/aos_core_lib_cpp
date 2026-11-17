/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/mocks/cloudconnectionmock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/sm/launcher/offlinehandler.hpp>

using namespace testing;

namespace aos::sm::launcher {

namespace {

/***********************************************************************************************************************
 * Utils
 **********************************************************************************************************************/

class OfflineCheckListenerStub : public OfflineCheckListenerItf {
public:
    void OnOfflineCheck() override
    {
        std::lock_guard lock {mMutex};

        mNumChecks++;

        mCondVar.notify_all();
    }

    bool WaitChecks(int numChecks, std::chrono::milliseconds timeout = std::chrono::seconds(1))
    {
        std::unique_lock lock {mMutex};

        return mCondVar.wait_for(lock, timeout, [&]() { return mNumChecks >= numChecks; });
    }

    int NumChecks()
    {
        std::lock_guard lock {mMutex};

        return mNumChecks;
    }

private:
    std::mutex              mMutex;
    std::condition_variable mCondVar;
    int                     mNumChecks {};
};

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class OfflineHandlerTest : public Test {
protected:
    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override
    {
        EXPECT_CALL(mCloudConnection, SubscribeListener)
            .WillOnce(Invoke([this](cloudconnection::ConnectionListenerItf& listener) {
                mConnectionListener = &listener;

                return ErrorEnum::eNone;
            }));
        EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));
    }

    void TearDown() override { EXPECT_TRUE(mHandler.Stop().IsNone()); }

    void Start(bool connected)
    {
        EXPECT_CALL(mCloudConnection, IsConnected).WillOnce(Return(connected));

        ASSERT_TRUE(mHandler.Start(mCloudConnection, mListener).IsNone());
    }

    StrictMock<cloudconnection::CloudConnectionMock> mCloudConnection;
    cloudconnection::ConnectionListenerItf*          mConnectionListener {};
    OfflineCheckListenerStub                         mListener;
    OfflineHandler                                   mHandler;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(OfflineHandlerTest, OnlineNodeNeverExpires)
{
    Start(true);

    EXPECT_EQ(mListener.NumChecks(), 0);
    EXPECT_FALSE(mHandler.IsExpired(Time::cMilliseconds));

    // Arming does nothing while online.
    mHandler.Arm(Time::cMilliseconds);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(mListener.NumChecks(), 0);
}

TEST_F(OfflineHandlerTest, OfflineOnStartRequestsCheck)
{
    Start(false);

    EXPECT_EQ(mListener.NumChecks(), 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(mHandler.IsExpired(Time::cMilliseconds));
    EXPECT_FALSE(mHandler.IsExpired(Time::cHours));

    // The instance without offline TTL never expires.
    EXPECT_FALSE(mHandler.IsExpired(0));
}

TEST_F(OfflineHandlerTest, ArmedDeadlineRequestsCheck)
{
    Start(true);

    mConnectionListener->OnDisconnect();

    ASSERT_EQ(mListener.NumChecks(), 1);

    mHandler.Arm(Time::cMilliseconds * 20);

    EXPECT_TRUE(mListener.WaitChecks(2));
    EXPECT_TRUE(mHandler.IsExpired(Time::cMilliseconds * 20));
}

TEST_F(OfflineHandlerTest, ConnectCancelsArmedCheck)
{
    Start(true);

    mConnectionListener->OnDisconnect();
    mHandler.Arm(Time::cMilliseconds * 100);
    mConnectionListener->OnConnect();

    EXPECT_FALSE(mListener.WaitChecks(2, std::chrono::milliseconds(300)));
    EXPECT_FALSE(mHandler.IsExpired(Time::cMilliseconds));
}

} // namespace aos::sm::launcher
