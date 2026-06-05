/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/mocks/cloudconnectionmock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>

#include <core/cm/alerts/alerts.hpp>

using namespace testing;

namespace aos::cm::alerts {

namespace {

/***********************************************************************************************************************
 * Stubs
 **********************************************************************************************************************/

class SenderStub : public SenderItf {
public:
    Error SendAlerts(const aos::Alerts& alerts) override
    {
        std::lock_guard lock {mMutex};

        LOG_DBG() << "Send alerts called" << Log::Field("items", alerts.mItems.Size());

        mMessages.push_back(alerts);
        mCondVar.notify_all();

        return ErrorEnum::eNone;
    }

    bool WaitForMessage(aos::Alerts& message, std::chrono::milliseconds timeout = std::chrono::seconds(5))
    {
        std::unique_lock lock {mMutex};

        if (!mCondVar.wait_for(lock, timeout, [&]() { return !mMessages.empty(); })) {
            return false;
        }

        message = std::move(mMessages.front());

        mMessages.erase(mMessages.begin());

        return true;
    }

    std::vector<aos::Alerts> GetMessages()
    {
        std::lock_guard lock {mMutex};

        return mMessages;
    }

private:
    std::mutex               mMutex;
    std::condition_variable  mCondVar;
    std::vector<aos::Alerts> mMessages;
};

/***********************************************************************************************************************
 * Mocks
 **********************************************************************************************************************/

class AlertsListenerMock : public AlertsListenerItf {
public:
    MOCK_METHOD(Error, OnAlertReceived, (const AlertVariant& alert), (override));
};

std::unique_ptr<AlertVariant> CreateSystemAlert(
    const Time& timestamp, const std::string& nodeID = "node1", const std::string& message = "test message")
{
    auto alert        = std::make_unique<SystemAlert>();
    alert->mTimestamp = timestamp;
    alert->mNodeID    = nodeID.c_str();
    alert->mMessage   = message.c_str();

    return std::make_unique<AlertVariant>(*alert);
}

std::unique_ptr<AlertVariant> CreateCoreAlert(
    const Time& timestamp, const std::string& nodeID = "node1", const std::string& message = "test message")
{
    auto alert        = std::make_unique<CoreAlert>();
    alert->mTimestamp = timestamp;
    alert->mNodeID    = nodeID.c_str();
    alert->mMessage   = message.c_str();

    return std::make_unique<AlertVariant>(*alert);
}

std::unique_ptr<aos::Alerts> CreatePackage(size_t messageStartSuffix = 0)
{
    auto alerts = std::make_unique<aos::Alerts>();

    while (!alerts->mItems.IsFull()) {
        auto alert = CreateSystemAlert(Time::Now(), "node1", "test message " + std::to_string(messageStartSuffix++));

        alerts->mItems.PushBack(*alert);
    }

    return alerts;
}

} // namespace

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class AlertsTest : public Test {
protected:
    void SetUp() override { tests::utils::InitLog(); }

    alerts::Config                       mConfig {Time::cSeconds * 1};
    SenderStub                           mCommunication;
    cloudconnection::CloudConnectionMock mCloudConnection;
    std::unique_ptr<Alerts>              mAlerts = std::make_unique<Alerts>();
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(AlertsTest, DuplicatesAreSkipped)
{
    const auto     cTime                    = Time::Now();
    constexpr auto cExpectedSentAlertsCount = 3;

    const std::array cAlerts = {
        CreateSystemAlert(cTime, "node1", "test message 1"),
        CreateSystemAlert(cTime.Add(Time::cSeconds), "node1", "test message 1"),
        CreateSystemAlert(cTime.Add(Time::cSeconds * 2), "node1", "test message 1"),
        CreateSystemAlert(cTime.Add(Time::cSeconds * 3), "node1", "test message 1"),
        CreateCoreAlert(cTime, "node1", "test message 2"),
        CreateCoreAlert(cTime.Add(Time::cSeconds), "node1", "test message 2"),
        CreateCoreAlert(cTime.Add(Time::cSeconds * 2), "node2", "test message 3"),
    };

    cloudconnection::ConnectionListenerItf* connectionListener = nullptr;

    EXPECT_CALL(mCloudConnection, SubscribeListener)
        .WillOnce(Invoke([&connectionListener](cloudconnection::ConnectionListenerItf& listener) {
            connectionListener = &listener;

            return ErrorEnum::eNone;
        }));

    auto err = mAlerts->Init(mConfig, mCommunication, mCloudConnection);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mAlerts->Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    connectionListener->OnConnect();

    for (const auto& alert : cAlerts) {
        err = mAlerts->OnAlertReceived(*alert);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    auto msg = std::make_unique<aos::Alerts>();
    EXPECT_TRUE(mCommunication.WaitForMessage(*msg));

    EXPECT_EQ(msg->mItems.Size(), cExpectedSentAlertsCount);

    EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));

    err = mAlerts->Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(AlertsTest, AlertIsSkippedIfBufferIsFull)
{
    const auto cTime = Time::Now();

    cloudconnection::ConnectionListenerItf* connectionListener = nullptr;

    EXPECT_CALL(mCloudConnection, SubscribeListener)
        .WillOnce(Invoke([&connectionListener](cloudconnection::ConnectionListenerItf& listener) {
            connectionListener = &listener;

            return ErrorEnum::eNone;
        }));

    std::string message;

    for (size_t i = 0; i < cAlertsCacheSize; ++i) {
        auto alert = CreateCoreAlert(cTime, "node1", std::to_string(i));

        auto err = mAlerts->OnAlertReceived(*alert);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    auto err = mAlerts->Init(mConfig, mCommunication, mCloudConnection);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mAlerts->OnAlertReceived(*CreateCoreAlert(cTime, "node1", "skipped alert"));
    EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mAlerts->Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    connectionListener->OnConnect();

    size_t receivedAlertsCount = 0;

    for (size_t i = 0; i < cAlertsCacheSize / cAlertItemsCount; ++i) {
        auto msg = std::make_unique<aos::Alerts>();

        EXPECT_TRUE(mCommunication.WaitForMessage(*msg));

        if (!msg) {
            continue;
        }

        if (msg) {
            const auto alertsInPackage = msg->mItems.Size();

            EXPECT_EQ(alertsInPackage, cAlertItemsCount);

            receivedAlertsCount += alertsInPackage;
        }
    }

    EXPECT_EQ(receivedAlertsCount, cAlertsCacheSize);

    EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));

    err = mAlerts->Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(AlertsTest, PackagesAreSent)
{
    const std::array cAlertPackages {
        CreatePackage(0),
        CreatePackage(cAlertItemsCount),
    };

    cloudconnection::ConnectionListenerItf* connectionListener = nullptr;

    EXPECT_CALL(mCloudConnection, SubscribeListener)
        .WillOnce(Invoke([&connectionListener](cloudconnection::ConnectionListenerItf& listener) {
            connectionListener = &listener;

            return ErrorEnum::eNone;
        }));

    std::vector<aos::Alerts> receivedPackages;

    mConfig.mSendPeriod = Time::cSeconds * 3;

    auto err = mAlerts->Init(mConfig, mCommunication, mCloudConnection);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mAlerts->Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    connectionListener->OnConnect();

    for (const auto& package : cAlertPackages) {
        for (const auto& alert : package->mItems) {
            err = mAlerts->OnAlertReceived(alert);
            EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
        }

        auto msg = std::make_unique<aos::Alerts>();
        EXPECT_TRUE(mCommunication.WaitForMessage(*msg));

        if (msg) {
            EXPECT_EQ(msg->mItems, package->mItems);

            receivedPackages.push_back(*msg);
        } else {
            FAIL() << "Received message is not alerts";
        }
    }

    EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));

