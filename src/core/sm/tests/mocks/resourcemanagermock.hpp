/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_TESTS_MOCKS_RESOURCEMANAGERMOCK_HPP_
#define AOS_CORE_SM_TESTS_MOCKS_RESOURCEMANAGERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/sm/resourcemanager/itf/resourceinfoprovider.hpp>

namespace aos::sm::resourcemanager {

/**
 * Resource info provider mock.
 */
class ResourceInfoProviderMock : public ResourceInfoProviderItf {
public:
    MOCK_METHOD(Error, GetResourcesInfos, (Array<aos::ResourceInfo>&), (override));
    MOCK_METHOD(Error, GetResourceInfo, (const String&, ResourceInfo&), (override));
};

} // namespace aos::sm::resourcemanager

#endif
