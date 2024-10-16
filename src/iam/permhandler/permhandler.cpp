/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/iam/permhandler.hpp"
#include "aos/common/tools/uuid.hpp"
#include "log.hpp"

namespace aos {
namespace iam {
namespace permhandler {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

RetWithError<StaticString<cSecretLen>> PermHandler::RegisterInstance(
    const InstanceIdent& instanceIdent, const Array<FunctionalServicePermissions>& instancePermissions)
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
        return {"", AOS_ERROR_WRAP(err)};
    }

    return {secret};
}

Error PermHandler::UnregisterInstance(const InstanceIdent& instanceIdent)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Unregister instance: instance=" << instanceIdent;

    auto result = FindByInstanceIdent(instanceIdent);
    if (!result.mError.IsNone()) {
        LOG_WRN() << "Unregister instance not registered: instance=" << instanceIdent;

        return AOS_ERROR_WRAP(result.mError);
    }

    return AOS_ERROR_WRAP(mInstancesPerms.Remove(result.mValue).mError);
}

Error PermHandler::GetPermissions(const String& secret, const String& funcServerID, InstanceIdent& instanceIdent,
    Array<PermKeyValue>& servicePermissions)
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
    const Array<FunctionalServicePermissions>& instancePermissions)
{
    const auto err = mInstancesPerms.PushBack(InstancePermissions {secret, instanceIdent, instancePermissions});
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<InstancePermissions*> PermHandler::FindBySecret(const String& secret)
{
    return mInstancesPerms.Find([&secret](const auto& item) { return secret == item.mSecret; });
}

RetWithError<InstancePermissions*> PermHandler::FindByInstanceIdent(const InstanceIdent& instanceIdent)
{
    return mInstancesPerms.Find([&instanceIdent](const auto& elem) { return instanceIdent == elem.mInstanceIdent; });
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
        return {"", AOS_ERROR_WRAP(result.mError)};
    }

    return result.mValue->mSecret;
}

} // namespace permhandler
} // namespace iam
} // namespace aos
