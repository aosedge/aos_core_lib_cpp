/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <core/common/tools/logger.hpp>

#include "launcher.hpp"

namespace aos::cm::launcher {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

class ShouldRebalanceVisitor : public StaticVisitor<bool> {
public:
    Res Visit(const SystemQuotaAlert& alert) const { return alert.mState == QuotaAlertStateEnum::eFall; }

    template <typename T>
    Res Visit(const T& alert) const
    {
        (void)alert;
        return false;
    }
};

Error PushUnique(Array<StaticString<cIDLen>>& array, const StaticString<cIDLen>& value)
{
    if (array.Contains(value)) {
        return ErrorEnum::eNone;
    }

    return array.PushBack(value);
}

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::Init(const Config& config, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    InstanceRunnerItf& runner, imagemanager::ItemInfoProviderItf& itemInfoProvider, oci::OCISpecItf& ociSpec,
    unitconfig::NodeConfigProviderItf& nodeConfigProvider, storagestate::StorageStateItf& storageState,
    networkmanager::NetworkManagerItf& networkManager, MonitoringProviderItf& monitorProvider,
    alerts::AlertsProviderItf& alertsProvider, iamclient::IdentProviderItf& identProvider,
    IdentifierPoolValidator gidValidator, IdentifierPoolValidator uidValidator, StorageItf& storage)
{
    LOG_DBG() << "Init Launcher";

    mConfig             = config;
    mStorage            = &storage;
    mNodeInfoProvider   = &nodeInfoProvider;
    mRunner             = &runner;
    mNodeConfigProvider = &nodeConfigProvider;
    mStorageState       = &storageState;
    mMonitorProvider    = &monitorProvider;
    mAlertsProvider     = &alertsProvider;
    mIdentProvider      = &identProvider;

    mNetworkManager.Init(networkManager);

    auto err = mInstanceManager.Init(
        config, itemInfoProvider, storageState, ociSpec, gidValidator, uidValidator, storage, mNetworkManager);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mNodeManager.Init(*mNodeInfoProvider, *mNodeConfigProvider, *mRunner);
    mBalancer.Init(
        mInstanceManager, itemInfoProvider, ociSpec, mNodeManager, *mMonitorProvider, *mRunner, mNetworkManager);

    return ErrorEnum::eNone;
}

Error Launcher::Start()
{
    LOG_DBG() << "Start Launcher";

    LockGuard updateLock {mUpdateMutex};
    LockGuard balancingLock {mBalancingMutex};

    mIsRunning = true;

    // Start managers.
    if (auto err = mInstanceManager.Start(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager.Start(); !err.IsNone()) {
        return err;
    }

    if (auto err = mBalancer.LoadInstances(); !err.IsNone()) {
        return err;
    }

    // Subscribe to providers.
    if (auto err = mNodeInfoProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mIdentProvider->SubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    StaticArray<AlertTag, 1> alertTags;

    alertTags.PushBack(AlertTagEnum::eSystemQuotaAlert);

    if (auto err = mAlertsProvider->SubscribeListener(alertTags, *this); !err.IsNone()) {
        return err;
    }

    // Set initial subjects list.
    auto subjects = MakeUnique<SubjectArray>(&mAllocator);

    if (auto err = mIdentProvider->GetSubjects(*subjects); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto [_, err] = mInstanceManager.SetSubjects(*subjects); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // Start monitoring thread.
    mDisableProcessUpdates = false;
    mUpdatedNodes.Clear();
    mNewSubjects.SetValue(*subjects); // Check subjects after startup.

    UpdateInstanceStatuses();

    if (auto err = mWorkerThread.Run([this](void*) { ProcessUpdate(); }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::Stop()
{
    LOG_DBG() << "Stop Launcher";

    UniqueLock updateLock {mUpdateMutex};

    // Finish monitoring thread.
    mIsRunning             = false;
    mDisableProcessUpdates = false;
    mAlertReceived         = false;
    mUpdatedNodes.Clear();
    mNewSubjects.Reset();
    mInstanceStatuses.Clear();

    mProcessUpdatesCondVar.NotifyAll();
    mAllNodesConnectedCondVar.NotifyAll();

    // Unsubscribe from providers.
    if (auto err = mIdentProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mAlertsProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (auto err = mNodeInfoProvider->UnsubscribeListener(*this); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // Stop managers.
    if (auto err = mInstanceManager.Stop(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager.Stop(); !err.IsNone()) {
        return err;
    }

    updateLock.Unlock();

    if (auto err = mWorkerThread.Join(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::RunInstances(const Array<RunInstanceRequest>& requests, Array<InstanceStatus>& statuses)
{
    LOG_DBG() << "Run instances";

    UniqueLock updateLock {mUpdateMutex};
    UniqueLock balancingLock {mBalancingMutex};

    WaitAllNodesConnected(updateLock);

    if (!mIsRunning) {
        return AOS_ERROR_WRAP(ErrorEnum::eCanceled);
    }

    // Disable node monitoring for the duration of running instances.
    mDisableProcessUpdates    = true;
    auto enableNodeMonitoring = DeferRelease(this, [](Launcher* self) {
        self->mDisableProcessUpdates = false;
        self->mProcessUpdatesCondVar.NotifyAll();
    });

    ScheduleInstances(requests);

    auto runErr = mBalancer.RunInstances(updateLock, false);

    FailActivatingInstances();
    UpdateInstanceStatuses();

    if (!runErr.IsNone()) {
        return AOS_ERROR_WRAP(runErr);
    }

    if (auto err = statuses.Assign(mInstanceStatuses); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::GetInstancesStatuses(Array<InstanceStatus>& statuses)
{
    LockGuard updateLock {mUpdateMutex};

    if (auto err = statuses.Assign(mInstanceStatuses); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::SubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    LockGuard updateLock {mUpdateMutex};

    LOG_DBG() << "Subscribe instance status listener";

    if (auto err = mInstanceStatusListeners.PushBack(&listener); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::UnsubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    LockGuard updateLock {mUpdateMutex};

    LOG_DBG() << "Unsubscribe instance status listener";

    auto count = mInstanceStatusListeners.Remove(&listener);

    return count == 0 ? AOS_ERROR_WRAP(ErrorEnum::eNotFound) : ErrorEnum::eNone;
}

Error Launcher::OverrideEnvVars(const OverrideEnvVarsRequest& envVars)
{
    (void)envVars;

    LOG_DBG() << "Override env vars";

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

void Launcher::UpdateInstanceStatuses()
{
    const auto& activeInstances        = mInstanceManager.GetActiveInstances();
    const auto& preinstalledComponents = mInstanceManager.GetPreinstalledComponents();
    const auto  totalSize              = activeInstances.Size() + preinstalledComponents.Size();

    bool changed = mInstanceStatuses.Size() != totalSize;

    if (auto err = mInstanceStatuses.Resize(totalSize); !err.IsNone()) {
        LOG_ERR() << "Failed to resize instance statuses array" << Log::Field(AOS_ERROR_WRAP(err));

        return;
    }

    for (size_t i = 0; i < activeInstances.Size(); ++i) {
        const auto& newStatus = activeInstances[i]->GetStatus();

        if (mInstanceStatuses[i] != newStatus) {
            changed              = true;
            mInstanceStatuses[i] = newStatus;
        }
    }

    for (size_t i = 0; i < preinstalledComponents.Size(); ++i) {
        const auto& newStatus = preinstalledComponents[i];
        const auto  index     = activeInstances.Size() + i;

        if (mInstanceStatuses[index] != newStatus) {
            changed                  = true;
            mInstanceStatuses[index] = newStatus;
        }
    }

    if (!changed) {
        return;
    }

    for (auto& listener : mInstanceStatusListeners) {
        listener->OnInstancesStatusesChanged(mInstanceStatuses);
    }
}

void Launcher::FailActivatingInstances()
{
    for (auto& instance : mInstanceManager.GetActiveInstances()) {
        if (instance->GetStatus().mState == aos::InstanceStateEnum::eActivating) {
            // Keep node ID, because instance still scheduled, but node didn't send activating status.
            instance->SetError(AOS_ERROR_WRAP(ErrorEnum::eTimeout), false);
        }
    }
}

Error Launcher::Rebalance(UniqueLock<Mutex>& lock)
{
    LOG_DBG() << "Rebalance instances";

    // Disable node monitoring for the duration of rebalancing.
    mDisableProcessUpdates    = true;
    auto enableNodeMonitoring = DeferRelease(this, [](Launcher* self) {
        self->mDisableProcessUpdates = false;
        self->mProcessUpdatesCondVar.NotifyAll();
    });

    ScheduleInstances();

    auto runErr = mBalancer.RunInstances(lock, true);

    FailActivatingInstances();

    if (!runErr.IsNone()) {
        return AOS_ERROR_WRAP(runErr);
    }

    UpdateInstanceStatuses();

    return ErrorEnum::eNone;
}

void Launcher::ProcessUpdate()
{
    while (true) {
        UniqueLock updateLock {mUpdateMutex};

        mProcessUpdatesCondVar.Wait(updateLock, [this]() {
            return (!mUpdatedNodes.IsEmpty() || mNewSubjects.HasValue() || mAlertReceived || !mIsRunning)
                && !mDisableProcessUpdates;
        });

        WaitAllNodesConnected(updateLock);

        UniqueLock balancingLock {mBalancingMutex};

        if (!mIsRunning) {
            return;
        }

        // Update subjects.
        Error err;
        bool  doRebalance = false;

        if (mNewSubjects.HasValue()) {
            Tie(doRebalance, err) = mInstanceManager.SetSubjects(mNewSubjects.GetValue());
            if (!err.IsNone()) {
                LOG_ERR() << "Failed to set subjects" << Log::Field(AOS_ERROR_WRAP(err));
            }

            mNewSubjects.Reset();
        }

        // Resend instances.
        if (!mUpdatedNodes.IsEmpty()) {
            if (!doRebalance) {
                if (err = mNodeManager.ResendInstances(updateLock, mUpdatedNodes); !err.IsNone()) {
                    LOG_ERR() << "Failed to resend instances" << Log::Field(AOS_ERROR_WRAP(err));
                }
            } else {
                LOG_INF() << "Rebalancing will be performed, skip resending instances";
            }

            mUpdatedNodes.Clear();
        }

        // Process received alert
        if (mAlertReceived) {
            mAlertReceived = false;
            doRebalance    = true;
        }

        // Rebalance.
        if (doRebalance) {
            if (err = Rebalance(updateLock); !err.IsNone()) {
                LOG_ERR() << "Rebalancing failed" << Log::Field(AOS_ERROR_WRAP(err));
            }
        }
    }
}

void Launcher::WaitAllNodesConnected(UniqueLock<Mutex>& lock)
{
    auto allNodesConnected = [this]() {
        auto notConnected = [](const Node& node) { return !node.GetInfo().mIsConnected; };

        return !mNodeManager.GetNodes().ContainsIf(notConnected) || !mIsRunning;
    };

    mAllNodesConnectedCondVar.Wait(lock, allNodesConnected);
}

void Launcher::ScheduleInstances()
{
    for (auto& instance : mInstanceManager.GetActiveInstances()) {
        auto instanceIdent = instance->GetInfo().mInstanceIdent;

        if (auto err = mInstanceManager.ScheduleInstance(instance); !err.IsNone()) {
            LOG_ERR() << "Can't schedule instance" << Log::Field("instance", instanceIdent) << Log::Field(err);

            continue;
        }
    }

    for (auto& instance : mInstanceManager.GetCachedInstances()) {
        auto instanceIdent = instance->GetInfo().mInstanceIdent;

        if (instance->GetInfo().mState != InstanceStateEnum::eDisabled) {
            continue;
        }

        if (auto err = mInstanceManager.ScheduleInstance(instance); !err.IsNone()) {
            LOG_DBG() << "Can't schedule disabled instance" << Log::Field("instance", instanceIdent) << Log::Field(err);

            continue;
        }
    }
}

void Launcher::ScheduleInstances(const Array<RunInstanceRequest>& requests)
{
    // Sort input requests by priority
    auto sortedRequests = MakeUnique<StaticArray<RunInstanceRequest, cMaxNumInstances>>(&mAllocator);
    *sortedRequests     = requests;

    sortedRequests->Sort([](const RunInstanceRequest& left, const RunInstanceRequest& right) {
        return left.mPriority > right.mPriority || (left.mPriority == right.mPriority && left.mItemID < right.mItemID);
    });

    // Schedule instances
    for (const auto& request : requests) {
        for (size_t i = 0; i < request.mNumInstances; i++) {

            InstanceIdent instanceIdent {request.mItemID, request.mSubjectInfo.mSubjectID, i, request.mUpdateItemType};

            auto err = mInstanceManager.ScheduleInstance(instanceIdent, request);
            if (!err.IsNone()) {
                LOG_ERR() << "Can't schedule instance" << Log::Field("instance", instanceIdent) << Log::Field(err);

                continue;
            }
        }
    }
}

Error Launcher::OnInstanceStatusReceived(const InstanceStatus& status)
{
    LOG_INF() << "Instance status received" << Log::Field("instance", static_cast<const InstanceIdent&>(status));

    LockGuard updateLock {mUpdateMutex};

    if (auto err = mInstanceManager.UpdateStatus(status); !err.IsNone()) {
        return err;
    }

    UpdateInstanceStatuses();

    return ErrorEnum::eNone;
}

Error Launcher::OnNodeInstancesStatusesReceived(const String& nodeID, const Array<InstanceStatus>& statuses)
{
    LOG_INF() << "Node instances statuses received" << Log::Field("nodeID", nodeID);

    LockGuard updateLock {mUpdateMutex};

    if (!mIsRunning) {
        return ErrorEnum::eNone;
    }

    Error firstErr;

    // Update instance manager.
    for (const auto& status : statuses) {
        if (auto err = mInstanceManager.UpdateStatus(status); !err.IsNone() && firstErr.IsNone()) {
            firstErr = err;
        }
    }

    // Update node manager.
    if (auto err = mNodeManager.UpdateRunnigInstances(nodeID, statuses); !err.IsNone() && firstErr.IsNone()) {
        firstErr = err;
    }

    if (!firstErr.IsNone()) {
        return firstErr;
    }

    UpdateInstanceStatuses();

    if (auto err = PushUnique(mUpdatedNodes, nodeID); !err.IsNone()) {
        LOG_ERR() << "Failed to add node ID to updated nodes" << Log::Field(AOS_ERROR_WRAP(err));

        return AOS_ERROR_WRAP(err);
    }

    mProcessUpdatesCondVar.NotifyAll();

    return ErrorEnum::eNone;
}

void Launcher::OnNodeInfoChanged(const UnitNodeInfo& info)
{
    LOG_DBG() << "Node info changed" << Log::Field("nodeID", info.mNodeID);

    UniqueLock updateLock {mUpdateMutex};

    if (mNodeManager.UpdateNodeInfo(info)) {
        if (auto err = PushUnique(mUpdatedNodes, info.mNodeID); !err.IsNone()) {
            LOG_ERR() << "Failed to add node ID to updated nodes" << Log::Field(AOS_ERROR_WRAP(err));

            return;
        }

        mProcessUpdatesCondVar.NotifyAll();
        mAllNodesConnectedCondVar.NotifyAll();
    }
}

Error Launcher::OnAlertReceived(const AlertVariant& alert)
{
    LOG_DBG() << "Alert received" << Log::Field("alert", alert.ApplyVisitor(GetAlertTagVisitor()));

    UniqueLock updateLock {mUpdateMutex};

    if (!alert.ApplyVisitor(ShouldRebalanceVisitor())) {
        return ErrorEnum::eNone;
    }

    mAlertReceived = true;
    mProcessUpdatesCondVar.NotifyAll();

    return ErrorEnum::eNone;
}

void Launcher::SubjectsChanged(const Array<StaticString<cIDLen>>& subjects)
{
    LOG_DBG() << "Subjects changed";

    LockGuard updateLock {mUpdateMutex};

    mNewSubjects.SetValue(subjects);

    mProcessUpdatesCondVar.NotifyAll();
}

} // namespace aos::cm::launcher
