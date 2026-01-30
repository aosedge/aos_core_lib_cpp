/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/fs.hpp>
#include <core/common/tools/logger.hpp>

#include "cryptoutils.hpp"

namespace aos::crypto {

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cReadChunkSize = AOS_CONFIG_TYPES_READ_FILE_BUFFER_SIZE;

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

Mutex                                sReadBufferMutex;
StaticArray<uint8_t, cReadChunkSize> sReadBuffer;

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error CalculateFileHash(const String& path, const Hash& algorithm, HasherItf& hashProvider, Array<uint8_t>& hash)
{
    auto [hasher, err] = hashProvider.CreateHash(algorithm);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    fs::File file;

    err = file.Open(path, fs::File::Mode::Read);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    LockGuard lock {sReadBufferMutex};

    while (true) {
        err = file.ReadBlock(sReadBuffer);
        if (!err.IsNone() && !err.Is(ErrorEnum::eEOF)) {
            return AOS_ERROR_WRAP(err);
        }

        if (sReadBuffer.IsEmpty()) {
            break;
        }

        err = hasher->Update(sReadBuffer);
        if (!err.IsNone()) {
            if (!err.Is(ErrorEnum::eEOF)) {
                return AOS_ERROR_WRAP(err);
            }

            break;
        }
    }

    err = hasher->Finalize(hash);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

} // namespace aos::crypto
