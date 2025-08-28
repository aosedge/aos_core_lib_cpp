/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_DESIREDSTATUS_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_DESIREDSTATUS_HPP_

#include <core/common/crypto/crypto.hpp>
#include <core/common/types/types.hpp>

#include "common.hpp"

namespace aos::cloudprotocol {

/**
 * Max number of update images per update item.
 */
constexpr auto cMaxNumUpdateImages = AOS_CONFIG_CLOUDPROTOCOL_MAX_NUM_UPDATE_IMAGES;

/**
 * Algorithm len.
 */
constexpr auto cAlgLen = AOS_CONFIG_CLOUDPROTOCOL_ALG_LEN;

/**
 * IV size.
 */
constexpr auto cIVSize = AOS_CONFIG_CLOUDPROTOCOL_IV_SIZE;

/**
 * Key size.
 */
constexpr auto cKeySize = AOS_CONFIG_CLOUDPROTOCOL_KEY_SIZE;

/**
 * OCSP value len.
 */
constexpr auto cOCSPValueLen = AOS_CONFIG_CLOUDPROTOCOL_OCSP_VALUE_LEN;

/**
 * OCSP values count.
 */
constexpr auto cOCSPValuesCount = AOS_CONFIG_CLOUDPROTOCOL_OCSP_VALUES_COUNT;

/**
 * Certificate fingerprint len.
 */
constexpr auto cCertFingerprintLen = AOS_CONFIG_CLOUDPROTOCOL_CERT_FINGERPRINT_LEN;

/**
 * Max number of certificates.
 */
constexpr auto cMaxNumCertificates = AOS_CONFIG_CLOUDPROTOCOL_MAX_NUM_CERTIFICATES;

/**
 * Node state.
 */
class NodeStateType {
public:
    enum class Enum {
        eProvisioned,
        ePaused,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sNodeStateStrings[] = {
            "provisioned",
            "paused",
        };

        return Array<const char* const>(sNodeStateStrings, ArraySize(sNodeStateStrings));
    };
};

using NodeStateEnum = NodeStateType::Enum;
using NodeState     = EnumStringer<NodeStateType>;

/**
 * Desired node state.
 */
struct DesiredNodeState {
    Identifier mIdentifier;
    NodeState  mState;

    /**
     * Compares desired nodes states.
     *
     * @param state nodes state to compare.
     * @return bool.
     */
    bool operator==(const DesiredNodeState& state) const
    {
        return mIdentifier == state.mIdentifier && mState == state.mState;
    }

    /**
     * Compares desired nodes states.
     *
     * @param state desired nodes state to compare.
     * @return bool.
     */
    bool operator!=(const DesiredNodeState& state) const { return !operator==(state); }
};

/**
 * Resource ratios.
 */
struct ResourceRatios {
    Optional<double> mCPU;
    Optional<double> mRAM;
    Optional<double> mStorage;
    Optional<double> mState;

    /**
     * Compares resource ratios.
     *
     * @param ratios resource ratios to compare.
     * @return bool.
     */
    bool operator==(const ResourceRatios& ratios) const
    {
        return mCPU == ratios.mCPU && mRAM == ratios.mRAM && mStorage == ratios.mStorage && mState == ratios.mState;
    }

    /**
     * Compares resource ratios.
     *
     * @param ratios resource ratios to compare.
     * @return bool.
     */
    bool operator!=(const ResourceRatios& ratios) const { return !operator==(ratios); }
};

/**
 * Node config.
 */
struct NodeConfig {
    Optional<Identifier>                                        mNode;
    Identifier                                                  mNodeGroupSubject;
    Optional<AlertRules>                                        mAlertRules;
    Optional<ResourceRatios>                                    mResourceRatios;
    StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels> mLabels;
    uint64_t                                                    mPriority {0};

