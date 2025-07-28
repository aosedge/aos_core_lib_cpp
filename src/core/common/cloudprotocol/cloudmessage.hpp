/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CLOUDPROTOCOL_CLOUDMESSAGE_HPP_
#define AOS_CLOUDPROTOCOL_CLOUDMESSAGE_HPP_

#include "aos/common/cloudprotocol/alerts.hpp"
#include "aos/common/cloudprotocol/certificates.hpp"
#include "aos/common/cloudprotocol/cloudprotocol.hpp"
#include "aos/common/cloudprotocol/desiredstatus.hpp"
#include "aos/common/cloudprotocol/envvars.hpp"
#include "aos/common/cloudprotocol/log.hpp"
#include "aos/common/cloudprotocol/monitoring.hpp"
#include "aos/common/cloudprotocol/provisioning.hpp"
#include "aos/common/cloudprotocol/state.hpp"
#include "aos/common/cloudprotocol/unitstatus.hpp"
#include "aos/common/tools/variant.hpp"

namespace aos::cloudprotocol {

/**
 * Cloud protocol version.
 */
static constexpr auto cProtocolVersion = 7;

/**
 * Cloud message variant type.
 */
using MessageVariant = Variant<Alerts, Monitoring, UnitStatus, DeltaUnitStatus, DesiredStatus, NewState, StateRequest,
    StateAcceptance, UpdateState, RequestLog, PushLog, OverrideEnvVarsRequest, OverrideEnvVarsStatuses,
    RenewCertsNotification, IssuedUnitCerts, IssueUnitCerts, InstallUnitCertsConfirmation, StartProvisioningRequest,
    StartProvisioningResponse, FinishProvisioningRequest, FinishProvisioningResponse, DeprovisioningRequest,
    DeprovisioningResponse>;

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
