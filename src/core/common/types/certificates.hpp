/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_CERTIFICATES_HPP_
#define AOS_CORE_COMMON_TYPES_CERTIFICATES_HPP_

#include <core/common/crypto/crypto.hpp>
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
    bool operator==(const CertIdent& rhs) const { return mType == rhs.mType && mNodeID == rhs.mNodeID; }

    /**
     * Compares certificate identification.
     *
     * @param rhs cert ident to compare with.
     * @return bool.
     */
    bool operator!=(const CertIdent& rhs) const { return !operator==(rhs); }
};

/**
 * Node secret.
 */
struct NodeSecret {
    StaticString<cIDLen>          mNodeID;
    StaticString<cCertSecretSize> mSecret;

    /**
     * Compares node secrets.
     *
     * @param rhs node secret to compare with.
     * @return bool.
     */
    bool operator==(const NodeSecret& rhs) const { return mNodeID == rhs.mNodeID && mSecret == rhs.mSecret; }

    /**
     * Compares node secrets.
     *
     * @param rhs node secret to compare with.
     * @return bool.
     */
    bool operator!=(const NodeSecret& rhs) const { return !operator==(rhs); }
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
    bool operator==(const UnitSecrets& rhs) const { return mVersion == rhs.mVersion && mNodes == rhs.mNodes; }

    /**
     * Compares unit secrets.
     *
     * @param rhs unit secrets to compare with.
     * @return bool.
     */
    bool operator!=(const UnitSecrets& rhs) const { return !operator==(rhs); }
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
    bool operator==(const IssuedCertData& rhs) const
    {
        return CertIdent::operator==(rhs) && mCertificateChain == rhs.mCertificateChain;
    }

    /**
     * Compares issued certificate data.
     *
     * @param rhs cert data to compare with.
     * @return bool.
     */
    bool operator!=(const IssuedCertData& rhs) const { return !operator==(rhs); }
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
    bool operator==(const InstallCertStatus& rhs) const
    {
        return CertIdent::operator==(rhs) && mSerial == rhs.mSerial && mError == rhs.mError;
    }

    /**
     * Compares install certificate status.
     *
     * @param rhs cert status to compare with.
     * @return bool.
     */
    bool operator!=(const InstallCertStatus& rhs) const { return !operator==(rhs); }
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

    bool operator==(const RenewCertData& rhs) const
    {
        return CertIdent::operator==(rhs) && mSerial == rhs.mSerial && mValidTill == rhs.mValidTill;
    }

    /**
     * Compares renew certificate data.
     *
     * @param rhs cert data to compare with.
     * @return bool.
     */
    bool operator!=(const RenewCertData& rhs) const { return !operator==(rhs); }
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
    bool operator==(const IssueCertData& rhs) const { return CertIdent::operator==(rhs) && mCSR == rhs.mCSR; }

    /**
     * Compares issue certificate data.
     *
     * @param rhs cert data to compare with.
     * @return bool.
     */
    bool operator!=(const IssueCertData& rhs) const { return !operator==(rhs); }
};

/**
 * Renew certificates notification.
 */
struct RenewCertsNotification {
    StaticArray<RenewCertData, cCertsPerUnitCount> mCertificates;
    UnitSecrets                                    mUnitSecrets;

    /**
     * Compares renew certificate notification.
     *
     * @param rhs cert notification to compare with.
     * @return bool.
     */
    bool operator==(const RenewCertsNotification& rhs) const
    {
        return mCertificates == rhs.mCertificates && mUnitSecrets == rhs.mUnitSecrets;
    }

    /**
     * Compares renew certificate notification.
     *
     * @param rhs cert notification to compare with.
     * @return bool.
     */
    bool operator!=(const RenewCertsNotification& rhs) const { return !operator==(rhs); }
};

/**
 * Issued unit certificates.
 */
struct IssuedUnitCerts {
    StaticArray<IssuedCertData, cCertsPerUnitCount> mCertificates;

    /**
     * Compares issued unit certificates.
     *
     * @param rhs unit certificates to compare with.
     * @return bool.
     */
    bool operator==(const IssuedUnitCerts& rhs) const { return mCertificates == rhs.mCertificates; }

    /**
     * Compares issued unit certificates.
     *
     * @param rhs unit certificates to compare with.
     * @return bool.
     */
    bool operator!=(const IssuedUnitCerts& rhs) const { return !operator==(rhs); }
};

/**
 * Issue unit certificates.
 */
struct IssueUnitCerts {
    StaticArray<IssueCertData, cCertsPerUnitCount> mRequests;

    /**
     * Compares issue unit certificates.
     *
     * @param rhs unit certificates to compare with.
     * @return bool.
     */
    bool operator==(const IssueUnitCerts& rhs) const { return mRequests == rhs.mRequests; }

    /**
     * Compares issue unit certificates.
     *
     * @param rhs unit certificates to compare with.
     * @return bool.
     */
    bool operator!=(const IssueUnitCerts& rhs) const { return !operator==(rhs); }
};

/**
 * Install unit certificates confirmation.
 */
struct InstallUnitCertsConfirmation {
    StaticArray<InstallCertStatus, cCertsPerUnitCount> mCertificates;

    /**
     * Compares install unit certificates confirmation.
     *
     * @param rhs certificates confirmation to compare with.
     * @return bool.
     */
    bool operator==(const InstallUnitCertsConfirmation& rhs) const { return mCertificates == rhs.mCertificates; }

    /**
     * Compares install unit certificates confirmation.
     *
     * @param rhs certificates confirmation to compare with.
     * @return bool.
     */
    bool operator!=(const InstallUnitCertsConfirmation& rhs) const { return !operator==(rhs); }
};

} // namespace aos

#endif
