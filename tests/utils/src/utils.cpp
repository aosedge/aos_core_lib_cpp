/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>

#include "aos/common/tools/string.hpp"

namespace aos::test {

std::string ErrorToStr(const Error& error)
{
    std::string result(cMaxErrorStrLen, ' ');

    auto errStr = String(result.c_str());

    errStr.Convert(error);
    result.resize(errStr.Size());

    return result;
}

} // namespace aos::test
