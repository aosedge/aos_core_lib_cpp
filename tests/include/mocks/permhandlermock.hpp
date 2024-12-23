/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_PERMHADNLER_MOCK_HPP_
#define AOS_PERMHADNLER_MOCK_HPP_

#include <gmock/gmock.h>

#include "aos/iam/permhandler.hpp"

namespace aos::iam::permhandler {

/**
 * Permission handler mock.
 */
class PermHandlerMock : public PermHandlerItf {
public:
    MOCK_METHOD(RetWithError<StaticString<cSecretLen>>, RegisterInstance,
        (const InstanceIdent&, const Array<FunctionalServicePermissions>&), (override));
    MOCK_METHOD(Error, UnregisterInstance, (const InstanceIdent&), (override));
    MOCK_METHOD(
        Error, GetPermissions, (const String&, const String&, InstanceIdent&, Array<PermKeyValue>&), (override));
};

} // namespace aos::iam::permhandler

#endif
