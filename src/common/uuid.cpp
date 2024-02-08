/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <time.h>

#include "aos/common/uuid.hpp"

namespace aos {
namespace uuid {

// UUID template assumed to have even number of digits between separators.
static const String cTemplate = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";

UUID CreateUUID()
{
    UUID result;

    while (result.Size() < result.MaxSize()) {
        unsigned value = rand();
        auto     chunk = Array<uint8_t>(reinterpret_cast<uint8_t*>(&value), sizeof(value));

        auto chunkSize = Min(result.MaxSize() - result.Size(), chunk.Size());

        result.Insert(result.end(), chunk.begin(), chunk.begin() + chunkSize);
    }

    return result;
}

StaticString<cUUIDStrLen> UUIDToString(const UUID& src)
{
    StaticString<cUUIDStrLen> result;

    assert(cTemplate.Size() <= result.MaxSize());
    assert(src.Size() == src.MaxSize());

    for (size_t i = 0; i < src.Size(); i++) {
        if (cTemplate[result.Size()] == '-') {
            result.PushBack('-');
        }

        StaticString<2> chunk;
        const auto      curByte = Array<uint8_t>(src.Get() + i, 1);

        chunk.ByteArrayToHex(curByte);

        result.Insert(result.end(), chunk.begin(), chunk.end());
    }

    return result;
}

RetWithError<UUID> StringToUUID(const StaticString<uuid::cUUIDStrLen>& src)
{
    assert(cTemplate.Size() == src.Size());

    UUID result;

    for (size_t i = 0; i < src.Size();) {
        if (src[i] == '-') {
            i++;
            continue;
        }

        if (i + 1 >= src.Size()) {
            return {result, ErrorEnum::eInvalidArgument};
        }

        assert(i + 2 <= src.Size());
        assert(src[i + 1] != '-');

        StaticString<2>         srcChunk;
        StaticArray<uint8_t, 1> resultChunk;

        srcChunk.Insert(srcChunk.begin(), src.Get() + i, src.Get() + i + 2);

        srcChunk.HexToByteArray(resultChunk);

        result.Append(resultChunk);

        i += 2;
    }

    return {result, ErrorEnum::eNone};
}

} // namespace uuid
} // namespace aos
