// SPDX-License-Identifier: Apache-2.0
//
// Copyright (C) 2023 Renesas Electronics Corporation.
// Copyright (C) 2023 EPAM Systems, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ERROR_HPP_
#define ERROR_HPP_

#include "utils/stringer.hpp"

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
