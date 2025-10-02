/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_TYPES_ALERTS_HPP_
#define AOS_CORE_COMMON_TYPES_ALERTS_HPP_

#include <core/common/config.hpp>
#include <core/common/tools/optional.hpp>
#include <core/common/tools/variant.hpp>

#include "common.hpp"

namespace aos {

/**
 * Alert items count.
 */
constexpr auto cAlertItemsCount = AOS_CONFIG_TYPES_ALERT_ITEMS_COUNT;

/**
 * Alert message len.
 */
constexpr auto cAlertMessageLen = AOS_CONFIG_TYPES_ALERT_MESSAGE_LEN;

/**
 * Alert parameter len.
 */
constexpr auto cAlertParameterLen = AOS_CONFIG_TYPES_ALERT_PARAMETER_LEN;

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
            "updateItemInstanceAlert",
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
    /**
     * Constructor.
     *
     * @param tag alert tag.
     */
    explicit AlertItem(AlertTag tag)
        : mTag(tag)
    {
    }

    Time     mTimestamp {};
    AlertTag mTag;

    /**
     * Compares alert item.
     *
     * @param rhs alert item to compare with.
     * @return bool.
     */
    bool operator==(const AlertItem& rhs) const { return mTimestamp == rhs.mTimestamp && mTag == rhs.mTag; }

    /**
     * Compares alert item.
     *
     * @param rhs alert item to compare with.
     * @return bool.
     */
    bool operator!=(const AlertItem& rhs) const { return !operator==(rhs); }

    /**
     * Outputs alert item to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const AlertItem& alert) { return log << alert.mTimestamp << ":" << alert.mTag; }
};

/**
 * System alert.
 */
struct SystemAlert : public AlertItem {
    /**
     * Constructor.
     */
    SystemAlert()
        : AlertItem(AlertTagEnum::eSystemAlert)
    {
    }

    StaticString<cIDLen>           mNodeID;
    StaticString<cAlertMessageLen> mMessage;

    /**
     * Compares system alert.
     *
     * @param rhs system alert to compare with.
     * @return bool.
     */
    bool operator==(const SystemAlert& rhs) const
    {
        return AlertItem::operator==(rhs) && mNodeID == rhs.mNodeID && mMessage == rhs.mMessage;
    }

    /**
     * Compares system alert.
     *
     * @param rhs system alert to compare with.
     * @return bool.
     */
    bool operator!=(const SystemAlert& rhs) const { return !operator==(rhs); }

    /**
     * Outputs system alert to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const SystemAlert& alert)
    {
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << alert.mNodeID << ":" << alert.mMessage
                   << "}";
    }
};

/**
 * Core alert.
 */
struct CoreAlert : AlertItem {
    /**
     * Constructor.
     */
    CoreAlert()
        : AlertItem(AlertTagEnum::eCoreAlert)
    {
    }

    StaticString<cIDLen>           mNodeID;
    CoreComponent                  mCoreComponent;
    StaticString<cAlertMessageLen> mMessage;

    /**
     * Compares core alert.
     *
     * @param rhs core alert to compare with.
     * @return bool.
     */
    bool operator==(const CoreAlert& rhs) const
    {
        return AlertItem::operator==(rhs) && mNodeID == rhs.mNodeID && mCoreComponent == rhs.mCoreComponent
            && mMessage == rhs.mMessage;
    }

    /**
     * Compares core alert.
     *
     * @param rhs core alert to compare with.
     * @return bool.
     */
    bool operator!=(const CoreAlert& rhs) const { return !operator==(rhs); }

    /**
     * Outputs core alert to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const CoreAlert& alert)
    {
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << alert.mNodeID << ":" << alert.mCoreComponent
                   << ":" << alert.mMessage << "}";
    }
};

/**
 * Resource allocate alert.
 */
struct ResourceAllocateAlert : AlertItem, InstanceIdent {
    /**
     * Constructor.
     */
    ResourceAllocateAlert()
        : AlertItem(AlertTagEnum::eResourceAllocateAlert)
    {
    }

    StaticString<cIDLen>           mNodeID;
    StaticString<cResourceNameLen> mResource;
    StaticString<cAlertMessageLen> mMessage;

    /**
     * Compares resource allocate alert.
     *
     * @param rhs resource allocate alert to compare with.
     * @return bool.
     */
    bool operator==(const ResourceAllocateAlert& rhs) const
    {
        return AlertItem::operator==(rhs) && InstanceIdent::operator==(rhs) && mNodeID == rhs.mNodeID
            && mResource == rhs.mResource && mMessage == rhs.mMessage;
    }

    /**
     * Compares resource allocate alert.
     *
     * @param alert resource allocate alert to compare with.
     * @return bool.
     */
    bool operator!=(const ResourceAllocateAlert& rhs) const { return !operator==(rhs); }

    /**
     * Outputs resource allocate alert to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const ResourceAllocateAlert& alert)
    {
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << static_cast<const InstanceIdent&>(alert)
                   << ":" << alert.mNodeID << ":" << alert.mResource << ":" << alert.mMessage << "}";
    }
};

/**
 * Quota alert state.
 */
class QuotaAlertStateType {
public:
    enum class Enum {
        eRaise,
        eContinue,
        eFall,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "raise",
            "continue",
            "fall",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using QuotaAlertStateEnum = QuotaAlertStateType::Enum;
using QuotaAlertState     = EnumStringer<QuotaAlertStateType>;

/**
 * System quota alert.
 */
struct SystemQuotaAlert : AlertItem {
    /**
     * Constructor.
     */
    SystemQuotaAlert()
        : AlertItem(AlertTagEnum::eSystemQuotaAlert)
    {
    }

