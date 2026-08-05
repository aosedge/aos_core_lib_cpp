/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_CERTIFICATES_HPP_
#define AOS_CORE_COMMON_TYPES_CERTIFICATES_HPP_

#include <core/common/crypto/itf/x509.hpp>
#include <core/common/tools/optional.hpp>

#include "common.hpp"

namespace aos {

/**
 * Supported version of UnitSecret message.
 */
static constexpr auto cUnitSecretVersion = "2.0.0";

/**
 * Maximum number of certificates per node.
 */
static constexpr auto cCertsPerNodeCount = static_cast<size_t>(CertTypeEnum::eNumCertificates);

/**
 * Maximum number of certificates per unit.
 */
static constexpr auto cCertsPerUnitCount = cMaxNumNodes * cCertsPerNodeCount;

/**
 * Certificate identification.
 */
struct CertIdent {
    CertType             mType;
    StaticString<cIDLen> mNodeID;

    /**
     * Compares certificate identification.
     *
     * @param rhs cert ident to compare with.
     * @return bool.
     */
    friend bool operator==(const CertIdent& lhs, const CertIdent& rhs)
    {
        return lhs.mType == rhs.mType && lhs.mNodeID == rhs.mNodeID;
    };

    /**
     * Compares certificate identification.
     *
     * @param rhs cert ident to compare with.
     * @return bool.
     */
    friend bool operator!=(const CertIdent& lhs, const CertIdent& rhs) { return !(lhs == rhs); };
};

/**
 * Node secret.
 */
struct NodeSecret {
    StaticString<cIDLen>     mNodeID;
    StaticString<cSecretLen> mSecret;

    /**
     * Compares node secrets.
     *
     * @param rhs node secret to compare with.
     * @return bool.
     */
    friend bool operator==(const NodeSecret& lhs, const NodeSecret& rhs)
    {
        return lhs.mNodeID == rhs.mNodeID && lhs.mSecret == rhs.mSecret;
    };

    /**
     * Compares node secrets.
     *
     * @param rhs node secret to compare with.
     * @return bool.
     */
    friend bool operator!=(const NodeSecret& lhs, const NodeSecret& rhs) { return !(lhs == rhs); };
};

using NodeSecretArray = StaticArray<NodeSecret, cMaxNumNodes>;

/**
 * Unit secrets.
 */
struct UnitSecrets {
    StaticString<cVersionLen> mVersion;
    NodeSecretArray           mNodes;

    /**
     * Compares unit secrets.
     *
     * @param rhs unit secrets to compare with.
     * @return bool.
     */
    friend bool operator==(const UnitSecrets& lhs, const UnitSecrets& rhs)
    {
        return lhs.mVersion == rhs.mVersion && lhs.mNodes == rhs.mNodes;
    };

    /**
     * Compares unit secrets.
     *
     * @param rhs unit secrets to compare with.
     * @return bool.
     */
    friend bool operator!=(const UnitSecrets& lhs, const UnitSecrets& rhs) { return !(lhs == rhs); };
};

/**
 * Issued certificate data.
 */
struct IssuedCertData : public CertIdent {
    StaticString<crypto::cCertChainPEMLen> mCertificateChain;

    /**
     * Compares issued certificate data.
     *
     * @param rhs cert data to compare with.
     * @return bool.
     */
    friend bool operator==(const IssuedCertData& lhs, const IssuedCertData& rhs)
    {
        return (static_cast<const CertIdent&>(lhs) == rhs) && lhs.mCertificateChain == rhs.mCertificateChain;
    };

    /**
     * Compares issued certificate data.
     *
     * @param rhs cert data to compare with.
     * @return bool.
     */
    friend bool operator!=(const IssuedCertData& lhs, const IssuedCertData& rhs) { return !(lhs == rhs); };
};

/**
 * Install certificate status.
 */
struct InstallCertStatus : public CertIdent {
    StaticString<crypto::cSerialNumStrLen> mSerial;
    Error                                  mError;

    /**
     * Compares install certificate status.
     *
     * @param rhs cert status to compare with.
     * @return bool.
     */
    friend bool operator==(const InstallCertStatus& lhs, const InstallCertStatus& rhs)
    {
        return (static_cast<const CertIdent&>(lhs) == rhs) && lhs.mSerial == rhs.mSerial && lhs.mError == rhs.mError;
    };

    /**
     * Compares install certificate status.
     *
     * @param rhs cert status to compare with.
     * @return bool.
     */
    friend bool operator!=(const InstallCertStatus& lhs, const InstallCertStatus& rhs) { return !(lhs == rhs); };
};

/**
 * Renew certificate data.
 */
struct RenewCertData : public CertIdent {
    StaticString<crypto::cSerialNumStrLen> mSerial;
    Optional<Time>                         mValidTill;

