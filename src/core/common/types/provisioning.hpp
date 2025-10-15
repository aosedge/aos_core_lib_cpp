/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_PROVISIONING_HPP_
#define AOS_CORE_COMMON_TYPES_PROVISIONING_HPP_

#include "certificates.hpp"

namespace aos {

/**
 * CSR info.
 */
struct CSRInfo {
    CertType                         mType;
    StaticString<crypto::cCSRPEMLen> mCSR;

    /**
     * Compares CSR info.
     *
     * @param rhs CSR info to compare with.
     * @return bool.
     */
    bool operator==(const CSRInfo& rhs) const { return mType == rhs.mType && mCSR == rhs.mCSR; }

    /**
     * Compares CSR info.
     *
     * @param rhs CSR info to compare with.
     * @return bool.
     */
    bool operator!=(const CSRInfo& rhs) const { return !operator==(rhs); }
};

using CSRInfoArray = StaticArray<CSRInfo, cCertsPerNodeCount>;

/**
 * Start provisioning request.
 */
struct StartProvisioningRequest {
    StaticString<cIDLen>     mNodeID;
    StaticString<cSecretLen> mPassword;

    /**
     * Compares start provisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const StartProvisioningRequest& rhs) const
    {
        return mNodeID == rhs.mNodeID && mPassword == rhs.mPassword;
    }

    /**
     * Compares start provisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const StartProvisioningRequest& rhs) const { return !operator==(rhs); }
};

/**
 * Start provisioning response.
 */
struct StartProvisioningResponse {
    StaticString<cIDLen> mNodeID;
    CSRInfoArray         mCSRs;
    Error                mError;

    /**
     * Compares start provisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const StartProvisioningResponse& rhs) const
    {
        return mNodeID == rhs.mNodeID && mCSRs == rhs.mCSRs && mError == rhs.mError;
    }

    /**
     * Compares start provisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const StartProvisioningResponse& rhs) const { return !operator==(rhs); }
};

/**
 * Provisioning certificate data.
 */
struct ProvisioningCertData {
    CertType                               mCertType;
    StaticString<crypto::cCertChainPEMLen> mCertChain;

    /**
     * Compares provisioning certificate data.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const ProvisioningCertData& rhs) const
    {
        return mCertType == rhs.mCertType && mCertChain == rhs.mCertChain;
    }

    /**
     * Compares provisioning certificate data.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const ProvisioningCertData& rhs) const { return !operator==(rhs); }
};

using ProvisioningCertArray = StaticArray<ProvisioningCertData, cCertsPerNodeCount>;

/**
 * Finish provisioning request message.
 */
struct FinishProvisioningRequest {
    StaticString<cIDLen>     mNodeID;
    ProvisioningCertArray    mCertificates;
    StaticString<cSecretLen> mPassword;

    /**
     * Compares finish provisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const FinishProvisioningRequest& rhs) const
    {
        return mNodeID == rhs.mNodeID && mCertificates == rhs.mCertificates && mPassword == rhs.mPassword;
    }

    /**
     * Compares finish provisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const FinishProvisioningRequest& rhs) const { return !operator==(rhs); }
};

/**
 * Finish provisioning response message.
 */
struct FinishProvisioningResponse {
    StaticString<cIDLen> mNodeID;
    Error                mError;

    /**
     * Compares finish provisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const FinishProvisioningResponse& rhs) const
    {
        return mNodeID == rhs.mNodeID && mError == rhs.mError;
    }

    /**
     * Compares finish provisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const FinishProvisioningResponse& rhs) const { return !operator==(rhs); }
};

/**
 * Deprovisioning request message.
 */
struct DeprovisioningRequest {
    StaticString<cIDLen>     mNodeID;
    StaticString<cSecretLen> mPassword;

    /**
     * Compares deprovisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const DeprovisioningRequest& rhs) const
    {
        return mNodeID == rhs.mNodeID && mPassword == rhs.mPassword;
    }

    /**
     * Compares deprovisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const DeprovisioningRequest& rhs) const { return !operator==(rhs); }
};

/**
 * Deprovisioning response message.
 */
struct DeprovisioningResponse {
    StaticString<cIDLen> mNodeID;
    Error                mError;

    /**
     * Compares deprovisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator==(const DeprovisioningResponse& rhs) const { return mNodeID == rhs.mNodeID && mError == rhs.mError; }

    /**
     * Compares deprovisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    bool operator!=(const DeprovisioningResponse& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
