/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_SM_NODECONFIG_TESTS_MOCKS_JSONPROVIDERMOCK_HPP_
#define AOS_CORE_SM_NODECONFIG_TESTS_MOCKS_JSONPROVIDERMOCK_HPP_

#include <gmock/gmock.h>

#include <core/common/nodeconfig/itf/jsonprovider.hpp>

namespace aos::sm::nodeconfig {

/**
 * JSON provider mock.
 */
class JSONProviderMock : public aos::nodeconfig::JSONProviderItf {
public:
    MOCK_METHOD(Error, NodeConfigFromJSON, (const String&, aos::NodeConfig&), (const, override));
    MOCK_METHOD(Error, NodeConfigToJSON, (const aos::NodeConfig&, String&), (const, override));
};

} // namespace aos::sm::nodeconfig

#endif
