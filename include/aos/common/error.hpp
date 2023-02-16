/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ERROR_HPP_
#define AOS_ERROR_HPP_

#include "aos/common/stringer.hpp"

namespace aos {

/**
 * Error types.
 *
 */
class ErrorType {
public:
    enum class Enum { eNone, eFailed, eNumErrors };

    static Pair<const char* const*, size_t> GetStrings()
    {
        static const char* const cErrorTypeStrings[static_cast<size_t>(Enum::eNumErrors)] = {"none", "failed"};

        return Pair<const char* const*, size_t>(cErrorTypeStrings, static_cast<size_t>(Enum::eNumErrors));
    };
};

typedef ErrorType::Enum ErrorEnum;

/**
 * Aos errors.
 */
class Error : public EnumStringer<ErrorType> {
public:
    using EnumStringer<ErrorType>::EnumStringer;

    /**
     * Checks if error is none.
     */
    bool IsNone() const { return GetValue() == ErrorEnum::eNone; }

    /**
     * Checks if error has specified type.
     *
     * @param e error to check.
     * @return bool result.
     */
    bool Is(const Error& e) const { return GetValue() == e.GetValue(); }
};

} // namespace aos

#endif
