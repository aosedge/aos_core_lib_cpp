/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_PROVISIONING_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_PROVISIONING_HPP_

#include <core/common/types/certificates.hpp>
#include <core/common/types/obsolete.hpp>

namespace aos::cloudprotocol {

/**
 * StartProvisioningRequest message.
 */
struct StartProvisioningRequest {
    StaticString<cNodeIDLen>      mNodeID;
    StaticString<cCertSecretSize> mPassword;

    /**
     * Compares start provisioning request.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const StartProvisioningRequest& other) const
    {
        return mNodeID == other.mNodeID && mPassword == other.mPassword;
    }

    /**
     * Compares start provisioning request.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const StartProvisioningRequest& other) const { return !operator==(other); }
};

/**
 * StartProvisioningResponse message.
 */
struct StartProvisioningResponse {
    StaticString<cNodeIDLen>                       mNodeID;
    Error                                          mError;
    StaticArray<IssueCertData, cCertsPerUnitCount> mCSRs;

    /**
     * Compares start provisioning response.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const StartProvisioningResponse& other) const
    {
        return mNodeID == other.mNodeID && mError == other.mError && mCSRs == other.mCSRs;
    }

    /**
     * Compares start provisioning response.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const StartProvisioningResponse& other) const { return !operator==(other); }
};

/**
 * FinishProvisioningRequest message.
 */
struct FinishProvisioningRequest {
    StaticString<cNodeIDLen>                        mNodeID;
    StaticArray<IssuedCertData, cCertsPerUnitCount> mCertificates;
    StaticString<cCertSecretSize>                   mPassword;

    /**
     * Compares finish provisioning request.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const FinishProvisioningRequest& other) const
    {
        return mNodeID == other.mNodeID && mCertificates == other.mCertificates && mPassword == other.mPassword;
    }

    /**
     * Compares finish provisioning request.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const FinishProvisioningRequest& other) const { return !operator==(other); }
};

/**
 * FinishProvisioningResponse message.
 */
struct FinishProvisioningResponse {
    StaticString<cNodeIDLen> mNodeID;
    Error                    mError;

    /**
     * Compares finish provisioning response.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const FinishProvisioningResponse& other) const
    {
        return mNodeID == other.mNodeID && mError == other.mError;
    }

    /**
     * Compares finish provisioning response.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const FinishProvisioningResponse& other) const { return !operator==(other); }
};

/**
 * DeprovisioningRequest message.
 */
struct DeprovisioningRequest {
    StaticString<cNodeIDLen>      mNodeID;
    StaticString<cCertSecretSize> mPassword;

    /**
     * Compares deprovisioning request.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const DeprovisioningRequest& other) const
    {
        return mNodeID == other.mNodeID && mPassword == other.mPassword;
    }

    /**
     * Compares deprovisioning request.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const DeprovisioningRequest& other) const { return !operator==(other); }
};

/**
 * DeprovisioningRequest message.
 */
struct DeprovisioningResponse {
    StaticString<cNodeIDLen> mNodeID;
    Error                    mError;

    /**
     * Compares deprovisioning response.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const DeprovisioningResponse& other) const
    {
        return mNodeID == other.mNodeID && mError == other.mError;
    }

    /**
     * Compares deprovisioning response.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const DeprovisioningResponse& other) const { return !operator==(other); }
};

} // namespace aos::cloudprotocol

#endif
