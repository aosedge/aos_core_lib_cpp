/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_CLOUDPROTOCOL_PROVISIONING_HPP_
#define AOS_CORE_CM_CLOUDPROTOCOL_PROVISIONING_HPP_

#include <core/common/types/types.hpp>

#include "common.hpp"

namespace aos::cloudprotocol {

/**
 * CSR info.
 */
struct CSRInfo {
    CertType                         mType;
    StaticString<crypto::cCSRPEMLen> mCSR;

    /**
     * Compares CSR info.
     *
     * @param other CSR info to compare with.
     * @return bool.
     */
    bool operator==(const CSRInfo& other) const { return mType == other.mType && mCSR == other.mCSR; }

    /**
     * Compares CSR info.
     *
     * @param other CSR info to compare with.
     * @return bool.
     */
    bool operator!=(const CSRInfo& other) const { return !operator==(other); }
};

using CSRInfoArray = StaticArray<CSRInfo, cCertsPerNodeCount>;

/**
 * Certificate info.
 */
struct CertInfo {
    CertType                               mCertType;
    StaticString<crypto::cCertChainPEMLen> mCertChain;

    /**
     * Compares certificate info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const CertInfo& other) const
    {
        return mCertType == other.mCertType && mCertChain == other.mCertChain;
    }

    /**
     * Compares certificate info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const CertInfo& other) const { return !operator==(other); }
};

using CertInfoArray = StaticArray<CertInfo, cCertsPerNodeCount>;

/**
 * Start provisioning request.
 */
struct StartProvisioningRequest {
    Identity                      mNode;
    StaticString<cCertSecretSize> mPassword;

    /**
     * Compares start provisioning request.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const StartProvisioningRequest& other) const
    {
        return mNode == other.mNode && mPassword == other.mPassword;
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
 * Start provisioning response.
 */
struct StartProvisioningResponse {
    Identity     mNode;
    CSRInfoArray mCSRs;
    Error        mError;

    /**
     * Compares start provisioning response.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const StartProvisioningResponse& other) const
    {
        return mNode == other.mNode && mCSRs == other.mCSRs && mError == other.mError;
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
    Identity                      mNode;
    CertInfoArray                 mCertificates;
    StaticString<cCertSecretSize> mPassword;

    /**
     * Compares finish provisioning request.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const FinishProvisioningRequest& other) const
    {
        return mNode == other.mNode && mCertificates == other.mCertificates && mPassword == other.mPassword;
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
    Identity mNode;
    Error    mError;

    /**
     * Compares finish provisioning response.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const FinishProvisioningResponse& other) const
    {
        return mNode == other.mNode && mError == other.mError;
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
    Identity                      mNode;
    StaticString<cCertSecretSize> mPassword;

    /**
     * Compares deprovisioning request.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const DeprovisioningRequest& other) const
    {
        return mNode == other.mNode && mPassword == other.mPassword;
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
    Identity mNode;
    Error    mError;

    /**
     * Compares deprovisioning response.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const DeprovisioningResponse& other) const
    {
        return mNode == other.mNode && mError == other.mError;
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
