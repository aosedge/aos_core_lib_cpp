/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CM_LAUNCHER_STUBS_MONITORINGPROVIDERSTUB_HPP_
#define AOS_CM_LAUNCHER_STUBS_MONITORINGPROVIDERSTUB_HPP_

#include <map>

#include <core/cm/launcher/itf/monitoringprovider.hpp>

namespace aos::cm::launcher {

class MonitoringProviderStub : public MonitoringProviderItf {
public:
    void Init() { mData.clear(); }

    void SetAverageMonitoring(const String& nodeID, const monitoring::NodeMonitoringData& data)
    {
        mData[nodeID] = data;
    }

    Error GetAverageMonitoring(const String& nodeID, monitoring::NodeMonitoringData& monitoring) const override
    {
        auto it = mData.find(nodeID);
        if (it == mData.end()) {
            return ErrorEnum::eNotFound;
        }

        monitoring = it->second;

        return ErrorEnum::eNone;
    }

private:
    std::map<StaticString<cIDLen>, monitoring::NodeMonitoringData> mData;
};

} // namespace aos::cm::launcher

#endif
