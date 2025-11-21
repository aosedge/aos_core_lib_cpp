/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_UPDATEMANAGER_TESTS_MOCKS_IMAGEMANAGERMOCK_HPP_
#define AOS_CORE_CM_UPDATEMANAGER_TESTS_MOCKS_IMAGEMANAGERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/cm/imagemanager/itf/imagemanager.hpp>

namespace aos::cm::imagemanager {

/**
 * Image manager mock.
 */
class ImageManagerMock : public ImageManagerItf {
public:
    MOCK_METHOD(Error, DownloadUpdateItems,
        (const Array<UpdateItemInfo>& itemsInfo, const Array<crypto::CertificateInfo>&,
            const Array<crypto::CertificateChainInfo>&, Array<UpdateItemStatus>&),
        (override));
    MOCK_METHOD(Error, InstallUpdateItems, (const Array<UpdateItemInfo>&, Array<UpdateItemStatus>&), (override));
    MOCK_METHOD(Error, Cancel, (), (override));
    MOCK_METHOD(Error, GetUpdateItemsStatuses, (Array<UpdateItemStatus>&), (override));
    MOCK_METHOD(Error, SubscribeListener, (ItemStatusListenerItf&), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (ItemStatusListenerItf&), (override));
};

} // namespace aos::cm::imagemanager

#endif
