/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/iam/permhandler.hpp"
#include "aos/common/tools/uuid.hpp"
#include "log.hpp"

namespace aos::iam::permhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

RetWithError<StaticString<cSecretLen>> PermHandler::RegisterInstance(
    const InstanceIdent& instanceIdent, const Array<FunctionServicePermissions>& instancePermissions)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Register instance: instance=" << instanceIdent;

    Error                    err;
    StaticString<cSecretLen> secret;

    Tie(secret, err) = GetSecretForInstance(instanceIdent);
    if (err.IsNone()) {
        return {secret};
    }

    secret = GenerateSecret();

    err = AddSecret(secret, instanceIdent, instancePermissions);
    if (!err.IsNone()) {
        return {{}, AOS_ERROR_WRAP(err)};
    }

    return {secret};
}

Error PermHandler::UnregisterInstance(const InstanceIdent& instanceIdent)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unregister instance: instance=" << instanceIdent;

    if (mInstancesPerms.RemoveIf([&instanceIdent](const auto& item) { return instanceIdent == item.mInstanceIdent; })
        == 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    return ErrorEnum::eNone;
}

Error PermHandler::GetPermissions(const String& secret, const String& funcServerID, InstanceIdent& instanceIdent,
    Array<FunctionPermissions>& servicePermissions)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Get permission: secret=" << secret << ", funcServerID=" << funcServerID;

    const auto result = FindBySecret(secret);
    if (!result.mError.IsNone()) {
        return AOS_ERROR_WRAP(result.mError);
    }

    instanceIdent = result.mValue->mInstanceIdent;

    for (const auto& it : result.mValue->mFuncServicePerms) {
        if (it.mName == funcServerID) {
            if (it.mPermissions.Size() > servicePermissions.MaxSize()) {
                return AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
            }

            servicePermissions = it.mPermissions;

            return ErrorEnum::eNone;
        }
    }

    return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error PermHandler::AddSecret(const String& secret, const InstanceIdent& instanceIdent,
    const Array<FunctionServicePermissions>& instancePermissions)
{
    const auto err = mInstancesPerms.PushBack(InstancePermissions {secret, instanceIdent, instancePermissions});
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<InstancePermissions*> PermHandler::FindBySecret(const String& secret)
{
    return mInstancesPerms.FindIf([&secret](const auto& item) { return secret == item.mSecret; });
}

RetWithError<InstancePermissions*> PermHandler::FindByInstanceIdent(const InstanceIdent& instanceIdent)
{
    return mInstancesPerms.FindIf([&instanceIdent](const auto& elem) { return instanceIdent == elem.mInstanceIdent; });
}

StaticString<cSecretLen> PermHandler::GenerateSecret()
{
    StaticString<cSecretLen> newSecret;

    do {
        newSecret = uuid::UUIDToString(uuid::CreateUUID());

    } while (!FindBySecret(newSecret).mError.Is(ErrorEnum::eNotFound));

    return newSecret;
}

RetWithError<StaticString<cSecretLen>> PermHandler::GetSecretForInstance(const InstanceIdent& instanceIdent)
{
    const auto result = FindByInstanceIdent(instanceIdent);
    if (!result.mError.IsNone()) {
        return {{}, AOS_ERROR_WRAP(result.mError)};
    }

    return result.mValue->mSecret;
}

} // namespace aos::iam::permhandler