    /**
     * Compares node configs.
     *
     * @param nodeConfig node config to compare.
     * @return bool.
     */
    bool operator==(const NodeConfig& nodeConfig) const
    {
        return mNode == nodeConfig.mNode && mNodeGroupSubject == nodeConfig.mNodeGroupSubject
            && mAlertRules == nodeConfig.mAlertRules && mResourceRatios == nodeConfig.mResourceRatios
            && mLabels == nodeConfig.mLabels && mPriority == nodeConfig.mPriority;
    }

    /**
     * Compares node configs.
     *
     * @param nodeConfig node config to compare.
     * @return bool.
     */
    bool operator!=(const NodeConfig& nodeConfig) const { return !operator==(nodeConfig); }
};

/**
 * Unit config.
 */
struct UnitConfig {
    StaticString<cVersionLen>             mVersion;
    StaticString<cVersionLen>             mFormatVersion;
    StaticArray<NodeConfig, cMaxNumNodes> mNodes;

    /**
     * Compares unit config.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const UnitConfig& other) const
    {
        return mVersion == other.mVersion && mFormatVersion == other.mFormatVersion && mNodes == other.mNodes;
    }

    /**
     * Compares unit config.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const UnitConfig& other) const { return !operator==(other); }
};

/**
 * Decryption info.
 */
struct DecryptInfo {
    StaticString<cAlgLen>          mBlockAlg;
    StaticArray<uint8_t, cIVSize>  mBlockIV;
    StaticArray<uint8_t, cKeySize> mBlockKey;

    /**
     * Compares decryption info.
     *
     * @param other decryption info to compare with.
     * @return bool.
     */
    bool operator==(const DecryptInfo& other) const
    {
        return mBlockAlg == other.mBlockAlg && mBlockIV == other.mBlockIV && mBlockKey == other.mBlockKey;
    }

    /**
     * Compares decryption info.
     *
     * @param other decryption info to compare with.
     * @return bool.
     */
    bool operator!=(const DecryptInfo& other) const { return !operator==(other); }
};

/**
 * Sign info.
 */
struct SignInfo {
    StaticString<cChainNameLen>                                mChainName;
    StaticString<cAlgLen>                                      mAlg;
    StaticArray<uint8_t, crypto::cSignatureSize>               mValue;
    StaticString<cTimeStrLen>                                  mTrustedTimestamp;
    StaticArray<StaticString<cOCSPValueLen>, cOCSPValuesCount> mOCSPValues;
    /**
     * Compares sign info.
     *
     * @param other sign info to compare with.
     * @return bool.
     */
    bool operator==(const SignInfo& other) const
    {
        return mChainName == other.mChainName && mAlg == other.mAlg && mValue == other.mValue
            && mTrustedTimestamp == other.mTrustedTimestamp && mOCSPValues == other.mOCSPValues;
    }

    /**
     * Compares sign info.
     *
     * @param other sign info to compare with.
     * @return bool.
     */
    bool operator!=(const SignInfo& other) const { return !operator==(other); }
};

/**
 * Update image info.
 */
struct UpdateImageInfo {
    ImageInfo                                       mImage;
    StaticArray<StaticString<cURLLen>, cMaxNumURLs> mURLs;
    StaticString<cSHA256Size>                       mSHA256;
    size_t                                          mSize {};
    DecryptInfo                                     mDecryptInfo;
    SignInfo                                        mSignInfo;

    /**
     * Compares update image info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const UpdateImageInfo& other) const
    {
        return mImage == other.mImage && mURLs == other.mURLs && mSHA256 == other.mSHA256 && mSize == other.mSize
            && mDecryptInfo == other.mDecryptInfo && mSignInfo == other.mSignInfo;
    }

    /**
     * Compares update item info.
     */
    bool operator!=(const UpdateImageInfo& other) const { return !operator==(other); }
};

/**
 * Update item info.
 */
struct UpdateItemInfo : public PlatformInfo {
    Identifier                                        mIdentifier;
    StaticString<cVersionLen>                         mVersion;
    StaticArray<UpdateImageInfo, cMaxNumUpdateImages> mImages;

