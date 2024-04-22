/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/iam/permhandler.hpp"
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
    LockGuard lock(mMutex);

    LOG_DBG() << "Register instance: instance = " << instanceIdent;

    Error                    err;
    StaticString<cSecretLen> uuidSecret;

    Tie(uuidSecret, err) = GetSecretForInstance(instanceIdent);
    if (err.IsNone()) {
        return {uuidSecret};
    }

    uuidSecret = GenerateSecret();

    err = AddSecret(uuidSecret, instanceIdent, instancePermissions);
    if (!err.IsNone()) {
        return {"", AOS_ERROR_WRAP(err)};
    }

    return {uuidSecret};
}

Error PermHandler::UnregisterInstance(const InstanceIdent& instanceIdent)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Unregister instance: instance = " << instanceIdent;

    auto result = FindByInstanceIdent(instanceIdent);
    if (!result.mError.IsNone()) {
        LOG_WRN() << "Unregister instance not registered: instance = " << instanceIdent;

        return AOS_ERROR_WRAP(result.mError);
    }

    return AOS_ERROR_WRAP(mInstancesPerms.Remove(result.mValue).mError);
}

Error PermHandler::GetPermissions(const String& secretUUID, const String& funcServerID, InstanceIdent& instanceIdent,
    Array<PermKeyValue>& servicePermissions)
{
    LockGuard lock(mMutex);

    LOG_DBG() << "Get permission: secretUUID = " << secretUUID << ", funcServerID = " << funcServerID;

    const auto result = FindBySecretUUID(secretUUID);
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

Error PermHandler::AddSecret(const String& secretUUID, const InstanceIdent& instanceIdent,
    const Array<FunctionalServicePermissions>& instancePermissions)
{
    const auto err = mInstancesPerms.PushBack(InstancePermissions {secretUUID, instanceIdent, instancePermissions});
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

RetWithError<InstancePermissions*> PermHandler::FindBySecretUUID(const String& secretUUID)
{
    return mInstancesPerms.Find([&secretUUID](const auto& item) { return secretUUID == item.mSecretUUID; });
}

RetWithError<InstancePermissions*> PermHandler::FindByInstanceIdent(const InstanceIdent& instanceIdent)
{
    return mInstancesPerms.Find([&instanceIdent](const auto& elem) { return instanceIdent == elem.mInstanceIdent; });
}

StaticString<cSecretLen> PermHandler::GenerateSecret()
{
    StaticString<cSecretLen> newUUID;

    do {
        newUUID = uuid::UUIDToString(uuid::CreateUUID());

    } while (!FindBySecretUUID(newUUID).mError.Is(ErrorEnum::eNotFound));

    return newUUID;
}

RetWithError<StaticString<cSecretLen>> PermHandler::GetSecretForInstance(const InstanceIdent& instanceIdent)
{
    const auto result = FindByInstanceIdent(instanceIdent);
    if (!result.mError.IsNone()) {
        return {"", AOS_ERROR_WRAP(result.mError)};
    }

    return result.mValue->mSecretUUID;
}

} // namespace permhandler
} // namespace iam
} // namespace aos
