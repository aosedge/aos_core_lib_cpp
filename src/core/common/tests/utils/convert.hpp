/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * This file provides helper functions for converting between standard library types
 * and Aos types.
 */

#ifndef AOS_CORE_COMMON_TESTS_UTILS_CONVERT_HPP_
#define AOS_CORE_COMMON_TESTS_UTILS_CONVERT_HPP_

#include <vector>

#include <core/common/tools/array.hpp>

namespace aos::tests::utils {

/**
 * Converts Aos array to std::vector.
 *
 * @tparam T element type.
 * @param src source array.
 * @return std::vector<T>.
 */
template <typename T>
std::vector<T> ToVector(const Array<T>& src)
{
    return std::vector<T>(src.begin(), src.end());
}

} // namespace aos::tests::utils

#endif
