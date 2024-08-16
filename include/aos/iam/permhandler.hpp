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
constexpr auto cPermissionKeyLen = AOS_CONFIG_PERMHANDLER_PERM_KEY_LEN;

/**
 * Maximum length of permhandler permission value string.
 */
constexpr auto cPermissionValueLen = AOS_CONFIG_PERMHANDLER_PERM_VALUE_LEN;

/**
 * Maximum number of permhandler service permissions.
 */
constexpr auto cServicePermissionMaxCount = AOS_CONFIG_PERMHANDLER_SERVICE_PERMS_MAX_COUNT;

/**
 * Maximum length of functional service name.
 */
constexpr auto cFunctionalServiceNameLen = AOS_CONFIG_PERMHANDLER_FUNC_SERVICE_NAME_LEN;

/**
 * Maximum number of permhandler functional services.
 */
constexpr auto cFuncServiceMaxCount = AOS_CONFIG_PERMHANDLER_FUNC_SERVICE_MAX_COUNT;

/**
 * Maximum length of permhandler secret.
 */
constexpr auto cSecretLen = AOS_CONFIG_PERMHANDLER_SECRET_LEN;

/**
 * Permission key value.
 */
struct PermKeyValue {
    StaticString<cPermissionKeyLen>   mKey;
    StaticString<cPermissionValueLen> mValue;

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
    StaticString<cFunctionalServiceNameLen>               mName;
    StaticArray<PermKeyValue, cServicePermissionMaxCount> mPermissions;
};

/**
 * Instance permissions.
 */
struct InstancePermissions {
    StaticString<cSecretLen>                                        mSecret;
    InstanceIdent                                                   mInstanceIdent;
    StaticArray<FunctionalServicePermissions, cFuncServiceMaxCount> mFuncServicePerms;
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
     * @returns RetWithError<StaticString<cSecretLen>>.
     */
    virtual RetWithError<StaticString<cSecretLen>> RegisterInstance(
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
     * Returns instance ident and permissions by secret and functional server ID.
     *
     * @param secret secret.
     * @param funcServerID functional server ID.
     * @param[out] instanceIdent result instance ident.
     * @param[out] servicePermissions result service permission.
     * @returns Error.
     */
    virtual Error GetPermissions(const String& secret, const String& funcServerID, InstanceIdent& instanceIdent,
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
     * @returns RetWithError<StaticString<cSecretLen>>.
     */
    RetWithError<StaticString<cSecretLen>> RegisterInstance(
        const InstanceIdent& instanceIdent, const Array<FunctionalServicePermissions>& instancePermissions) override;

    /**
     * Unregisters instance deletes service instance with permissions from cache.
     *
     * @param instanceIdent instance identification.
     * @returns Error.
     */
    Error UnregisterInstance(const InstanceIdent& instanceIdent) override;

    /**
     * Returns instance ident and permissions by secret and functional server ID.
     *
     * @param secret secret.
     * @param funcServerID functional server ID.
     * @param[out] instanceIdent result instance ident.
     * @param[out] servicePermissions result service permission.
     * @returns Error.
     */
    Error GetPermissions(const String& secret, const String& funcServerID, InstanceIdent& instanceIdent,
        Array<PermKeyValue>& servicePermissions) override;

private:
    Error                                  AddSecret(const String& secret, const InstanceIdent& instanceIdent,
                                         const Array<FunctionalServicePermissions>& instancePermissions);
    RetWithError<InstancePermissions*>     FindBySecret(const String& secret);
    RetWithError<InstancePermissions*>     FindByInstanceIdent(const InstanceIdent& instanceIdent);
    StaticString<cSecretLen>               GenerateSecret();
    RetWithError<StaticString<cSecretLen>> GetSecretForInstance(const InstanceIdent& instanceIdent);

    Mutex                                              mMutex;
    StaticArray<InstancePermissions, cMaxNumInstances> mInstancesPerms;
};

/** @}*/

} // namespace permhandler
} // namespace iam
} // namespace aos

#endif
