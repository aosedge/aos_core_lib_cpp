/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_PROVISIONING_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_PROVISIONING_HPP_

#include <core/common/types/types.hpp>

#include "common.hpp"

namespace aos::cloudprotocol {

/**
 * StartProvisioningRequest message.
 */
struct StartProvisioningRequest {
    Identity                      mNodeID;
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
 * CSR info.
 */
struct CSRInfo {
    CertType                         mType;
    StaticString<crypto::cCSRPEMLen> mCsr;

    /**
     * Compares CSR info.
     *
     * @param other CSR info to compare with.
     * @return bool.
     */
    bool operator==(const CSRInfo& other) const { return mType == other.mType && mCsr == other.mCsr; }

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
 * StartProvisioningResponse message.
 */
struct StartProvisioningResponse {
    Identity     mNodeID;
    Error        mError;
    CSRInfoArray mCSRs;

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
 * Certificate info.
 */
struct CertInfo {
    CertType                               mType;
    StaticString<crypto::cCertChainPEMLen> mCertificateChain;

    /**
     * Compares certificate info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const CertInfo& other) const
    {
        return mType == other.mType && mCertificateChain == other.mCertificateChain;
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
 * FinishProvisioningRequest message.
 */
struct FinishProvisioningRequest {
    Identity                      mNodeID;
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
    Identity mNodeID;
    Error    mError;

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
    Identity                      mNodeID;
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
    Identity mNodeID;
    Error    mError;

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
