/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_UNITSTATUS_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_UNITSTATUS_HPP_

#include <core/common/crypto/crypto.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/types/types.hpp>

#include "cloudprotocol.hpp"

namespace aos::cloudprotocol {

/**
 * Annotations length.
 */
static constexpr auto cAnnotationsLen = AOS_CONFIG_CLOUDPROTOCOL_ANNOTATION_LEN;

/**
 * Component ID len.
 */
static constexpr auto cComponentIDLen = AOS_CONFIG_CLOUDPROTOCOL_COMPONENT_ID_LEN;

/**
 * Component type len.
 */
static constexpr auto cComponentTypeLen = AOS_CONFIG_CLOUDPROTOCOL_COMPONENT_TYPE_LEN;

/**
 * Unit config status count.
 */
static constexpr auto cUnitConfigStatusCount = AOS_CONFIG_CLOUDPROTOCOL_UNIT_CONFIG_STATUS_COUNT;

/**
 * Unit config status.
 */
struct UnitConfigStatus {
    StaticString<cVersionLen> mVersion;
    ItemStatus                mStatus;
    Error                     mError;

    /**
     * Compares unit config status.
     *
     * @param status unit config status to compare with.
     * @return bool.
     */
    bool operator==(const UnitConfigStatus& status) const
    {
        return mVersion == status.mVersion && mStatus == status.mStatus && mError == status.mError;
    }

    /**
     * Compares unit config status.
     *
     * @param status unit config status to compare with.
     * @return bool.
     */
    bool operator!=(const UnitConfigStatus& status) const { return !operator==(status); }
};

using UnitConfigStatusStaticArray = StaticArray<UnitConfigStatus, cUnitConfigStatusCount>;

/**
 * Component status.
 */
struct ComponentStatus {
    StaticString<cComponentIDLen>           mComponentID;
    StaticString<cComponentTypeLen>         mComponentType;
    StaticString<cVersionLen>               mVersion;
    Optional<StaticString<cNodeIDLen>>      mNodeID;
    ItemStatus                              mStatus;
    Optional<StaticString<cAnnotationsLen>> mAnnotations;
    Error                                   mError;

    /**
     * Compares component status.
     *
     * @param status component status to compare with.
     * @return bool.
     */
    bool operator==(const ComponentStatus& status) const
    {
        return mComponentID == status.mComponentID && mComponentType == status.mComponentType
            && mVersion == status.mVersion && mNodeID == status.mNodeID && mStatus == status.mStatus
            && mAnnotations == status.mAnnotations && mError == status.mError;
    }

    /**
     * Compares component status.
     *
     * @param status component status to compare with.
     * @return bool.
     */
    bool operator!=(const ComponentStatus& status) const { return !operator==(status); }
};

using ComponentStatusStaticArray = StaticArray<ComponentStatus, 2 * cMaxNumNodes>;

/**
 * Unit status
 */
struct UnitStatus {
    UnitConfigStatusStaticArray         mUnitConfigStatus;
    StaticArray<NodeInfo, cMaxNumNodes> mNodeInfo;
    ServiceStatusStaticArray            mServiceStatus;
    InstanceStatusStaticArray           mInstanceStatus;
    LayerStatusStaticArray              mLayerStatus;
    ComponentStatusStaticArray          mComponentStatus;
    StaticString<cSubjectIDLen>         mUnitSubjects;

    /**
     * Compares unit status.
     *
     * @param status unit status to compare with.
     * @return bool.
     */
    bool operator==(const UnitStatus& status) const
    {
        return mUnitConfigStatus == status.mUnitConfigStatus && mNodeInfo == status.mNodeInfo
            && mServiceStatus == status.mServiceStatus && mInstanceStatus == status.mInstanceStatus
            && mLayerStatus == status.mLayerStatus && mComponentStatus == status.mComponentStatus
            && mUnitSubjects == status.mUnitSubjects;
    }

    /**
     * Compares unit status.
     *
     * @param status unit status to compare with.
     * @return bool.
     */
    bool operator!=(const UnitStatus& status) const { return !operator==(status); }
};

/**
 * Delta unit status.
 */
struct DeltaUnitStatus {
    Optional<UnitConfigStatusStaticArray>         mUnitConfigStatus;
    Optional<StaticArray<NodeInfo, cMaxNumNodes>> mNodeInfo;
    Optional<ServiceStatusStaticArray>            mServiceStatus;
    Optional<InstanceStatusStaticArray>           mInstanceStatus;
    Optional<LayerStatusStaticArray>              mLayerStatus;
    Optional<ComponentStatusStaticArray>          mComponentStatus;
    Optional<StaticString<cSubjectIDLen>>         mUnitSubjects;

    /**
     * Compares unit status.
     *
     * @param status unit status to compare with.
     * @return bool.
     */
    bool operator==(const DeltaUnitStatus& status) const
    {
        return mUnitConfigStatus == status.mUnitConfigStatus && mNodeInfo == status.mNodeInfo
            && mServiceStatus == status.mServiceStatus && mInstanceStatus == status.mInstanceStatus
            && mLayerStatus == status.mLayerStatus && mComponentStatus == status.mComponentStatus
            && mUnitSubjects == status.mUnitSubjects;
    }

    /**
     * Compares unit status.
     *
     * @param status unit status to compare with.
     * @return bool.
     */
    bool operator!=(const DeltaUnitStatus& status) const { return !operator==(status); }
};

} // namespace aos::cloudprotocol

#endif
