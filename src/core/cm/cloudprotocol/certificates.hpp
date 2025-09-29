/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_CM_CLOUDPROTOCOL_CERTIFICATES_HPP_
#define AOS_CORE_CM_CLOUDPROTOCOL_CERTIFICATES_HPP_

#include <core/common/crypto/crypto.hpp>
#include <core/common/tools/map.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/types/types.hpp>

#include "common.hpp"

namespace aos::cloudprotocol {

/**
 * Supported version of UnitSecret message.
 */
static constexpr auto cUnitSecretVersion = "2.0.0";

/**
 * Certificate secret size.
 */
static constexpr auto cCertSecretSize = AOS_CONFIG_CLOUDPROTOCOL_CERT_SECRET_SIZE;

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
    CertType mType;
    Identity mNode;

    /**
     * Compares certificate identification.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const CertIdent& other) const { return mType == other.mType && mNode == other.mNode; }

    /**
     * Compares certificate identification.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const CertIdent& other) const { return !operator==(other); }
};

/**
 * Node secret.
 */
struct NodeSecret {
    Identity                      mNode;
    StaticString<cCertSecretSize> mSecret;

    /**
     * Compares node secrets.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const NodeSecret& other) const { return mNode == other.mNode && mSecret == other.mSecret; }

    /**
     * Compares node secrets.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const NodeSecret& other) const { return !operator==(other); }
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
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const UnitSecrets& other) const { return mVersion == other.mVersion && mNodes == other.mNodes; }

    /**
     * Compares unit secrets.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const UnitSecrets& other) const { return !operator==(other); }
};

/**
 * Issued certificate data.
 */
struct IssuedCertData : public CertIdent {
    StaticString<crypto::cCertChainPEMLen> mCertificateChain;

    /**
     * Compares issued certificate data.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const IssuedCertData& other) const
    {
        return CertIdent::operator==(other) && mCertificateChain == other.mCertificateChain;
    }

    /**
     * Compares issued certificate data.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const IssuedCertData& other) const { return !operator==(other); }
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
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const InstallCertStatus& other) const
    {
        return CertIdent::operator==(other) && mSerial == other.mSerial && mError == other.mError;
    }

    /**
     * Compares install certificate status.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const InstallCertStatus& other) const { return !operator==(other); }
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
     * @param other object to compare with.
     * @return bool.
     */

    bool operator==(const RenewCertData& other) const
    {
        return CertIdent::operator==(other) && mSerial == other.mSerial && mValidTill == other.mValidTill;
    }

    /**
     * Compares renew certificate data.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const RenewCertData& other) const { return !operator==(other); }
};

/**
 * Issue certificate data.
 */
struct IssueCertData : public CertIdent {
    StaticString<crypto::cCSRPEMLen> mCSR;

    /**
     * Compares issue certificate data.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const IssueCertData& other) const { return CertIdent::operator==(other) && mCSR == other.mCSR; }

    /**
     * Compares issue certificate data.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const IssueCertData& other) const { return !operator==(other); }
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
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const RenewCertsNotification& other) const
    {
        return mCertificates == other.mCertificates && mUnitSecrets == other.mUnitSecrets;
    }

    /**
     * Compares renew certificate notification.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const RenewCertsNotification& other) const { return !operator==(other); }
};

/**
 * Issued unit certificates.
 */
struct IssuedUnitCerts {
    StaticArray<IssuedCertData, cCertsPerUnitCount> mCertificates;

    /**
     * Compares issued unit certificates.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const IssuedUnitCerts& other) const { return mCertificates == other.mCertificates; }

    /**
     * Compares issued unit certificates.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const IssuedUnitCerts& other) const { return !operator==(other); }
};

/**
 * Issue unit certificates.
 */
struct IssueUnitCerts {
    StaticArray<IssueCertData, cCertsPerUnitCount> mRequests;

    /**
     * Compares issue unit certificates.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const IssueUnitCerts& other) const { return mRequests == other.mRequests; }

    /**
     * Compares issue unit certificates.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const IssueUnitCerts& other) const { return !operator==(other); }
};

/**
 * Install unit certificates confirmation.
 */
struct InstallUnitCertsConfirmation {
    StaticArray<InstallCertStatus, cCertsPerUnitCount> mCertificates;

    /**
     * Compares install unit certificates confirmation.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const InstallUnitCertsConfirmation& other) const { return mCertificates == other.mCertificates; }

    /**
     * Compares install unit certificates confirmation.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const InstallUnitCertsConfirmation& other) const { return !operator==(other); }
};

} // namespace aos::cloudprotocol

#endif