    err = mAlerts->Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(AlertsTest, PackagesAreSentOnReconnect)
{
    const auto package = CreatePackage();

    cloudconnection::ConnectionListenerItf* connectionListener = nullptr;

    EXPECT_CALL(mCloudConnection, SubscribeListener)
        .WillOnce(Invoke([&connectionListener](cloudconnection::ConnectionListenerItf& listener) {
            connectionListener = &listener;

            return ErrorEnum::eNone;
        }));

    std::vector<aos::Alerts> receivedPackages;

    auto err = mAlerts->Init(mConfig, mCommunication, mCloudConnection);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mAlerts->Start();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    for (const auto& alert : package->mItems) {
        err = mAlerts->OnAlertReceived(alert);
        EXPECT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
    }

    auto msg = std::make_unique<aos::Alerts>();
    EXPECT_FALSE(mCommunication.WaitForMessage(*msg));

    connectionListener->OnConnect();

    EXPECT_TRUE(mCommunication.WaitForMessage(*msg));

    if (msg) {
        EXPECT_EQ(msg->mItems, package->mItems);

        receivedPackages.push_back(*msg);
    } else {
        FAIL() << "Received message is not alerts";
    }

    EXPECT_CALL(mCloudConnection, UnsubscribeListener).WillOnce(Return(ErrorEnum::eNone));

    err = mAlerts->Stop();
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

TEST_F(AlertsTest, ListenersAreNotified)
{
    AlertsListenerMock coreAndSystemAlertsListener;
    AlertsListenerMock systemAlertsListener;

    const std::vector<AlertTag> systemAndCoreAlertTags = {
        AlertTagEnum::eSystemAlert,
        AlertTagEnum::eCoreAlert,
    };

    auto err = mAlerts->Init(mConfig, mCommunication, mCloudConnection);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mAlerts->SubscribeListener(
        Array<AlertTag>(&systemAndCoreAlertTags.front(), systemAndCoreAlertTags.size()), coreAndSystemAlertsListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mAlerts->SubscribeListener(Array<AlertTag>(&systemAndCoreAlertTags.front(), 1), systemAlertsListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    auto coreAlert   = CreateCoreAlert(Time::Now(), "node1", "core alert message");
    auto systemAlert = CreateSystemAlert(Time::Now(), "node1", "system alert message");

    EXPECT_CALL(systemAlertsListener, OnAlertReceived(_)).Times(0);
    EXPECT_CALL(coreAndSystemAlertsListener, OnAlertReceived(*coreAlert)).Times(1).WillOnce(Return(ErrorEnum::eNone));

    err = mAlerts->OnAlertReceived(*coreAlert);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(systemAlertsListener, OnAlertReceived(*systemAlert)).Times(1).WillOnce(Return(ErrorEnum::eNone));
    EXPECT_CALL(coreAndSystemAlertsListener, OnAlertReceived(*systemAlert)).Times(1).WillOnce(Return(ErrorEnum::eNone));

    err = mAlerts->OnAlertReceived(*systemAlert);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mAlerts->UnsubscribeListener(systemAlertsListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    EXPECT_CALL(systemAlertsListener, OnAlertReceived(_)).Times(0);
    EXPECT_CALL(coreAndSystemAlertsListener, OnAlertReceived(*systemAlert)).Times(1).WillOnce(Return(ErrorEnum::eNone));

    err = mAlerts->OnAlertReceived(*systemAlert);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);

    err = mAlerts->UnsubscribeListener(coreAndSystemAlertsListener);
    ASSERT_TRUE(err.IsNone()) << tests::utils::ErrorToStr(err);
}

} // namespace aos::cm::alerts
