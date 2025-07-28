/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OS_HPP_
#define AOS_OS_HPP_

#include <stdlib.h>

#include "aos/common/tools/string.hpp"

namespace aos::os {

/**
 * Returns value of specified environment variable.
 *
 * @param envName name of environment variable.
 * @param env result value.
 * @return Error.
 */
inline Error GetEnv(const String& envName, String& env)
{
    const String envValue = getenv(envName.CStr());
    if (envValue.CStr() == nullptr) {
        return ErrorEnum::eNotFound;
    }

    return env.Insert(env.begin(), envValue.begin(), envValue.end());
}

} // namespace aos::os

#endif
