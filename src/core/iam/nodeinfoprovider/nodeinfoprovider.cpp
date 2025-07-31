/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nodeinfoprovider.hpp"

namespace aos::iam::nodeinfoprovider {

bool IsMainNode(const NodeInfo& nodeInfo)
{
    return nodeInfo.mAttrs.FindIf([](const auto& attr) {
        return !attr.mName.Compare(cAttrMainNode, String::CaseSensitivity::CaseInsensitive);
    }) != nodeInfo.mAttrs.end();
}

} // namespace aos::iam::nodeinfoprovider