    StaticString<cIDLen>             mNodeID;
    StaticString<cAlertParameterLen> mParameter;
    size_t                           mValue {};
    QuotaAlertState                  mState;

    /**
     * Compares system quota alert.
     *
     * @param rhs system quota alert to compare with.
     * @return bool.
     */
    bool operator==(const SystemQuotaAlert& rhs) const
    {
        return AlertItem::operator==(rhs) && mNodeID == rhs.mNodeID && mParameter == rhs.mParameter
            && mValue == rhs.mValue && mState == rhs.mState;
    }

    /**
     * Compares system quota alert.
     *
     * @param rhs system quota alert to compare with.
     * @return bool.
     */
    bool operator!=(const SystemQuotaAlert& rhs) const { return !operator==(rhs); }

    /**
     * Outputs system quota alert to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const SystemQuotaAlert& alert)
    {
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << alert.mNodeID << ":" << alert.mParameter
                   << ":" << alert.mValue << ":" << alert.mState << "}";
    }
};

/**
 * Instance quota alert.
 */
struct InstanceQuotaAlert : AlertItem, InstanceIdent {
    /**
     * Constructor.
     */
    InstanceQuotaAlert()
        : AlertItem(AlertTagEnum::eInstanceQuotaAlert)
    {
    }

    StaticString<cAlertParameterLen> mParameter;
    size_t                           mValue {};
    QuotaAlertState                  mState;

    /**
     * Compares instance quota alert.
     *
     * @param rhs instance quota alert to compare with.
     * @return bool.
     */
    bool operator==(const InstanceQuotaAlert& rhs) const
    {
        return AlertItem::operator==(rhs) && InstanceIdent::operator==(rhs) && mParameter == rhs.mParameter
            && mValue == rhs.mValue && mState == rhs.mState;
    }

    /**
     * Compares instance quota alert.
     *
     * @param alert instance quota alert to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceQuotaAlert& rhs) const { return !operator==(rhs); }

    /**
     * Outputs instance quota alert to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const InstanceQuotaAlert& alert)
    {
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << static_cast<const InstanceIdent&>(alert)
                   << ":" << alert.mParameter << ":" << alert.mValue << ":" << alert.mState << "}";
    }
};

/**
 * Download state.
 */
class DownloadStateType {
public:
    enum class Enum {
        eStarted,
        ePaused,
        eInterrupted,
        eFinished,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "started",
            "paused",
            "interrupted",
            "finished",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using DownloadStateEnum = DownloadStateType::Enum;
using DownloadState     = EnumStringer<DownloadStateType>;

/**
 * Download alert.
 */
struct DownloadAlert : AlertItem {
    /**
     * Constructor.
     */
    DownloadAlert()
        : AlertItem(AlertTagEnum::eDownloadProgressAlert)
    {
    }

    StaticString<cIDLen>                     mImageID;
    StaticString<cURLLen>                    mURL;
    size_t                                   mDownloadedBytes {};
    size_t                                   mTotalBytes {};
    DownloadState                            mState;
    Optional<StaticString<cAlertMessageLen>> mReason;
    Error                                    mError;

    /**
     * Compares download alert.
     *
     * @param rhs download alert to compare with.
     * @return bool.
     */
    bool operator==(const DownloadAlert& rhs) const
    {
        return AlertItem::operator==(rhs) && mImageID == rhs.mImageID && mURL == rhs.mURL
            && mDownloadedBytes == rhs.mDownloadedBytes && mTotalBytes == rhs.mTotalBytes && mState == rhs.mState
            && mReason == rhs.mReason && mError == rhs.mError;
    }

    /**
     * Compares download alert.
     *
     * @param rhs download alert to compare with.
     * @return bool.
     */
    bool operator!=(const DownloadAlert& rhs) const { return !operator==(rhs); }

    /**
     * Outputs download alert to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const DownloadAlert& alert)
    {
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << alert.mImageID << ":" << alert.mURL << ":"
                   << alert.mDownloadedBytes << ":" << alert.mTotalBytes << ":" << alert.mState << ":"
                   << (alert.mReason.HasValue() ? *alert.mReason : "") << ":" << alert.mError << "}";
    }
};

/**
 * Instance alert.
 */
struct InstanceAlert : AlertItem, InstanceIdent {
    /**
     * Constructor.
     */
    InstanceAlert()
        : AlertItem(AlertTagEnum::eInstanceAlert)
    {
    }

    StaticString<cVersionLen>      mVersion;
    StaticString<cAlertMessageLen> mMessage;

    /**
     * Compares instance alert.
     *
     * @param rhs instance alert to compare with.
     * @return bool.
     */
    bool operator==(const InstanceAlert& rhs) const
    {
        return AlertItem::operator==(rhs) && InstanceIdent::operator==(rhs) && mVersion == rhs.mVersion
            && mMessage == rhs.mMessage;
    }

    /**
     * Compares instance alert.
     *
     * @param alert  instance alert to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceAlert& rhs) const { return !operator==(rhs); }

    /**
     * Outputs instance alert to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const InstanceAlert& alert)
    {
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << static_cast<const InstanceIdent&>(alert)
                   << ":" << alert.mVersion << ":" << alert.mMessage << "}";
    }
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

} // namespace aos

#endif
