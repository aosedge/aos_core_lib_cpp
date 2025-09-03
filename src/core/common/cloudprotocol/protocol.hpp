/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_CLOUDMESSAGE_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_CLOUDMESSAGE_HPP_

#include <core/common/tools/variant.hpp>

#include "alerts.hpp"
#include "certificates.hpp"
#include "common.hpp"
#include "desiredstatus.hpp"
#include "envvars.hpp"
#include "log.hpp"
#include "monitoring.hpp"
#include "provisioning.hpp"
#include "state.hpp"
#include "unitstatus.hpp"

namespace aos::cloudprotocol {

/**
 * Cloud protocol version.
 */
static constexpr auto cProtocolVersion = 7;

/**
 * Message type.
 */
class MessageTypeType {
public:
    enum class Enum {
        eAlerts,
        eDeprovisioningRequest,
        eDeprovisioningResponse,
        eDesiredStatus,
        eFinishProvisioningRequest,
        eFinishProvisioningResponse,
        eInstallUnitCertificatesConfirmation,
        eIssuedUnitCertificates,
        eIssueUnitCertificates,
        eMonitoringData,
        eNewState,
        eOverrideEnvVars,
        eOverrideEnvVarsStatus,
        ePushLog,
        eRenewCertificatesNotification,
        eRequestLog,
        eStartProvisioningRequest,
        eStartProvisioningResponse,
        eStateAcceptance,
        eStateRequest,
        eUnitStatus,
        eUpdateState,
    };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {
            "alerts",
            "deprovisioningRequest",
            "deprovisioningResponse",
            "desiredStatus",
            "finishProvisioningRequest",
            "finishProvisioningResponse",
            "installUnitCertificatesConfirmation",
            "issuedUnitCertificates",
            "issueUnitCertificates",
            "monitoringData",
            "newState",
            "overrideEnvVars",
            "overrideEnvVarsStatus",
            "pushLog",
            "renewCertificatesNotification",
            "requestLog",
            "startProvisioningRequest",
            "startProvisioningResponse",
            "stateAcceptance",
            "stateRequest",
            "unitStatus",
            "updateState",
        };

        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using MessageTypeEnum = MessageTypeType::Enum;
using MessageType     = EnumStringer<MessageTypeType>;

/**
 * Cloud message variant type.
 */
using MessageVariant = Variant<Alerts, Monitoring, UnitStatus, DesiredStatus, NewState, StateRequest, StateAcceptance,
    UpdateState, RequestLog, PushLog, OverrideEnvVarsRequest, OverrideEnvVarsStatuses, RenewCertsNotification,
    IssuedUnitCerts, IssueUnitCerts, InstallUnitCertsConfirmation, StartProvisioningRequest, StartProvisioningResponse,
    FinishProvisioningRequest, FinishProvisioningResponse, DeprovisioningRequest, DeprovisioningResponse>;

/**
 * Cloud message header.
 */
struct MessageHeader {
    /**
     * Default constructor.
     */
    MessageHeader() = default;

    /**
     * Constructor.
     *
     * @param version protocol version.
     * @param systemID system identifier.
     */
    MessageHeader(size_t version, const String& systemID)
        : mVersion(version)
        , mSystemID(systemID)
    {
    }

    size_t                     mVersion {};
    StaticString<cSystemIDLen> mSystemID;

    /**
     * Compares message headers.
     *
     * @param header message header to compare with.
     * @return bool.
     */
    bool operator==(const MessageHeader& header) const
    {
        return mVersion == header.mVersion && mSystemID == header.mSystemID;
    }

    /**
     * Compares message headers.
     *
     * @param header message header to compare with.
     * @return bool.
     */
    bool operator!=(const MessageHeader& header) const { return !operator==(header); }
};

/**
 * Cloud message.
 */
struct CloudMessage {
    /**
     * Default constructor.
     */
    CloudMessage() = default;

    /**
     * Constructor.
     *
     * @param header message header.
     * @param data message data.
     */
    CloudMessage(const MessageHeader& header, const MessageVariant& data)
        : mHeader(header)
        , mData(data)
    {
    }

    MessageHeader  mHeader;
    MessageVariant mData;

    /**
     * Compares cloud messages.
     *
     * @param message cloud message to compare with.
     * @return bool.
     */
    bool operator==(const CloudMessage& message) const { return mHeader == message.mHeader && mData == message.mData; }

    /**
     * Compares cloud messages.
     *
     * @param message cloud message to compare with.
     * @return bool.
     */
    bool operator!=(const CloudMessage& message) const { return !operator==(message); }
};

} // namespace aos::cloudprotocol

#endif
