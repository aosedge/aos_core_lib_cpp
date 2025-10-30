/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_CORE_COMMON_MONITORING_TESTS_STUBS_ALERTSENDERSTUB_HPP_
#define AOS_CORE_COMMON_MONITORING_TESTS_STUBS_ALERTSENDERSTUB_HPP_

#include <core/common/alerts/alerts.hpp>

namespace aos::monitoring {

/**
 * Visitor to extract SystemQuotaAlert and InstanceQuotaAlert from AlertVariant.
 */
class GetAlertVariantVisitor : public StaticVisitor<void> {
public:
    GetAlertVariantVisitor(std::vector<SystemQuotaAlert>& systemAlerts, std::vector<InstanceQuotaAlert>& instanceAlerts)
        : mSystemAlerts(systemAlerts)
        , mInstanceAlerts(instanceAlerts)
    {
    }

    void Visit(const SystemQuotaAlert& alert) const { mSystemAlerts.push_back(alert); }

    void Visit(const InstanceQuotaAlert& alert) const { mInstanceAlerts.push_back(alert); }

    void Visit(...) const { }

private:
    std::vector<SystemQuotaAlert>&   mSystemAlerts;
    std::vector<InstanceQuotaAlert>& mInstanceAlerts;
};

/**
 * Alert sender stub.
 */
class AlertSenderStub : public alerts::SenderItf {
public:
    Error SendAlert(const AlertVariant& alert) override
    {
        std::lock_guard lock {mMutex};

        LOG_DBG() << "Send alert: alert=" << alert;

        GetAlertVariantVisitor visitor {mSystemQuotaAlerts, mInstanceQuotaAlerts};

        alert.ApplyVisitor(visitor);

        return ErrorEnum::eNone;
    }

    std::vector<SystemQuotaAlert> GetSystemQuotaAlerts() const
    {
        std::lock_guard lock {mMutex};

        return mSystemQuotaAlerts;
    }

    std::vector<InstanceQuotaAlert> GetInstanceQuotaAlerts() const
    {
        std::lock_guard lock {mMutex};

        return mInstanceQuotaAlerts;
    }

private:
    mutable std::mutex              mMutex;
    std::vector<SystemQuotaAlert>   mSystemQuotaAlerts;
    std::vector<InstanceQuotaAlert> mInstanceQuotaAlerts;
};

} // namespace aos::monitoring

#endif
