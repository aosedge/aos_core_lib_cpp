/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/iam/nodeinfoprovider.hpp"

static bool CaseInsensitiveEqual(const aos::String& lhs, const aos::String& rhs)
{
    if (lhs.Size() != rhs.Size()) {
        return false;
    }

    for (size_t i = 0; i < lhs.Size(); ++i) {
        if (tolower(lhs[i]) != tolower(rhs[i])) {
            return false;
        }
    }

    return true;
}

namespace aos::iam::nodeinfoprovider {

bool IsMainNode(const NodeInfo& nodeInfo)
{
    aos::NodeAttribute* attr = nullptr;
    aos::Error          err;

    aos::Tie(attr, err)
        = nodeInfo.mAttrs.Find([](const auto& attr) { return CaseInsensitiveEqual(attr.mName, cAttrMainNode); });

    if (err != aos::ErrorEnum::eNone) {
        assert(err.Is(aos::ErrorEnum::eNotFound));

        return false;
    }

    return true;
}

} // namespace aos::iam::nodeinfoprovider
