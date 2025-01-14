/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_ALERTS_HPP_
#define AOS_CLOUDPROTOCOL_ALERTS_HPP_

#include "aos/common/tools/log.hpp"
#include "aos/common/tools/variant.hpp"
#include "aos/common/types.hpp"

namespace aos::cloudprotocol {

/**
 * Alert message len.
 */
constexpr auto cAlertMessageLen = AOS_CONFIG_CLOUDPROTOCOL_ALERT_MESSAGE_LEN;

/**
 * Alert download target id len.
 */
constexpr auto cAlertDownloadTargetIDLen = AOS_CONFIG_CLOUDPROTOCOL_ALERT_CORE_DOWNLOAD_TARGET_ID_LEN;

/**
 * Alert download progress len.
 */
constexpr auto cAlertDownloadProgressLen = AOS_CONFIG_CLOUDPROTOCOL_ALERT_DOWNLOAD_PROGRESS_LEN;

/**
 * Alert parameter len.
 */
constexpr auto cAlertParameterLen = AOS_CONFIG_CLOUDPROTOCOL_ALERT_PARAMETER_LEN;

/**
 * Resource alert errors size.
 */
constexpr auto cAlertResourceErrorsSize = AOS_CONFIG_CLOUDPROTOCOL_ALERT_RESOURCE_ERRORS_SIZE;

/**
 * Alert tag.
 */
class AlertTagType {
public:
    enum class Enum {
        eSystemAlert,
        eCoreAlert,
        eResourceValidateAlert,
        eDeviceAllocateAlert,
        eSystemQuotaAlert,
        eInstanceQuotaAlert,
        eDownloadProgressAlert,
        eServiceInstanceAlert,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sAlertTagStrings[] = {
            "systemAlert",
            "coreAlert",
            "resourceValidateAlert",
            "deviceAllocateAlert",
            "systemQuotaAlert",
            "instanceQuotaAlert",
            "downloadProgressAlert",
            "serviceInstanceAlert",
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
     * @param timestamp alert timestamp.
     */
    AlertItem(AlertTag tag, const Time& timestamp)
        : mTimestamp(timestamp)
        , mTag(tag)
    {
    }

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
struct SystemAlert : AlertItem {
    /**
     * Constructor.
     *
     * @param timestamp alert timestamp.
     */
    SystemAlert(const Time& timestamp = Time::Now())
        : AlertItem(AlertTagEnum::eSystemAlert, timestamp)
    {
    }

    StaticString<cNodeIDLen>       mNodeID;
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
 * Core component type.
 */
class CoreComponentType {
public:
    enum class Enum {
        eServiceManager,
        eUpdateManager,
        eIAMAnager,
        eCommunicationManager,
        eVIS,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sTargetTypeStrings[] = {
            "aos-servicemanager",
            "aos-updatemanager",
            "aos-iamanager",
            "aos-communicationmanager",
            "aos-vis",
        };

        return Array<const char* const>(sTargetTypeStrings, ArraySize(sTargetTypeStrings));
    };
};

using CoreComponentEnum = CoreComponentType::Enum;
using CoreComponent     = EnumStringer<CoreComponentType>;

/**
 * Core alert.
 */
struct CoreAlert : AlertItem {
    /**
     * Constructor.
     *
     * @param timestamp alert timestamp.
     */
    CoreAlert(const Time& timestamp = Time::Now())
        : AlertItem(AlertTagEnum::eCoreAlert, timestamp)
    {
    }

    StaticString<cNodeIDLen>       mNodeID;
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
 * Download target type.
 */
class DownloadTargetType {
public:
    enum class Enum {
        eComponent,
        eLayer,
        eService,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sTargetTypeStrings[] = {
            "component",
            "layer",
            "service",
        };

        return Array<const char* const>(sTargetTypeStrings, ArraySize(sTargetTypeStrings));
    };
};

using DownloadTargetEnum = DownloadTargetType::Enum;
using DownloadTarget     = EnumStringer<DownloadTargetType>;

/**
 * Download alert.
 */
struct DownloadAlert : AlertItem {
    /**
     * Constructor.
     *
     * @param timestamp alert timestamp.
     */
    DownloadAlert(const Time& timestamp = Time::Now())
        : AlertItem(AlertTagEnum::eDownloadProgressAlert, timestamp)
    {
    }

    DownloadTarget                          mTargetType;
    StaticString<cAlertDownloadTargetIDLen> mTargetID;
    StaticString<cVersionLen>               mVersion;
    StaticString<cAlertMessageLen>          mMessage;
    StaticString<cURLLen>                   mURL;
    StaticString<cAlertDownloadProgressLen> mDownloadedBytes;
    StaticString<cAlertDownloadProgressLen> mTotalBytes;

    /**
     * Compares download alert.
     *
     * @param alert download alert to compare with.
     * @return bool.
     */
    bool operator==(const DownloadAlert& alert) const
    {
        return mTargetType == alert.mTargetType && mTargetID == alert.mTargetID && mVersion == alert.mVersion
            && mMessage == alert.mMessage && mURL == alert.mURL && mDownloadedBytes == alert.mDownloadedBytes
            && mTotalBytes == alert.mTotalBytes;
    }

    /**
     * Compares download alert.
     *
     * @param alert download alert to compare with.
     * @return bool.
     */
    bool operator!=(const DownloadAlert& alert) const { return !operator==(alert); }

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
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << alert.mTargetType << ":" << alert.mURL
                   << ":" << alert.mMessage << "}";
    }
};

/**
 * Alert status type.
 */
class AlertStatusType {
public:
    enum class Enum {
        eRaise,
        eContinue,
        eFall,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sAlertStatusStrings[] = {
            "raise",
            "continue",
            "fall",
        };

        return Array<const char* const>(sAlertStatusStrings, ArraySize(sAlertStatusStrings));
    };
};

using AlertStatusEnum = AlertStatusType::Enum;
using AlertStatus     = EnumStringer<AlertStatusType>;

/**
 * System quota alert.
 */
struct SystemQuotaAlert : AlertItem {
    /**
     * Constructor.
     *
     * @param timestamp alert timestamp.
     */
    SystemQuotaAlert(const Time& timestamp = Time::Now())
        : AlertItem(AlertTagEnum::eSystemQuotaAlert, timestamp)
        , mNodeID()
        , mParameter()
        , mValue(0)
        , mStatus()
    {
    }

    StaticString<cNodeIDLen>         mNodeID;
    StaticString<cAlertParameterLen> mParameter;
    uint64_t                         mValue;
    AlertStatus                      mStatus;

    /**
     * Compares system quota alert.
     *
     * @param alert system quota alert to compare with.
     * @return bool.
     */
    bool operator==(const SystemQuotaAlert& alert) const
    {
        return mNodeID == alert.mNodeID && mParameter == alert.mParameter && mValue == alert.mValue
            && mStatus == alert.mStatus;
    }

    /**
     * Compares system quota alert.
     *
     * @param alert system quota alert to compare with.
     * @return bool.
     */
    bool operator!=(const SystemQuotaAlert& alert) const { return !operator==(alert); }

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
                   << ":" << alert.mValue << ":" << alert.mStatus << "}";
    }
};

/**
 * Instance quota alert.
 */
struct InstanceQuotaAlert : AlertItem {
    /**
     * Constructor.
     *
     * @param timestamp alert timestamp.
     */
    InstanceQuotaAlert(const Time& timestamp = Time::Now())
        : AlertItem(AlertTagEnum::eInstanceQuotaAlert, timestamp)
        , mInstanceIdent()
        , mParameter()
        , mValue(0)
        , mStatus()
    {
    }

    InstanceIdent                    mInstanceIdent;
    StaticString<cAlertParameterLen> mParameter;
    uint64_t                         mValue;
    AlertStatus                      mStatus;

    /**
     * Compares instance quota alert.
     *
     * @param alert instance quota alert to compare with.
     * @return bool.
     */
    bool operator==(const InstanceQuotaAlert& alert) const
    {
        return mInstanceIdent == alert.mInstanceIdent && mParameter == alert.mParameter && mValue == alert.mValue
            && mStatus == alert.mStatus;
    }

    /**
     * Compares instance quota alert.
     *
     * @param alert instance quota alert to compare with.
     * @return bool.
     */
    bool operator!=(const InstanceQuotaAlert& alert) const { return !operator==(alert); }

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
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << alert.mParameter << ":" << alert.mValue
                   << ":" << alert.mStatus << "}";
    }
};

/**
 * Device allocate alert.
 */
struct DeviceAllocateAlert : AlertItem {
    /**
     * Constructor.
     *
     * @param timestamp alert timestamp.
     */
    DeviceAllocateAlert(const Time& timestamp = Time::Now())
        : AlertItem(AlertTagEnum::eDeviceAllocateAlert, timestamp)
        , mInstanceIdent()
    {
    }

