/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CNI_MOCK_HPP_
#define AOS_CNI_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/sm/cni.hpp"

namespace aos::sm::cni {

class CNIMock : public CNIItf {
public:
    MOCK_METHOD(Error, SetConfDir, (const String& configDir), (override));
    MOCK_METHOD(
        Error, AddNetworkList, (const NetworkConfigList& net, const RuntimeConf& rt, Result& result), (override));
    MOCK_METHOD(Error, DeleteNetworkList, (const NetworkConfigList& net, const RuntimeConf& rt), (override));
    MOCK_METHOD(Error, ValidateNetworkList, (const NetworkConfigList& net), (override));
    MOCK_METHOD(aos::Error, GetNetworkListCachedConfig, (NetworkConfigList & net, RuntimeConf& rt), (override));
};

} // namespace aos::sm::cni

#endif
