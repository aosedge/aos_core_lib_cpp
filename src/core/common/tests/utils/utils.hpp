/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TESTS_UTILS_UTILS_HPP_
#define AOS_CORE_COMMON_TESTS_UTILS_UTILS_HPP_

#include <string>
#include <vector>

#include <core/common/tools/string.hpp>

namespace aos::tests::utils {

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

/**
 * Converts a vector to an array.
 *
 * @tparam T type of the elements in the vector.
 * @param src vector to convert.
 * @return Array<T>
 */
template <typename T>
Array<T> ConvertToArray(const std::vector<T>& src)
{
    return Array<T> {src.data(), src.size()};
}

/**
 * Converts an initializer list to an array.
 *
 * @tparam T type of the elements in the initializer list.
 * @param list initializer list to convert.
 * @return Array<T>
 */
template <typename T>
Array<T> ConvertToArray(const std::initializer_list<T>& list)
{
    return Array<T>(list.begin(), list.size());
}

} // namespace aos::tests::utils

#endif
