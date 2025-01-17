/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_TEST_UTILS_HPP_
#define AOS_TEST_UTILS_HPP_

#include <string>

#include "aos/common/tools/string.hpp"

namespace aos::test {

/**
 * Compares two Aos arrays.
 *
 * @tparam T1 type of the first array.
 * @tparam T2 type of the second array.
 * @param array1 first array.
 * @param array2 second array.
 * @return bool.
 */
template <typename T1, typename T2>
static bool CompareArrays(const T1 array1, const T2 array2)
{
    if (array1.Size() != array2.Size()) {
        return false;
    }

    for (const auto& item : array1) {
        if (array2.Find(item) == array2.end()) {
            return false;
        }
    }

    for (const auto& item : array2) {
        if (array1.Find(item) == array1.end()) {
            return false;
        }
    }

    return true;
}

/**
 * Converts error to string.
 *
 * @param error
 * @return std::string
 */
std::string ErrorToStr(const Error& error);

} // namespace aos::test

#endif
