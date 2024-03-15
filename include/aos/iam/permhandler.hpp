/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_PERMHANDLER_HPP_
#define AOS_PERMHANDLER_HPP_

#include "aos/common/tools/thread.hpp"
#include "aos/common/tools/utils.hpp"
#include "aos/common/tools/uuid.hpp"
#include "aos/common/types.hpp"
#include "aos/iam/certmodules/certmodule.hpp"
#include "aos/iam/config.hpp"

namespace aos {
namespace iam {
namespace permhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Maximum length of permhandler permission key string.
 */
constexpr auto cPermKeyStrLen = AOS_CONFIG_PERMHANDLER_PERM_KEY_LEN;

/**
 * Maximum length of permhandler permission value string.
 */
constexpr auto cPermValueStlLen = AOS_CONFIG_PERMHANDLER_PERM_VALUE_LEN;

/**
 * Maximum number of permhandler service permissions.
 */
constexpr auto cServicePermissionMaxCount = AOS_CONFIG_PERMHANDLER_SERVICE_PERMS_MAX_COUNT;

/**
 * Permission key value.
 */
struct PermKeyValue {
    StaticString<cPermKeyStrLen>   mKey;
    StaticString<cPermValueStlLen> mValue;

    /**
     * Compares permission key value.
     *
     * @param rhs object to compare.
     * @return bool.
     */
    bool operator==(const PermKeyValue& rhs) { return (mKey == rhs.mKey) && (mValue == rhs.mValue); }
};

/**
 * Functional service permissions.
 */
struct FunctionalServicePermissions {
    StaticString<cSystemIDLen>                            mName;
    StaticArray<PermKeyValue, cServicePermissionMaxCount> mPermissions;
};

/**
 * Instance permissions.
 */
struct InstancePermissions {
    StaticString<uuid::cUUIDStrLen>                            mSecretUUID;
    InstanceIdent                                              mInstanceIdent;
    StaticArray<FunctionalServicePermissions, cMaxNumServices> mFuncServicePerms;
};

/**
 * Permission handler interface.
 */
class PermHandlerItf {
public:
    /**
     * Adds new service instance and its permissions into cache.
     *
     * @param instanceIdent instance identification.
     * @param instancePermissions instance permissions.
     * @returns RetWithError<StaticString<uuid::cUUIDStrLen>>.
     */
    virtual RetWithError<StaticString<uuid::cUUIDStrLen>> RegisterInstance(
        const InstanceIdent& instanceIdent, const Array<FunctionalServicePermissions>& instancePermissions)
        = 0;

    /**
     * Unregisters instance deletes service instance with permissions from cache.
     *
     * @param instanceIdent instance identification.
     * @returns Error.
     */
    virtual Error UnregisterInstance(const InstanceIdent& instanceIdent) = 0;

    /**
     * Retruns instance ident and permissions by secret UUID and functional server ID.
     *
     * @param secretUUID secret UUID.
     * @param funcServerID functional server ID.
     * @param[out] instanceIdent result instance ident.
     * @param[out] servicePermissions result service permission.
     * @returns Error.
     */
    virtual Error GetPermissions(const String& secretUUID, const String& funcServerID, InstanceIdent& instanceIdent,
        Array<PermKeyValue>& servicePermissions)
        = 0;

    virtual ~PermHandlerItf() = default;
};

/**
 * Permission handler implements PermHandlerItf.
 */
class PermHandler : public PermHandlerItf {
public:
    /**
     * Adds new service instance and its permissions into cache.
     *
     * @param instanceIdent instance identification.
     * @param instancePermissions instance permissions.
     * @returns RetWithError<StaticString<uuid::cUUIDStrLen>>.
     */
    RetWithError<StaticString<uuid::cUUIDStrLen>> RegisterInstance(
        const InstanceIdent& instanceIdent, const Array<FunctionalServicePermissions>& instancePermissions) override;

    /**
     * Unregisters instance deletes service instance with permissions from cache.
     *
     * @param instanceIdent instance identification.
     * @returns Error.
     */
    Error UnregisterInstance(const InstanceIdent& instanceIdent) override;

    /**
     * Retruns instance ident and permissions by secret UUID and functional server ID.
     *
     * @param secretUUID secret UUID.
     * @param funcServerID functional server ID.
     * @param[out] instanceIdent result instance ident.
     * @param[out] servicePermissions result service permission.
     * @returns Error.
     */
    Error GetPermissions(const String& secretUUID, const String& funcServerID, InstanceIdent& instanceIdent,
        Array<PermKeyValue>& servicePermissions) override;

private:
    Error                              AddSecret(const String& secretUUID, const InstanceIdent& instanceIdent,
                                     const Array<FunctionalServicePermissions>& instancePermissions);
    RetWithError<InstancePermissions*> FindBySecretUUID(const String& secretUUID);
    RetWithError<InstancePermissions*> FindByInstanceIdent(const InstanceIdent& instanceIdent);
    StaticString<uuid::cUUIDStrLen>    GenerateSecret();
    RetWithError<StaticString<uuid::cUUIDStrLen>> GetSecretForInstance(const InstanceIdent& instanceIdent);

    Mutex                                              mMutex;
    StaticArray<InstancePermissions, cMaxNumInstances> mInstancesPerms;
};

/** @}*/

} // namespace permhandler
} // namespace iam
} // namespace aos

#endif
