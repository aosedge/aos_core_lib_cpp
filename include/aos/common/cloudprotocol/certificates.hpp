/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_CERTIFICATES_HPP_
#define AOS_CLOUDPROTOCOL_CERTIFICATES_HPP_

#include "aos/common/cloudprotocol/cloudprotocol.hpp"
#include "aos/common/crypto/crypto.hpp"
#include "aos/common/tools/map.hpp"
#include "aos/common/tools/optional.hpp"
#include "aos/common/types.hpp"

namespace aos::cloudprotocol {

/**
 * Supported version of UnitSecret message.
 */
static constexpr auto cUnitSecretVersion = "2.0.0";

/**
 * Certificate installation description size.
 */
static constexpr auto cCertInstallationDescriptionSize
    = AOS_CONFIG_CLOUDPROTOCOL_CERTIFICATE_INSTALLATION_DESCRIPTION_SIZE;

/**
 * Certificate secret size.
 */
static constexpr auto cCertSecretSize = AOS_CONFIG_CLOUDPROTOCOL_CERTIFICATE_SECRET_SIZE;

/**
 * Cert type type.
 */
class CertTypeType {
public:
    enum class Enum {
        eOffline,
        eOnline,
        eUM,
        eSM,
        eCM,
        eIAM,
        eNumCertificates,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "offline",
            "online",
            "um",
            "sm",
            "cm",
            "iam",
            "unknown",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using CertTypeEnum = CertTypeType::Enum;
using CertType     = EnumStringer<CertTypeType>;

/**
 * Maximum number of certificates per node.
 */
static constexpr auto cCertificatesPerNodeCount = static_cast<size_t>(CertTypeEnum::eNumCertificates);

/**
 * Maximum number of certificates per unit.
 */
static constexpr auto cCertsPerUnitCount = cMaxNumNodes * cCertificatesPerNodeCount;

/**
 * IssuedCertData issued unit certificate data.
 */
struct IssuedCertData {
    CertType                            mType;
    StaticString<cNodeIDLen>            mNodeID;
    StaticString<crypto::cCertChainLen> mCertificateChain;

    /**
     * Compares issued certificate data.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const IssuedCertData& other) const
    {
        return mType == other.mType && mNodeID == other.mNodeID && mCertificateChain == other.mCertificateChain;
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
 * InstallCertData install certificate data.
 */
struct InstallCertData {
    CertType                                       mType;
    StaticString<cNodeIDLen>                       mNodeID;
    StaticString<crypto::cSerialNumStrLen>         mSerial;
    ItemStatus                                     mStatus {ItemStatusEnum::eInstalled};
    StaticString<cCertInstallationDescriptionSize> mDescription;

    /**
     * Compares install certificate data.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const InstallCertData& other) const
    {
        return mType == other.mType && mNodeID == other.mNodeID && mSerial == other.mSerial && mStatus == other.mStatus
            && mDescription == other.mDescription;
    }

    /**
     * Compares install certificate data.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const InstallCertData& other) const { return !operator==(other); }
};

/**
 * RenewCertData renew certificate data.
 */
struct RenewCertData {
    CertType                               mType;
    StaticString<cNodeIDLen>               mNodeID;
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
        return mType == other.mType && mNodeID == other.mNodeID && mSerial == other.mSerial
            && mValidTill == other.mValidTill;
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
 * UnitSecrets keeps secrets for nodes.
 */
struct UnitSecrets {
    StaticString<cVersionLen>                                                        mVersion;
    StaticMap<StaticString<cNodeIDLen>, StaticString<cCertSecretSize>, cMaxNumNodes> mNodes;

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
 * IssueCertData issue certificate data.
 */
struct IssueCertData {
    CertType                         mType;
    StaticString<cNodeIDLen>         mNodeID;
    StaticString<crypto::cCSRPEMLen> mCsr;

    /**
     * Compares issue certificate data.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const IssueCertData& other) const
    {
        return mType == other.mType && mNodeID == other.mNodeID && mCsr == other.mCsr;
    }

    /**
     * Compares issue certificate data.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const IssueCertData& other) const { return !operator==(other); }
};

/**
 * RenewCertsNotification renew certificate notification from cloud with pwd.
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
 * IssuedUnitCerts issued unit certificates info.
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
 * IssueUnitCerts issue unit certificates request.
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
 * InstallUnitCertsConfirmation install unit certificates confirmation.
 */
struct InstallUnitCertsConfirmation {
    StaticArray<InstallCertData, cCertsPerUnitCount> mCertificates;

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
