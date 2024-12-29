/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/common/tools/semver.hpp"
#include "aos/common/types.hpp"

namespace aos::semver {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

namespace {

/***********************************************************************************************************************
 * Consts
 **********************************************************************************************************************/

constexpr auto cMaxNumIdentifiers = 8;
const char*    cAllowedChars      = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-";
const char*    cNumericChars      = "0123456789";

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

bool IsNumericIdentifier(const String& identifier)
{
    for (const auto ch : identifier) {
        auto isNumeric = false;

        for (size_t i = 0; i < sizeof(cNumericChars); i++) {
            if (ch == cNumericChars[i]) {
                isNumeric = true;
                break;
            }
        }

        if (!isNumeric) {
            return false;
        }
    }

    return true;
}

Error ValidateChars(const String& identifier, const char* allowedChars)
{
    for (const auto ch : identifier) {
        auto allowed = false;

        for (size_t i = 0; i < strlen(allowedChars); i++) {
            if (ch == allowedChars[i]) {
                allowed = true;
                break;
            }
        }

        if (!allowed) {
            return ErrorEnum::eInvalidArgument;
        }
    }

    return ErrorEnum::eNone;
}

Error GetIdentifiers(String& version, Array<String>& identifiers)
{
    identifiers.Clear();

    size_t identPos = 0;
    auto   pos      = version.begin();
    Error  err;

    while (true) {
        Tie(identPos, err) = version.FindSubstr(identPos, ".");
        if (err.IsNone()) {
            version[identPos] = '\0';
            identPos++;
        }

        if (auto pushErr = identifiers.PushBack(pos); !pushErr.IsNone()) {
            return pushErr;
        }

        if (!err.IsNone()) {
            if (!err.Is(ErrorEnum::eNotFound)) {
                return err;
            } else {
                return ErrorEnum::eNone;
            }
        }

        pos = version.begin() + identPos;
    }

    return ErrorEnum::eNone;
}

Error ValidateNumericIdentifier(const String& identifier)
{
    if (auto err = ValidateChars(identifier, cNumericChars); !err.IsNone()) {
        return err;
    }

    if (identifier.Size() == 0) {
        return ErrorEnum::eInvalidArgument;
    }

    if (identifier.Size() > 1 && identifier[0] == '0') {
        return ErrorEnum::eInvalidArgument;
    }

    return ErrorEnum::eNone;
}

Error ValidateStrIdentifier(const String& identifier)
{
    if (auto err = ValidateChars(identifier, cAllowedChars); !err.IsNone()) {
        return err;
    }

    if (identifier.Size() == 0) {
        return ErrorEnum::eInvalidArgument;
    }

    return ErrorEnum::eNone;
}

Error ValidateBasePart(String& version)
{
    StaticArray<String, cMaxNumIdentifiers> identifiers;

    if (auto err = GetIdentifiers(version, identifiers); !err.IsNone()) {
        return err;
    }

    if (identifiers.Size() == 0 || identifiers.Size() > 3) {
        return ErrorEnum::eInvalidArgument;
    }

    for (const auto& identifier : identifiers) {
        if (auto err = ValidateNumericIdentifier(identifier); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error ValidatePrereleasePart(String& version)
{
    StaticArray<String, cMaxNumIdentifiers> identifiers;

    if (auto err = GetIdentifiers(version, identifiers); !err.IsNone()) {
        return err;
    }

    for (const auto& identifier : identifiers) {
        if (IsNumericIdentifier(identifier)) {
            if (auto err = ValidateNumericIdentifier(identifier); !err.IsNone()) {
                return err;
            }
        } else {
            if (auto err = ValidateStrIdentifier(identifier); !err.IsNone()) {
                return err;
            }
        }
    }

    return ErrorEnum::eNone;
}

Error ValidateMetadataPart(String& version)
{
    StaticArray<String, cMaxNumIdentifiers> identifiers;

    if (auto err = GetIdentifiers(version, identifiers); !err.IsNone()) {
        return err;
    }

    for (const auto& identifier : identifiers) {
        if (auto err = ValidateStrIdentifier(identifier); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

Error SplitVersion(String& version, String& basePart, String& prereleasePart, String& metadataPart)
{
    size_t prereleasePos;
    size_t metadataPos;
    Error  err;

    Tie(prereleasePos, err) = version.FindSubstr(0, "-");
    Tie(metadataPos, err)   = version.FindSubstr(0, "+");

    if (prereleasePos < version.Size()) {
        version[prereleasePos] = '\0';
        prereleasePos++;
    }

    if (metadataPos < version.Size()) {
        version[metadataPos] = '\0';
        metadataPos++;
    }

    basePart.Rebind(version.CStr());
    prereleasePart.Rebind(version.CStr() + prereleasePos);
    metadataPart.Rebind(version.CStr() + metadataPos);

    return ErrorEnum::eNone;
}

RetWithError<int> CompareNumericIdentifiers(const String& identifier1, const String& identifier2)
{
    auto result1 = identifier1.ToUint64();
    if (!result1.mError.IsNone()) {
        return {0, result1.mError};
    }

    auto result2 = identifier2.ToUint64();
    if (!result2.mError.IsNone()) {
        return {0, result2.mError};
    }

    if (result1.mValue < result2.mValue) {
        return {-1, ErrorEnum::eNone};
    }

    if (result1.mValue > result2.mValue) {
        return {1, ErrorEnum::eNone};
    }

    return {0, ErrorEnum::eNone};
}

RetWithError<int> CompareStrIdentifiers(const String& identifier1, const String& identifier2)
{
    if (identifier1 < identifier2) {
        return {-1, ErrorEnum::eNone};
    }

    if (identifier1 > identifier2) {
        return {1, ErrorEnum::eNone};
    }

    return {0, ErrorEnum::eNone};
}

RetWithError<int> CompareBaseParts(String& version1, String& version2)
{
    StaticArray<String, cMaxNumIdentifiers> identifiers1;
    StaticArray<String, cMaxNumIdentifiers> identifiers2;

    if (auto err = GetIdentifiers(version1, identifiers1); !err.IsNone()) {
        return {0, err};
    }

    if (identifiers1.Size() == 0 || identifiers1.Size() > 3) {
        return {0, ErrorEnum::eInvalidArgument};
    }

    if (auto err = GetIdentifiers(version2, identifiers2); !err.IsNone()) {
        return {0, err};
    }

    if (identifiers2.Size() == 0 || identifiers2.Size() > 3) {
        return {0, ErrorEnum::eInvalidArgument};
    }

    for (size_t i = 0; i < Min(identifiers1.Size(), identifiers2.Size()); i++) {
        if (auto result = CompareNumericIdentifiers(identifiers1[i], identifiers2[i]);
            !result.mError.IsNone() || result.mValue != 0) {
            return result;
        }
    }

    if (identifiers1.Size() < identifiers2.Size()) {
        return {-1, ErrorEnum::eNone};
    }

    if (identifiers1.Size() > identifiers2.Size()) {
        return {1, ErrorEnum::eNone};
    }

    return {0, ErrorEnum::eNone};
}

RetWithError<int> ComparePrereleaseParts(String& version1, String& version2)
{
    StaticArray<String, cMaxNumIdentifiers> identifiers1;
    StaticArray<String, cMaxNumIdentifiers> identifiers2;

    if (auto err = GetIdentifiers(version1, identifiers1); !err.IsNone()) {
        return {0, err};
    }

    if (auto err = GetIdentifiers(version2, identifiers2); !err.IsNone()) {
        return {0, err};
    }

    for (size_t i = 0; i < Min(identifiers1.Size(), identifiers2.Size()); i++) {
        auto isNumeric1 = IsNumericIdentifier(identifiers1[i]);
        auto isNumeric2 = IsNumericIdentifier(identifiers2[i]);

        if (isNumeric1 && !isNumeric2) {
            return {-1, ErrorEnum::eNone};
        }

        if (isNumeric2 && !isNumeric1) {
            return {1, ErrorEnum::eNone};
        }

        if (isNumeric1 && isNumeric2) {
            if (auto result = CompareNumericIdentifiers(identifiers1[i], identifiers2[i]);
                !result.mError.IsNone() || result.mValue != 0) {
                return result;
            }
        }

        if (auto result = CompareStrIdentifiers(identifiers1[i], identifiers2[i]);
            !result.mError.IsNone() || result.mValue != 0) {
            return result;
        }

        continue;
    }

    if (identifiers1.Size() < identifiers2.Size()) {
        return {-1, ErrorEnum::eNone};
    }

    if (identifiers1.Size() > identifiers2.Size()) {
        return {1, ErrorEnum::eNone};
    }

    return {0, ErrorEnum::eNone};
}

} // namespace

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error ValidateSemver(const String& version)
{
    StaticString<cVersionLen> tmpVersion(version);
    String                    basePart, prereleasePart, metadataPart;

    if (auto err = SplitVersion(tmpVersion, basePart, prereleasePart, metadataPart); !err.IsNone()) {
        return err;
    }

    if (auto err = ValidateBasePart(basePart); !err.IsNone()) {
        return err;
    }

    if (!prereleasePart.IsEmpty()) {
        if (auto err = ValidatePrereleasePart(prereleasePart); !err.IsNone()) {
            return err;
        }
    }

    if (!metadataPart.IsEmpty()) {
        if (auto err = ValidateMetadataPart(metadataPart); !err.IsNone()) {
            return err;
        }
    }

    return ErrorEnum::eNone;
}

RetWithError<int> CompareSemver(const String& version1, const String& version2)
{
    if (auto err = ValidateSemver(version1); !err.IsNone()) {
        return {0, err};
    }

    if (auto err = ValidateSemver(version2); !err.IsNone()) {
        return {0, err};
    }

    StaticString<cVersionLen> tmpVersion1(version1);
    StaticString<cVersionLen> tmpVersion2(version2);
    String                    basePart1, prereleasePart1, metadataPart1;
    String                    basePart2, prereleasePart2, metadataPart2;

    if (auto err = SplitVersion(tmpVersion1, basePart1, prereleasePart1, metadataPart1); !err.IsNone()) {
        return {0, err};
    }

    if (auto err = SplitVersion(tmpVersion2, basePart2, prereleasePart2, metadataPart2); !err.IsNone()) {
        return {0, err};
    }

    if (auto result = CompareBaseParts(basePart1, basePart2); !result.mError.IsNone() || result.mValue != 0) {
        return result;
    }

    if (!prereleasePart1.IsEmpty() && prereleasePart2.IsEmpty()) {
        return {-1, ErrorEnum::eNone};
    }

    if (prereleasePart1.IsEmpty() && !prereleasePart2.IsEmpty()) {
        return {1, ErrorEnum::eNone};
    }

    if (prereleasePart1.IsEmpty() && prereleasePart2.IsEmpty()) {
        return {0, ErrorEnum::eNone};
    }

    return ComparePrereleaseParts(prereleasePart1, prereleasePart2);
}

} // namespace aos::semver
