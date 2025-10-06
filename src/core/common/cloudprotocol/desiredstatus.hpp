/**
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_DESIREDSTATUS_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_DESIREDSTATUS_HPP_

#include <core/common/crypto/cryptohelper.hpp>
#include <core/common/types/obsolete.hpp>

#include "common.hpp"

namespace aos::cloudprotocol {

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
        static const char* const sStrings[] = {
            "provisioned",
            "paused",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
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

using DesiredNodeStateStaticArray = StaticArray<DesiredNodeState, cMaxNumNodes>;

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
 * Update image info.
 */
struct UpdateImageInfo {
    ImageInfo                                       mImage;
    StaticArray<StaticString<cURLLen>, cMaxNumURLs> mURLs;
    StaticArray<uint8_t, crypto::cSHA256Size>       mSHA256;
    size_t                                          mSize {};
    crypto::DecryptInfo                             mDecryptInfo;
    crypto::SignInfo                                mSignInfo;

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

using UpdateImageInfoStaticArray = StaticArray<UpdateImageInfo, cMaxNumUpdateImages>;

/**
 * Update item info.
 */
struct UpdateItemInfo {
    Identifier                 mIdentifier;
    StaticString<cVersionLen>  mVersion;
    UpdateImageInfoStaticArray mImages;

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

using UpdateItemInfoStaticArray = StaticArray<UpdateItemInfo, cMaxNumUpdateItems>;

using LabelsStaticArray = StaticArray<StaticString<cLabelNameLen>, cMaxNumNodeLabels>;

/**
 * Instance info.
 */
struct InstanceInfo {
    Identifier        mIdentifier;
    Identifier        mSubject;
    uint64_t          mPriority {0};
    size_t            mNumInstances;
    LabelsStaticArray mLabels;

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

using InstanceInfoStaticArray = StaticArray<InstanceInfo, cMaxNumInstances>;

/**
 * Desired status.
 */
struct DesiredStatus {
    DesiredNodeStateStaticArray       mNodes;
    Optional<UnitConfig>              mUnitConfig;
    UpdateItemInfoStaticArray         mUpdateItems;
    InstanceInfoStaticArray           mInstances;
    crypto::CertificateInfoArray      mCertificates;
    crypto::CertificateChainInfoArray mCertificateChains;

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
