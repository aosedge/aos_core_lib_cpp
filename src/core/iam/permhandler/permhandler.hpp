/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_PERMHANDLER_PERMHANDLER_HPP_
#define AOS_CORE_IAM_PERMHANDLER_PERMHANDLER_HPP_

#include <core/common/crypto/itf/uuid.hpp>
#include <core/common/iamclient/itf/permhandler.hpp>
#include <core/common/iamclient/itf/permprovider.hpp>
#include <core/common/tools/thread.hpp>
#include <core/common/tools/utils.hpp>

#include <core/iam/config.hpp>

namespace aos::iam::permhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Permission handler interface.
 */
class PermHandlerItf : public iamclient::PermHandlerItf, public iamclient::PermProviderItf { };

/**
 * Permission handler implements PermHandlerItf.
 */
class PermHandler : public PermHandlerItf {
public:
    /**
     * Initializes permission handler.
     *
     * @param uuidProvider UUID provider.
     * @returns Error.
     */
    Error Init(crypto::UUIDItf& uuidProvider);

    /**
     * Adds new service instance and its permissions into cache.
     *
     * @param instanceIdent instance identification.
     * @param instancePermissions instance permissions.
     * @returns RetWithError<StaticString<cSecretLen>>.
     */
    RetWithError<StaticString<cSecretLen>> RegisterInstance(
        const InstanceIdent& instanceIdent, const Array<FunctionServicePermissions>& instancePermissions) override;

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
        Array<FunctionPermissions>& servicePermissions) override;

private:
    Error                                  AddSecret(const String& secret, const InstanceIdent& instanceIdent,
                                         const Array<FunctionServicePermissions>& instancePermissions);
    InstancePermissions*                   FindBySecret(const String& secret);
    InstancePermissions*                   FindByInstanceIdent(const InstanceIdent& instanceIdent);
    RetWithError<StaticString<cSecretLen>> GenerateSecret();
    RetWithError<StaticString<cSecretLen>> GetSecretForInstance(const InstanceIdent& instanceIdent);

    Mutex                                              mMutex;
    StaticArray<InstancePermissions, cMaxNumInstances> mInstancesPerms;
    crypto::UUIDItf*                                   mUUIDProvider = {};
};

/** @}*/

} // namespace aos::iam::permhandler

#endif
