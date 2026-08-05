/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "itf/asn1.hpp"

namespace aos::crypto::asn1 {

RetWithError<StaticString<cTimeStrLen>> ConvertTimeToASN1Str(const Time& time)
{
    int32_t day = 0, month = 0, year = 0, hour = 0, min = 0, sec = 0;

    auto err = time.GetDate(&day, &month, &year);
    if (!err.IsNone()) {
        return {{}, err};
    }

    err = time.GetTime(&hour, &min, &sec);
    if (!err.IsNone()) {
        return {{}, err};
    }

    StaticString<cTimeStrLen> result;

    (void)result.Resize(result.MaxSize());
    (void)snprintf(result.Get(), result.Size(), "%04d%02d%02d%02d%02d%02dZ", year, month, day, hour, min, sec);
    (void)result.Resize(strlen(result.CStr()));

    return {result, ErrorEnum::eNone};
}

} // namespace aos::crypto::asn1
