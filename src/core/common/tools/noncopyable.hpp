/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_NONCOPIABLE_HPP_
#define AOS_NONCOPIABLE_HPP_

namespace aos {

class NonCopyable {
public:
    NonCopyable(const NonCopyable&)            = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

protected:
    NonCopyable(void) = default;
};

} // namespace aos

#endif
