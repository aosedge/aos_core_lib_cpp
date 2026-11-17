/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <core/common/tests/mocks/instancestatusprovidermock.hpp>
#include <core/common/tests/utils/log.hpp>
#include <core/common/tests/utils/utils.hpp>
#include <core/common/tools/heapallocator.hpp>
#include <core/sm/launcher/statusnotifier.hpp>
#include <core/sm/tests/mocks/instanceidprovidermock.hpp>

#include "stubs/senderstub.hpp"

using namespace testing;

namespace aos::sm::launcher {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class StatusNotifierTest : public Test {
protected:
    static constexpr auto cWaitTimeout = Time::cSeconds;

    static void SetUpTestSuite() { tests::utils::InitLog(); }

    void SetUp() override { mNotifier.Init(mAllocator, mSender); }

    HeapAllocator  mAllocator;
    SenderStub     mSender;
    StatusNotifier mNotifier;
};

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(StatusNotifierTest, SendNodeStatusesSendsStatusOfEachInstance)
{
    NiceMock<InstanceIDProviderMock> instanceIDProvider;
    auto                             instances = std::make_unique<InstanceRegistry>();

    instances->Init(instanceIDProvider);

    InstanceInfo info;

    info.mItemID = "item0";

    for (uint64_t i = 0; i < 2; ++i) {
        info.mInstance = i;

        ASSERT_TRUE(instances->Add(info).mError.IsNone());
    }

    mNotifier.SendNodeStatuses(*instances);

    auto statuses = std::make_unique<InstanceStatusArray>();

    ASSERT_TRUE(mSender.WaitStatuses(*statuses, cWaitTimeout).IsNone());
    ASSERT_EQ(statuses->Size(), 2);
    EXPECT_EQ((*statuses)[0].mInstance, 0);
    EXPECT_EQ((*statuses)[1].mInstance, 1);
}

TEST_F(StatusNotifierTest, SendUpdateStatusesSendsStatuses)
{
    InstanceStatus status;

    status.mItemID = "item0";

    ASSERT_TRUE(mNotifier.SendUpdateStatuses(Array<InstanceStatus>(&status, 1)).IsNone());

    auto statuses = std::make_unique<InstanceStatusArray>();

    ASSERT_TRUE(mSender.WaitStatuses(*statuses, cWaitTimeout).IsNone());
    ASSERT_EQ(statuses->Size(), 1);
    EXPECT_EQ((*statuses)[0].mItemID, "item0");
}

TEST_F(StatusNotifierTest, NotifiesSubscribedListeners)
{
    StrictMock<instancestatusprovider::ListenerMock> listener;
    InstanceStatus                                   status;
    const auto                                       statuses = Array<InstanceStatus>(&status, 1);

    ASSERT_TRUE(mNotifier.Subscribe(listener).IsNone());
    EXPECT_TRUE(mNotifier.Subscribe(listener).Is(ErrorEnum::eAlreadyExist));

    EXPECT_CALL(listener, OnInstancesStatusesChanged).Times(1);

    mNotifier.NotifyListeners(statuses);

    ASSERT_TRUE(mNotifier.Unsubscribe(listener).IsNone());
    EXPECT_TRUE(mNotifier.Unsubscribe(listener).Is(ErrorEnum::eNotFound));

    // The unsubscribed listener isn't notified.
    mNotifier.NotifyListeners(statuses);
}

} // namespace aos::sm::launcher