    /**
     * Compares update item info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator==(const UpdateItemInfo& other) const
    {
        return mIdentifier == other.mIdentifier && mVersion == other.mVersion && mImages == other.mImages;
    }

    /**
     * Compares update item info.
     *
     * @param other object to compare with.
     * @return bool.
     */
    bool operator!=(const UpdateItemInfo& other) const { return !operator==(other); }
};

/**
 * Instance info.
 */
struct InstanceInfo {
    Identifier                                                  mIdentifier;
    Identifier                                                  mSubject;
    uint64_t                                                    mPriority {0};
    size_t                                                      mNumInstances;
    StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels> mLabels;

    /**
     * Compares instance info.
     *
     * @param other instance info to compare.
     * @return bool.
     */
    bool operator==(const InstanceInfo& other) const
    {
        return mIdentifier == other.mIdentifier && mSubject == other.mSubject && mPriority == other.mPriority
            && mNumInstances == other.mNumInstances && mLabels == other.mLabels;
    }

    /**
     * Compares instance info.
     *
     * @param other instance info to compare.
     * @return bool.
     */
    bool operator!=(const InstanceInfo& other) const { return !operator==(other); }
};

/**
 * Certificate info.
 */
struct CertificateInfo {
    StaticArray<uint8_t, crypto::cCertDERSize> mCertificate;
    StaticString<cCertFingerprintLen>          mFingerprint;

    /**
     * Compares certificate info.
     *
     * @param other certificate info to compare with.
     * @return bool.
     */
    bool operator==(const CertificateInfo& other) const
    {
        return mCertificate == other.mCertificate && mFingerprint == other.mFingerprint;
    }

    /**
     * Compares certificate info.
     *
     * @param other certificate info to compare with.
     * @return bool.
     */
    bool operator!=(const CertificateInfo& other) const { return !operator==(other); }
};

/**
 * Certificate chain info.
 */
struct CertificateChainInfo {
    StaticString<cChainNameLen>                                            mName;
    StaticArray<StaticString<cCertFingerprintLen>, crypto::cCertChainSize> mFingerprints;

    /**
     * Compares certificate chain info.
     *
     * @param other certificate chain info to compare with.
     * @return bool.
     */
    bool operator==(const CertificateChainInfo& other) const
    {
        return mName == other.mName && mFingerprints == other.mFingerprints;
    }

    /**
     * Compares certificate chain info.
     *
     * @param other certificate chain info to compare with.
     * @return bool.
     */
    bool operator!=(const CertificateChainInfo& other) const { return !operator==(other); }
};

/**
 * Desired status.
 */
struct DesiredStatus {
    StaticArray<DesiredNodeState, cMaxNumNodes>                 mNodes;
    Optional<UnitConfig>                                        mUnitConfig;
    StaticArray<UpdateItemInfo, cMaxNumUpdateItems>             mUpdateItems;
    StaticArray<InstanceInfo, cMaxNumInstances>                 mInstances;
    StaticArray<CertificateInfo, cMaxNumCertificates>           mCertificates;
    StaticArray<CertificateChainInfo, crypto::cCertChainsCount> mCertificateChains;

    /**
     * Compares desired status.
     *
     * @param other desired status to compare with.
     * @return bool.
     */
    bool operator==(const DesiredStatus& other) const
    {
        return mUnitConfig == other.mUnitConfig && mNodes == other.mNodes && mUpdateItems == other.mUpdateItems
            && mInstances == other.mInstances && mCertificates == other.mCertificates
            && mCertificateChains == other.mCertificateChains;
    }

    /**
     * Compares desired status.
     *
     * @param other desired status to compare with.
     * @return bool.
     */
    bool operator!=(const DesiredStatus& other) const { return !operator==(other); }
};

} // namespace aos::cloudprotocol

#endif
