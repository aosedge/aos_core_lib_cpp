/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CRYPTO_ITF_RAND_HPP_
#define AOS_CORE_COMMON_CRYPTO_ITF_RAND_HPP_

#include <core/common/tools/memory.hpp>
#include <core/common/tools/string.hpp>

namespace aos::crypto {

/**
 * Random generator interface.
 */
class RandomItf {
public:
    /**
     * Generates random integer value in range [0..maxValue].
     *
     * @param maxValue maximum value.
     * @return RetWithError<uint64_t>.
     */
    virtual RetWithError<uint64_t> RandInt(uint64_t maxValue) = 0;

    /**
     * Generates random buffer.
     *
     * @param[out] buffer result buffer.
     * @param size buffer size.
     * @return Error.
     */
    virtual Error RandBuffer(Array<uint8_t>& buffer, size_t size = 0) = 0;

    /**
     * Destructor.
     */
    virtual ~RandomItf() = default;
};

/**
 * Generates random string.
 *
 * @param result result string.
 * @param random random interface.
 * @return Error.
 */
template <size_t size>
Error GenerateRandomString(String& result, RandomItf& random)
{
    StaticArray<uint8_t, size> buffer;

    if (auto err = random.RandBuffer(buffer, size); !err.IsNone()) {
        return err;
    }

    return result.ByteArrayToHex(buffer);
}

} // namespace aos::crypto

#endif // AOS_CORE_COMMON_CRYPTO_ITF_RAND_HPP_
