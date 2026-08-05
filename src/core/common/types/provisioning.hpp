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
    friend bool operator==(const CSRInfo& lhs, const CSRInfo& rhs)
    {
        return lhs.mType == rhs.mType && lhs.mCSR == rhs.mCSR;
    };

    /**
     * Compares CSR info.
     *
     * @param rhs CSR info to compare with.
     * @return bool.
     */
    friend bool operator!=(const CSRInfo& lhs, const CSRInfo& rhs) { return !(lhs == rhs); };
};

using CSRInfoArray = StaticArray<CSRInfo, cCertsPerNodeCount>;

/**
 * Start provisioning request.
 */
struct StartProvisioningRequest : public Protocol {
    StaticString<cIDLen>     mNodeID;
    StaticString<cSecretLen> mPassword;

    /**
     * Compares start provisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator==(const StartProvisioningRequest& lhs, const StartProvisioningRequest& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mNodeID == rhs.mNodeID
            && lhs.mPassword == rhs.mPassword;
    };

    /**
     * Compares start provisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const StartProvisioningRequest& lhs, const StartProvisioningRequest& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * Start provisioning response.
 */
struct StartProvisioningResponse : public Protocol {
    StaticString<cIDLen> mNodeID;
    CSRInfoArray         mCSRs;
    Error                mError;

    /**
     * Compares start provisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator==(const StartProvisioningResponse& lhs, const StartProvisioningResponse& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mNodeID == rhs.mNodeID && lhs.mCSRs == rhs.mCSRs
            && lhs.mError == rhs.mError;
    };

    /**
     * Compares start provisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const StartProvisioningResponse& lhs, const StartProvisioningResponse& rhs)
    {
        return !(lhs == rhs);
    };
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
    friend bool operator==(const ProvisioningCertData& lhs, const ProvisioningCertData& rhs)
    {
        return lhs.mCertType == rhs.mCertType && lhs.mCertChain == rhs.mCertChain;
    };

    /**
     * Compares provisioning certificate data.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const ProvisioningCertData& lhs, const ProvisioningCertData& rhs) { return !(lhs == rhs); };
};

using ProvisioningCertArray = StaticArray<ProvisioningCertData, cCertsPerNodeCount>;

/**
 * Finish provisioning request message.
 */
struct FinishProvisioningRequest : public Protocol {
    StaticString<cIDLen>     mNodeID;
    ProvisioningCertArray    mCertificates;
    StaticString<cSecretLen> mPassword;

    /**
     * Compares finish provisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator==(const FinishProvisioningRequest& lhs, const FinishProvisioningRequest& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mNodeID == rhs.mNodeID
            && lhs.mCertificates == rhs.mCertificates && lhs.mPassword == rhs.mPassword;
    };

    /**
     * Compares finish provisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const FinishProvisioningRequest& lhs, const FinishProvisioningRequest& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * Finish provisioning response message.
 */
struct FinishProvisioningResponse : public Protocol {
    StaticString<cIDLen> mNodeID;
    Error                mError;

    /**
     * Compares finish provisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator==(const FinishProvisioningResponse& lhs, const FinishProvisioningResponse& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mNodeID == rhs.mNodeID && lhs.mError == rhs.mError;
    };

    /**
     * Compares finish provisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const FinishProvisioningResponse& lhs, const FinishProvisioningResponse& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * Deprovisioning request message.
 */
struct DeprovisioningRequest : public Protocol {
    StaticString<cIDLen>     mNodeID;
    StaticString<cSecretLen> mPassword;

    /**
     * Compares deprovisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator==(const DeprovisioningRequest& lhs, const DeprovisioningRequest& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mNodeID == rhs.mNodeID
            && lhs.mPassword == rhs.mPassword;
    };

    /**
     * Compares deprovisioning request.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const DeprovisioningRequest& lhs, const DeprovisioningRequest& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * Deprovisioning response message.
 */
struct DeprovisioningResponse : public Protocol {
    StaticString<cIDLen> mNodeID;
    Error                mError;

    /**
     * Compares deprovisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator==(const DeprovisioningResponse& lhs, const DeprovisioningResponse& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mNodeID == rhs.mNodeID && lhs.mError == rhs.mError;
    };

    /**
     * Compares deprovisioning response.
     *
     * @param rhs object to compare with.
     * @return bool.
     */
    friend bool operator!=(const DeprovisioningResponse& lhs, const DeprovisioningResponse& rhs)
    {
        return !(lhs == rhs);
    };
};

} // namespace aos

#endif
