/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nodeinfoprovider.hpp"

namespace aos::iam::nodeinfoprovider {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cNodeComponentStrLen  = 8;
constexpr auto cMaxNumNodeComponents = static_cast<size_t>(CoreComponentEnum::eNumComponents);

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

bool IsMainNode(const NodeInfoObsolete& nodeInfo)
{
    return nodeInfo.mAttrs.FindIf([](const auto& attr) {
        return !attr.mName.Compare(cAttrMainNode, String::CaseSensitivity::CaseInsensitive);
    }) != nodeInfo.mAttrs.end();
}

bool ContainsComponent(const NodeInfo& nodeInfo, const CoreComponent& component)
{
    auto it = nodeInfo.mAttrs.FindIf([](const NodeAttribute& attr) {
        return !attr.mName.Compare(cAttrAosComponents, String::CaseSensitivity::CaseInsensitive);
    });

    if (it == nodeInfo.mAttrs.end()) {
        return false;
    }

    StaticArray<StaticString<cNodeComponentStrLen>, cMaxNumNodeComponents> components;

    if (auto err = it->mValue.Split(components, ','); !err.IsNone()) {
        return false;
    }

    return components.FindIf([&component](const String& compStr) {
        return !compStr.Compare(component.ToString(), String::CaseSensitivity::CaseInsensitive);
    }) != components.end();
}

} // namespace aos::iam::nodeinfoprovider
