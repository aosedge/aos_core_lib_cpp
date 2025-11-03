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
    MOCK_METHOD(Error, InstallUpdateItems,
        (const Array<UpdateItemInfo>& itemsInfo, const Array<crypto::CertificateInfo>& certificates,
            const Array<crypto::CertificateChainInfo>& certificateChains, Array<UpdateItemStatus>& statuses),
        (override));
    MOCK_METHOD(Error, UninstallUpdateItems,
        (const Array<StaticString<cIDLen>>& ids, Array<UpdateItemStatus>& statuses), (override));
    MOCK_METHOD(Error, RevertUpdateItems, (const Array<StaticString<cIDLen>>& ids, Array<UpdateItemStatus>& statuses),
        (override));
    MOCK_METHOD(Error, GetUpdateItemsStatuses, (Array<UpdateItemStatus> & statuses), (override));
    MOCK_METHOD(Error, SubscribeListener, (ImageStatusListenerItf & listener), (override));
    MOCK_METHOD(Error, UnsubscribeListener, (ImageStatusListenerItf & listener), (override));
};

} // namespace aos::cm::imagemanager

#endif