    /**
     * Compares renew certificate data.
     *
     * @param rhs cert data to compare with.
     * @return bool.
     */

    friend bool operator==(const RenewCertData& lhs, const RenewCertData& rhs)
    {
        return (static_cast<const CertIdent&>(lhs) == rhs) && lhs.mSerial == rhs.mSerial
            && lhs.mValidTill == rhs.mValidTill;
    };

    /**
     * Compares renew certificate data.
     *
     * @param rhs cert data to compare with.
     * @return bool.
     */
    friend bool operator!=(const RenewCertData& lhs, const RenewCertData& rhs) { return !(lhs == rhs); };
};

/**
 * Issue certificate data.
 */
struct IssueCertData : public CertIdent {
    StaticString<crypto::cCSRPEMLen> mCSR;

    /**
     * Compares issue certificate data.
     *
     * @param rhs cert data to compare with.
     * @return bool.
     */
    friend bool operator==(const IssueCertData& lhs, const IssueCertData& rhs)
    {
        return (static_cast<const CertIdent&>(lhs) == rhs) && lhs.mCSR == rhs.mCSR;
    };

    /**
     * Compares issue certificate data.
     *
     * @param rhs cert data to compare with.
     * @return bool.
     */
    friend bool operator!=(const IssueCertData& lhs, const IssueCertData& rhs) { return !(lhs == rhs); };
};

/**
 * Renew certificates notification.
 */
struct RenewCertsNotification : public Protocol {
    StaticArray<RenewCertData, cCertsPerUnitCount> mCertificates;
    UnitSecrets                                    mUnitSecrets;

    /**
     * Compares renew certificate notification.
     *
     * @param rhs cert notification to compare with.
     * @return bool.
     */
    friend bool operator==(const RenewCertsNotification& lhs, const RenewCertsNotification& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mCertificates == rhs.mCertificates
            && lhs.mUnitSecrets == rhs.mUnitSecrets;
    };

    /**
     * Compares renew certificate notification.
     *
     * @param rhs cert notification to compare with.
     * @return bool.
     */
    friend bool operator!=(const RenewCertsNotification& lhs, const RenewCertsNotification& rhs)
    {
        return !(lhs == rhs);
    };
};

/**
 * Issued unit certificates.
 */
struct IssuedUnitCerts : public Protocol {
    StaticArray<IssuedCertData, cCertsPerUnitCount> mCertificates;

    /**
     * Compares issued unit certificates.
     *
     * @param rhs unit certificates to compare with.
     * @return bool.
     */
    friend bool operator==(const IssuedUnitCerts& lhs, const IssuedUnitCerts& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mCertificates == rhs.mCertificates;
    };

    /**
     * Compares issued unit certificates.
     *
     * @param rhs unit certificates to compare with.
     * @return bool.
     */
    friend bool operator!=(const IssuedUnitCerts& lhs, const IssuedUnitCerts& rhs) { return !(lhs == rhs); };
};

/**
 * Issue unit certificates.
 */
struct IssueUnitCerts : public Protocol {
    StaticArray<IssueCertData, cCertsPerUnitCount> mRequests;

    /**
     * Compares issue unit certificates.
     *
     * @param rhs unit certificates to compare with.
     * @return bool.
     */
    friend bool operator==(const IssueUnitCerts& lhs, const IssueUnitCerts& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mRequests == rhs.mRequests;
    };

    /**
     * Compares issue unit certificates.
     *
     * @param rhs unit certificates to compare with.
     * @return bool.
     */
    friend bool operator!=(const IssueUnitCerts& lhs, const IssueUnitCerts& rhs) { return !(lhs == rhs); };
};

/**
 * Install unit certificates confirmation.
 */
struct InstallUnitCertsConfirmation : public Protocol {
    StaticArray<InstallCertStatus, cCertsPerUnitCount> mCertificates;

    /**
     * Compares install unit certificates confirmation.
     *
     * @param rhs certificates confirmation to compare with.
     * @return bool.
     */
    friend bool operator==(const InstallUnitCertsConfirmation& lhs, const InstallUnitCertsConfirmation& rhs)
    {
        return (static_cast<const Protocol&>(lhs) == rhs) && lhs.mCertificates == rhs.mCertificates;
    };

    /**
     * Compares install unit certificates confirmation.
     *
     * @param rhs certificates confirmation to compare with.
     * @return bool.
     */
    friend bool operator!=(const InstallUnitCertsConfirmation& lhs, const InstallUnitCertsConfirmation& rhs)
    {
        return !(lhs == rhs);
    };
};

} // namespace aos

#endif
