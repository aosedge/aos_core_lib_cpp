/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_ALERTS_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_ALERTS_HPP_

#include <core/common/tools/variant.hpp>

#include "common.hpp"

namespace aos::cloudprotocol {

/**
 * Alert items count.
 */
constexpr auto cAlertItemsCount = AOS_CONFIG_CLOUDPROTOCOL_ALERT_ITEMS_COUNT;

/**
 * Alert tag.
 */
class AlertTagType {
public:
    enum class Enum {
        eSystemAlert,
        eCoreAlert,
        eResourceAllocateAlert,
        eSystemQuotaAlert,
        eInstanceQuotaAlert,
        eDownloadProgressAlert,
        eInstanceAlert,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sAlertTagStrings[] = {
            "systemAlert",
            "coreAlert",
            "resourceAllocateAlert",
            "systemQuotaAlert",
            "instanceQuotaAlert",
            "downloadProgressAlert",
            "instanceAlert",
        };

        return Array<const char* const>(sAlertTagStrings, ArraySize(sAlertTagStrings));
    };
};

using AlertTagEnum = AlertTagType::Enum;
using AlertTag     = EnumStringer<AlertTagType>;

/**
 * Alert item.
 */
struct AlertItem {
    Time     mTimestamp;
    AlertTag mTag;

    /**
     * Compares alert item.
     *
     * @param item alert item to compare with.
     * @return bool.
     */
    bool operator==(const AlertItem& item) const { return mTimestamp == item.mTimestamp && mTag == item.mTag; }

    /**
     * Compares alert item.
     *
     * @param item alert item to compare with.
     * @return bool.
     */
    bool operator!=(const AlertItem& item) const { return !operator==(item); }
};

/**
 * System alert.
 */
struct SystemAlert : AlertItem {
    Identity                       mNodeID;
    StaticString<cAlertMessageLen> mMessage;

    /**
     * Compares system alert.
     *
     * @param alert system alert to compare with.
     * @return bool.
     */
    bool operator==(const SystemAlert& alert) const { return mNodeID == alert.mNodeID && mMessage == alert.mMessage; }

    /**
     * Compares system alert.
     *
     * @param alert system alert to compare with.
     * @return bool.
     */
    bool operator!=(const SystemAlert& alert) const { return !operator==(alert); }
};

/**
 * Core alert.
 */
struct CoreAlert : AlertItem {
    Identity                       mNodeID;
    CoreComponent                  mCoreComponent;
    StaticString<cAlertMessageLen> mMessage;

    /**
     * Compares core alert.
     *
     * @param alert core alert to compare with.
     * @return bool.
     */
    bool operator==(const CoreAlert& alert) const
    {
        return mNodeID == alert.mNodeID && mCoreComponent == alert.mCoreComponent && mMessage == alert.mMessage;
    }

    /**
     * Compares core alert.
     *
     * @param alert core alert to compare with.
     * @return bool.
     */
    bool operator!=(const CoreAlert& alert) const { return !operator==(alert); }
};

/**
 * Resource allocate alert.
 */
struct ResourceAllocateAlert : AlertItem, InstanceIdent {
    Identity                       mNodeID;
    StaticString<cResourceNameLen> mResource;
    StaticString<cAlertMessageLen> mMessage;

    /**
     * Compares resource allocate alert.
     *
     * @param alert resource allocate alert to compare with.
     * @return bool.
     */
    bool operator==(const ResourceAllocateAlert& alert) const
    {
        return InstanceIdent::operator==(alert) && mNodeID == alert.mNodeID && mResource == alert.mResource
            && mMessage == alert.mMessage;
    }

    /**
     * Compares resource allocate alert.
     *
     * @param alert resource allocate alert to compare with.
     * @return bool.
     */
    bool operator!=(const ResourceAllocateAlert& alert) const { return !operator==(alert); }
};

/**
 * System quota alert.
 */
struct SystemQuotaAlert : AlertItem {
    Identity                         mNodeID;
    StaticString<cAlertParameterLen> mParameter;
    uint64_t                         mValue;

    /**
     * Compares system quota alert.
     *
     * @param alert system quota alert to compare with.
     * @return bool.
     */
    bool operator==(const SystemQuotaAlert& alert) const
    {
        return mNodeID == alert.mNodeID && mParameter == alert.mParameter && mValue == alert.mValue;
    }

    /**
     * Compares system quota alert.
     *
     * @param alert system quota alert to compare with.
     * @return bool.
     */
    bool operator!=(const SystemQuotaAlert& alert) const { return !operator==(alert); }
};

/**
 * Instance quota alert.
 */
struct InstanceQuotaAlert : AlertItem, InstanceIdent {
    StaticString<cAlertParameterLen> mParameter;
    uint64_t                         mValue;

    /**
     * Compares instance quota alert.
     *
     * @param alert instance quota alert to compare with.
     * @return bool.
     */
    bool operator==(const InstanceQuotaAlert& alert) const
    {
        return InstanceIdent::operator==(alert) && mParameter == alert.mParameter && mValue == alert.mValue;
    }

    /**
     * Compares instance quota alert.
     *
     * @param alert instance quota alert to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceQuotaAlert& alert) const { return !operator==(alert); }
};

/**
 * Download alert.
 */
struct DownloadAlert : AlertItem {
    Identity                       mIdentity;
    StaticString<cVersionLen>      mVersion;
    StaticString<cAlertMessageLen> mMessage;
    StaticString<cURLLen>          mURL;
    size_t                         mDownloadedBytes;
    size_t                         mTotalBytes;

    /**
     * Compares download alert.
     *
     * @param alert download alert to compare with.
     * @return bool.
     */
    bool operator==(const DownloadAlert& alert) const
    {
        return mIdentity == alert.mIdentity && mVersion == alert.mVersion && mMessage == alert.mMessage
            && mURL == alert.mURL && mDownloadedBytes == alert.mDownloadedBytes && mTotalBytes == alert.mTotalBytes;
    }

    /**
     * Compares download alert.
     *
     * @param alert download alert to compare with.
     * @return bool.
     */
    bool operator!=(const DownloadAlert& alert) const { return !operator==(alert); }
};

/**
 * Instance alert.
 */
struct InstanceAlert : AlertItem, InstanceIdent {
    StaticString<cVersionLen>      mVersion;
    StaticString<cAlertMessageLen> mMessage;

    /**
     * Compares instance alert.
     *
     * @param alert  instance alert to compare with.
     * @return bool.
     */
    bool operator==(const InstanceAlert& alert) const
    {
        return InstanceIdent::operator==(alert) && mVersion == alert.mVersion && mMessage == alert.mMessage;
    }

    /**
     * Compares instance alert.
     *
     * @param alert  instance alert to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceAlert& alert) const { return !operator==(alert); }
};

using AlertVariant = Variant<SystemAlert, CoreAlert, DownloadAlert, SystemQuotaAlert, InstanceQuotaAlert,
    ResourceAllocateAlert, InstanceAlert>;

using AlertVariantArray = StaticArray<AlertVariant, cAlertItemsCount>;

/**
 * Alerts message structure.
 */
struct Alerts {
    AlertVariantArray mItems;

    /**
     * Compares alerts.
     *
     * @param alerts alerts to compare with.
     * @return bool.
     */
    bool operator==(const Alerts& alerts) const { return mItems == alerts.mItems; }

    /**
     * Compares alerts.
     *
     * @param alerts alerts to compare with.
     * @return bool.
     */
    bool operator!=(const Alerts& alerts) const { return !operator==(alerts); }
};

} // namespace aos::cloudprotocol

#endif
