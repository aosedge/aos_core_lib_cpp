/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_TEST_UTILS_HPP_
#define AOS_TEST_UTILS_HPP_

namespace aos {

class TestUtils {
public:
    template <typename T1, typename T2>
    static bool CompareArrays(const T1 array1, const T2 array2)
    {
        if (array1.Size() != array2.Size()) {
            return false;
        }

        for (const auto& instance : array1) {
            if (!array2.Find(instance).mError.IsNone()) {
                return false;
            }
        }

        for (const auto& instance : array2) {
            if (!array1.Find(instance).mError.IsNone()) {
                return false;
            }
        }

        return true;
    }
};

} // namespace aos

#endif
