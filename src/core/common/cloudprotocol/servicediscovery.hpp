/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_CLOUDPROTOCOL_SERVICEDISCOVERY_HPP_
#define AOS_CORE_COMMON_CLOUDPROTOCOL_SERVICEDISCOVERY_HPP_

#include "common.hpp"

namespace aos::cloudprotocol {

/**
 * Service discovery supported protocols count.
 */
constexpr auto cServiceDiscoveryProtocolsCount = AOS_CONFIG_CLOUDPROTOCOL_SERVICE_DISCOVERY_PROTOCOLS_COUNT;

/**
 * Service discovery request.
 */
struct ServiceDiscoveryRequest {
    size_t                                                                       mVersion {};
    StaticString<cIDLen>                                                         mSystemID;
    StaticArray<StaticString<cProtocolNameLen>, cServiceDiscoveryProtocolsCount> mSupportedProtocols;

    /**
     * Compares service discovery request.
     *
     * @param request service discovery request to compare with.
     * @return bool.
     */
    bool operator==(const ServiceDiscoveryRequest& request) const
    {
        return mVersion == request.mVersion && mSystemID == request.mSystemID
            && mSupportedProtocols == request.mSupportedProtocols;
    }

    /**
     * Compares service discovery request.
     *
     * @param request service discovery request to compare with.
     * @return bool.
     */
    bool operator!=(const ServiceDiscoveryRequest& request) const { return !operator==(request); }
};

/**
 * Service discovery response error code.
 */
class ServiceDiscoveryResponseErrorType {
public:
    enum class Enum { eNoError, eRedirect, eRepeatLater, eError };

    static const Array<const char* const> GetStrings()
    {
        static const char* const sStrings[] = {"NoError", "Redirect", "RepeatLater", "Error"};
        return Array<const char* const>(sStrings, ArraySize(sStrings));
    };
};

using ServiceDiscoveryResponseErrorEnum = ServiceDiscoveryResponseErrorType::Enum;
using ServiceDiscoveryResponseError     = EnumStringer<ServiceDiscoveryResponseErrorType>;

/**
 * Service discovery response.
 */
struct ServiceDiscoveryResponse {
    size_t                                          mVersion {};
    StaticString<cIDLen>                            mSystemID;
    Duration                                        mNextRequestDelay;
    StaticArray<StaticString<cURLLen>, cMaxNumURLs> mConnectionInfo;
    StaticString<cBearerTokenLen>                   mAuthToken;
    ServiceDiscoveryResponseError                   mErrorCode;

    /**
     * Compares service discovery response.
     *
     * @param response service discovery response to compare with.
     * @return bool.
     */
    bool operator==(const ServiceDiscoveryResponse& response) const
    {
        return mVersion == response.mVersion && mSystemID == response.mSystemID
            && mNextRequestDelay == response.mNextRequestDelay && mConnectionInfo == response.mConnectionInfo
            && mAuthToken == response.mAuthToken && mErrorCode == response.mErrorCode;
    }

    /**
     * Compares service discovery response.
     *
     * @param response service discovery response to compare with.
     * @return bool.
     */
    bool operator!=(const ServiceDiscoveryResponse& response) const { return !operator==(response); }
};

} // namespace aos::cloudprotocol

#endif