    InstanceIdent                  mInstanceIdent;
    StaticString<cNodeIDLen>       mNodeID;
    StaticString<cDeviceNameLen>   mDevice;
    StaticString<cAlertMessageLen> mMessage;

    /**
     * Compares device allocate alert.
     *
     * @param alert device allocate alert to compare with.
     * @return bool.
     */
    bool operator==(const DeviceAllocateAlert& alert) const
    {
        return mInstanceIdent == alert.mInstanceIdent && mNodeID == alert.mNodeID && mDevice == alert.mDevice
            && mMessage == alert.mMessage;
    }

    /**
     * Compares device allocate alert.
     *
     * @param alert device allocate alert to compare with.
     * @return bool.
     */
    bool operator!=(const DeviceAllocateAlert& alert) const { return !operator==(alert); }

    /**
     * Outputs device allocate alert to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const DeviceAllocateAlert& alert)
    {
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << alert.mInstanceIdent << ":" << alert.mNodeID
                   << ":" << alert.mDevice << ":" << alert.mMessage << "}";
    }
};

/**
 * Resource validate alert.
 */
struct ResourceValidateAlert : AlertItem {
    /**
     * Constructor.
     *
     * @param timestamp alert timestamp.
     */
    ResourceValidateAlert(const Time& timestamp = Time::Now())
        : AlertItem(AlertTagEnum::eResourceValidateAlert, timestamp)
    {
    }

