/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_SUBJECTS_OBSERVER_MOCK_HPP_
#define AOS_SUBJECTS_OBSERVER_MOCK_HPP_

#include "aos/iam/identhandler.hpp"
#include <gmock/gmock.h>

namespace aos::iam::identhandler {

/**
 * Subjects observer mock.
 */
class SubjectsObserverMock : public SubjectsObserverItf {
public:
    MOCK_METHOD(Error, SubjectsChanged, (const Array<StaticString<cSubjectIDLen>>&), (override));
};

} // namespace aos::iam::identhandler

#endif
