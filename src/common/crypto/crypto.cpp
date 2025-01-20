/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/crypto/crypto.hpp"

namespace aos::crypto::asn1 {

RetWithError<StaticString<cTimeStrLen>> ConvertTimeToASN1Str(const Time& time)
{
    int day = 0, month = 0, year = 0, hour = 0, min = 0, sec = 0;

    auto err = time.GetDate(&day, &month, &year);
    if (!err.IsNone()) {
        return {{}, err};
    }

    err = time.GetTime(&hour, &min, &sec);
    if (!err.IsNone()) {
        return {{}, err};
    }

    StaticString<cTimeStrLen> result;

    result.Resize(result.MaxSize());
    snprintf(result.Get(), result.Size(), "%04d%02d%02d%02d%02d%02d", year, month, day, hour, min, sec);
    result.Resize(strlen(result.CStr()));

    return {result, ErrorEnum::eNone};
}

} // namespace aos::crypto::asn1
