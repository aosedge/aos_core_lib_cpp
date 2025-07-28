/*
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_ALERTPROCESSOR_HPP_
#define AOS_ALERTPROCESSOR_HPP_

#include "aos/common/alerts/alerts.hpp"
#include "aos/common/cloudprotocol/alerts.hpp"

namespace aos::monitoring {

/**
 * Resource level type.
 */
class ResourceLevelType {
public:
    enum class Enum {
        eSystem,
        eInstance,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sResourceLevelStrings[] = {
            "system",
            "instance",
        };

        return Array<const char* const>(sResourceLevelStrings, ArraySize(sResourceLevelStrings));
    };
};

using ResourceLevelEnum = ResourceLevelType::Enum;
using ResourceLevel     = EnumStringer<ResourceLevelType>;

/**
 * Resource type type.
 */
class ResourceTypeType {
public:
    enum class Enum {
        eCPU,
        eRAM,
        eDownload,
        eUpload,
        ePartition,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sResourceTypeStrings[] = {
            "cpu",
            "ram",
            "download",
            "upload",
            "partition",
        };

        return Array<const char* const>(sResourceTypeStrings, ArraySize(sResourceTypeStrings));
    };
};

using ResourceTypeEnum = ResourceTypeType::Enum;
using ResourceType     = EnumStringer<ResourceTypeType>;

/**
 * Resource identifier.
 */
struct ResourceIdentifier {
    ResourceLevel                             mLevel;
    ResourceType                              mType;
    Optional<StaticString<cPartitionNameLen>> mPartitionName;
    Optional<StaticString<cInstanceIDLen>>    mInstanceID;

    /**
     * Outputs resource identifier to log.
     *
     * @param log log to output.
     * @param identifier resource identifier.
     *
     * @return Log&.
     */
    friend Log& operator<<(Log& log, const ResourceIdentifier& identifier)
    {
        log << "{" << identifier.mLevel << ":" << identifier.mType;

        if (identifier.mPartitionName.HasValue()) {
            log << ":" << identifier.mPartitionName.GetValue();
        }

        if (identifier.mInstanceID.HasValue()) {
            log << ":" << identifier.mInstanceID.GetValue();
        }

        log << "}";

        return log;
    }
};

/**
 * Alert processor.
 */
class AlertProcessor {
public:
    /**
     * Initializes alert processor.
     *
     * @param id resource identifier.
     * @param maxValue max value.
     * @param rule alert rule.
     * @param sender alert sender.
     * @param alertTemplate alert template.
     * @return Error.
     */
    Error Init(ResourceIdentifier id, uint64_t maxValue, const AlertRulePercents& rule, alerts::SenderItf& sender,
        const cloudprotocol::AlertVariant& alertTemplate);

    /**
     * Initializes alert processor.
     *
     * @param id resource identifier.
     * @param rule alert rule.
     * @param sender alert sender.
     * @param alertTemplate alert template.
     * @return Error.
     */
    Error Init(ResourceIdentifier id, const AlertRulePoints& rule, alerts::SenderItf& sender,
        const cloudprotocol::AlertVariant& alertTemplate);

    /**
     * Checks alert detection. If alert condition is true, sends alert.
     *
     * @param currentValue current value.
     * @param currentTime current time.
     * @return Error.
     */
    Error CheckAlertDetection(const uint64_t currentValue, const Time& currentTime);

    /**
     * Returns resource identifier.
     *
     * @return const ResourceIdentifier&.
     */
    const ResourceIdentifier& GetID() const { return mID; }

private:
    Error HandleMaxThreshold(uint64_t currentValue, const Time& currentTime);
    Error HandleMinThreshold(uint64_t currentValue, const Time& currentTime);
    Error SendAlert(uint64_t currentValue, const Time& currentTime, cloudprotocol::AlertStatus status);

    ResourceIdentifier          mID {};
    alerts::SenderItf*          mAlertSender = nullptr;
    cloudprotocol::AlertVariant mAlertTemplate;

    Duration mMinTimeout       = 0;
    uint64_t mMinThreshold     = 0;
    uint64_t mMaxThreshold     = 0;
    Time     mMinThresholdTime = {};
    Time     mMaxThresholdTime = {};
    bool     mAlertCondition   = false;
};

using AlertProcessorStaticArray = StaticArray<AlertProcessor, 4 + cMaxNumPartitions>;

} // namespace aos::monitoring

#endif