    StaticString<cNodeIDLen>                     mNodeID;
    StaticString<cResourceNameLen>               mName;
    StaticArray<Error, cAlertResourceErrorsSize> mErrors;

    /**
     * Compares resource validate alert.
     *
     * @param alert resource validate alert to compare with.
     * @return bool.
     */
    bool operator==(const ResourceValidateAlert& alert) const
    {
        return mNodeID == alert.mNodeID && mName == alert.mName && mErrors == alert.mErrors;
    }

    /**
     * Compares resource validate alert.
     *
     * @param alert resource validate alert to compare with.
     * @return bool.
     */
    bool operator!=(const ResourceValidateAlert& alert) const { return !operator==(alert); }

    /**
     * Outputs resource validate alert to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const ResourceValidateAlert& alert)
    {
        log << "{" << static_cast<const AlertItem&>(alert) << ":" << alert.mNodeID << ":" << alert.mName;

        for (const auto& error : alert.mErrors) {
            log << ":" << error;
        }

        return log << "}";
    }
};

/**
 * Service instance alert.
 */
struct ServiceInstanceAlert : AlertItem {
    /**
     * Constructor.
     *
     * @param timestamp alert timestamp.
     */
    ServiceInstanceAlert(const Time& timestamp = Time::Now())
        : AlertItem(AlertTagEnum::eServiceInstanceAlert, timestamp)
        , mInstanceIdent()
    {
    }

    InstanceIdent                  mInstanceIdent;
    StaticString<cVersionLen>      mServiceVersion;
    StaticString<cAlertMessageLen> mMessage;

    /**
     * Compares service instance alert.
     *
     * @param alert service instance alert to compare with.
     * @return bool.
     */
    bool operator==(const ServiceInstanceAlert& alert) const
    {
        return mInstanceIdent == alert.mInstanceIdent && mServiceVersion == alert.mServiceVersion
            && mMessage == alert.mMessage;
    }

    /**
     * Compares service instance alert.
     *
     * @param alert service instance alert to compare with.
     * @return bool.
     */
    bool operator!=(const ServiceInstanceAlert& alert) const { return !operator==(alert); }

    /**
     * Outputs service instance alert to log.
     *
     * @param log log to output.
     * @param alert alert.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const ServiceInstanceAlert& alert)
    {
        return log << "{" << static_cast<const AlertItem&>(alert) << ":" << alert.mInstanceIdent << ":"
                   << alert.mServiceVersion << ":" << alert.mMessage << "}";
    }
};

using AlertVariant = Variant<SystemAlert, CoreAlert, DownloadAlert, SystemQuotaAlert, InstanceQuotaAlert,
    DeviceAllocateAlert, ResourceValidateAlert, ServiceInstanceAlert>;

} // namespace aos::cloudprotocol

#endif
