/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "aos/sm/cni.hpp"

using namespace aos::sm::cni;
using namespace testing;

class MockCNI : public CNIItf {
public:
    MOCK_METHOD(aos::Error, Init, (const aos::String& configDir), (override));
    MOCK_METHOD(
        (aos::RetWithError<Result>), AddNetworkList, (const NetworkConfigList& net, const RuntimeConf& rt), (override));
    MOCK_METHOD(aos::Error, DeleteNetworkList, (const NetworkConfigList& net, const RuntimeConf& rt), (override));
    MOCK_METHOD(aos::Error, ValidateNetworkList, (const NetworkConfigList& net), (override));
};
