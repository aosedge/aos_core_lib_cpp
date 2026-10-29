/*
 * Copyright (C) 2026 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_IAM_IDENTHANDLER_IDENTHANDLER_HPP_
#define AOS_CORE_IAM_IDENTHANDLER_IDENTHANDLER_HPP_

#include <core/common/crypto/itf/certloader.hpp>
#include <core/common/iamclient/itf/certprovider.hpp>
#include <core/common/iamclient/itf/identprovider.hpp>
#include <core/common/tools/memory.hpp>
#include <core/common/tools/thread.hpp>
#include <core/iam/identhandler/itf/identmodule.hpp>

namespace aos::iam::identhandler {

/** @addtogroup iam Identification and Access Manager
 *  @{
 */

/**
 * Ident handler.
 *
 * System ID is always taken from the online certificate SAN URI (urn:aos:unit:<system-id>).
 * Unit model, version and subjects are provided by the configured ident module.
 */
class IdentHandler : public iamclient::IdentProviderItf, private iamclient::CertListenerItf {
public:
    /**
     * Initializes ident handler.
     *
     * @param identModule ident module providing unit model, version and subjects.
     * @param certProvider certificate provider.
     * @param certLoader certificate loader.
     * @param allocator allocator.
     * @param provisioningMode provisioning mode.
     * @return Error.
     */
    Error Init(IdentModuleItf& identModule, iamclient::CertProviderItf& certProvider, crypto::CertLoaderItf& certLoader,
        AllocatorItf& allocator, bool provisioningMode);

    /**
     * Starts ident handler.
     *
     * @return Error.
     */
    Error Start();

    /**
     * Stops ident handler.
     *
     * @return Error.
     */
    Error Stop();

    /**
     * Returns System info.
     *
     * @param[out] info result system info.
     * @returns Error.
     */
    Error GetSystemInfo(SystemInfo& info) override;

    /**
     * Returns subjects.
     *
     * @param[out] subjects result subjects.
     * @returns Error.
     */
    Error GetSubjects(Array<StaticString<cIDLen>>& subjects) override;

    /**
     * Subscribes subjects listener.
     *
     * @param subjectsListener subjects listener.
     * @returns Error.
     */
    Error SubscribeListener(iamclient::SubjectsListenerItf& subjectsListener) override;

    /**
     * Unsubscribes subjects listener.
     *
     * @param subjectsListener subjects listener.
     * @returns Error.
     */
    Error UnsubscribeListener(iamclient::SubjectsListenerItf& subjectsListener) override;

private:
    static constexpr auto cOnlineCert = "online";

    void  OnCertChanged(const CertInfo& info) override;
    Error GetSystemIDFromOnlineCert(String& systemID);
    Error GetSystemIDFromCertURL(const String& certURL, String& systemID);

    Mutex                       mMutex;
    StaticString<cIDLen>        mSystemID;
    IdentModuleItf*             mIdentModule {};
    iamclient::CertProviderItf* mCertProvider {};
    crypto::CertLoaderItf*      mCertLoader {};
    AllocatorItf*               mAllocator {};
    bool                        mProvisioningMode {};
    bool                        mSubscribed {};
};

/** @}*/

} // namespace aos::iam::identhandler

#endif
