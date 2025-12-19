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

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Error Launcher::Init(const Config& config, StorageItf& storage, nodeinfoprovider::NodeInfoProviderItf& nodeInfoProvider,
    InstanceRunnerItf& runner, imagemanager::ItemInfoProviderItf& itemInfoProvider,
    imagemanager::BlobInfoProviderItf& blobInfoProvider, oci::OCISpecItf& ociSpec,
    unitconfig::NodeConfigProviderItf& nodeConfigProvider, storagestate::StorageStateItf& storageState,
    networkmanager::NetworkManagerItf& networkManager, MonitoringProviderItf& monitorProvider,
    alerts::AlertsProviderItf& alertsProvider, iamclient::IdentProviderItf& identProvider, IDValidator gidValidator,
    IDValidator uidValidator)
{
    LOG_DBG() << "Init Launcher";

    mConfig             = config;
    mStorage            = &storage;
    mNodeInfoProvider   = &nodeInfoProvider;
    mRunner             = &runner;
    mNodeConfigProvider = &nodeConfigProvider;
    mStorageState       = &storageState;
    mNetworkManager     = &networkManager;
    mMonitorProvider    = &monitorProvider;
    mAlertsProvider     = &alertsProvider;
    mIdentProvider      = &identProvider;

    auto err = mInstanceManager.Init(
        config, storage, storageState, itemInfoProvider, blobInfoProvider, ociSpec, gidValidator, uidValidator);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mNodeManager.Init(*mNodeInfoProvider, *mNodeConfigProvider, *mStorageState, *mRunner);
    mBalancer.Init(mInstanceManager, itemInfoProvider, blobInfoProvider, ociSpec, mNodeManager, *mMonitorProvider,
        *mRunner, *mNetworkManager);

    return ErrorEnum::eNone;
}

Error Launcher::Start()
{
    LOG_DBG() << "Start Launcher";

    LockGuard lock {mMutex};

    mIsRunning = true;

    // Start managers.
    if (auto err = mInstanceManager.Start(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager.Start(); !err.IsNone()) {
        return err;
    }

    if (auto err = mNodeManager.LoadSentInstances(mInstanceManager.GetActiveInstances()); !err.IsNone()) {
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

    if (auto [_, err] = mBalancer.SetSubjects(*subjects); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    // Notify status listeners.
    NotifyInstanceStatusListeners(mInstanceStatuses);

    // Start monitoring thread.
    mDisableProcessUpdates = false;
    mUpdatedNodes.Clear();
    mNewSubjects.SetValue(*subjects); // Check subjects after startup.

    if (auto err = mWorkerThread.Run([this](void*) { ProcessUpdate(); }); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::Stop()
{
    LOG_DBG() << "Stop Launcher";

    UniqueLock lock {mMutex};

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

    // Finish monitoring thread.
    mUpdatedNodes.Clear();
    mIsRunning             = false;
    mDisableProcessUpdates = false;
    mProcessUpdatesCondVar.NotifyAll();

    lock.Unlock();

    if (auto err = mWorkerThread.Join(); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::RunInstances(const Array<RunInstanceRequest>& requests, Array<InstanceStatus>& statuses)
{
    LOG_DBG() << "Run instances";

    UniqueLock lock {mMutex};

    // Disable node monitoring for the duration of running instances.
    mDisableProcessUpdates    = true;
    auto enableNodeMonitoring = DeferRelease(this, [](Launcher* self) {
        self->mDisableProcessUpdates = false;
        self->mProcessUpdatesCondVar.NotifyAll();
    });

    statuses.Clear();

    if (auto err = mRunRequests.Assign(requests); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    UpdateData(false);

    auto runErr = mBalancer.RunInstances(mRunRequests, lock, false);

    FailActivatingInstances();

    if (!runErr.IsNone()) {
        return AOS_ERROR_WRAP(runErr);
    }

    NotifyInstanceStatusListeners(statuses);

    return ErrorEnum::eNone;
}

Error Launcher::GetInstancesStatuses(Array<InstanceStatus>& statuses)
{
    LockGuard lock {mMutex};

    statuses.Clear();

    for (const auto& instance : mInstanceManager.GetActiveInstances()) {
        if (auto err = statuses.PushBack(instance->GetStatus()); !err.IsNone()) {
            LOG_ERR() << "Failed to push new instance status" << Log::Field(AOS_ERROR_WRAP(err));

            break;
        }
    }

    return ErrorEnum::eNone;
}

Error Launcher::SubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    LockGuard lock {mMutex};

    LOG_DBG() << "Subscribe instance status listener";

    if (auto err = mInstanceStatusListeners.PushBack(&listener); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    return ErrorEnum::eNone;
}

Error Launcher::UnsubscribeListener(instancestatusprovider::ListenerItf& listener)
{
    LockGuard lock {mMutex};

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

void Launcher::NotifyInstanceStatusListeners(Array<InstanceStatus>& statuses)
{
    statuses.Clear();

    for (const auto& instance : mInstanceManager.GetActiveInstances()) {
        if (auto err = statuses.PushBack(instance->GetStatus()); !err.IsNone()) {
            LOG_ERR() << "Failed to push new instance status" << Log::Field(AOS_ERROR_WRAP(err));

            break;
        }
    }

    for (auto& listener : mInstanceStatusListeners) {
        listener->OnInstancesStatusesChanged(statuses);
    }
}

void Launcher::UpdateData(bool rebalancing)
{
    for (auto& node : mNodeManager.GetNodes()) {
        const auto& nodeID = node.GetInfo().mNodeID;

        auto nodeMonitoring = MakeUnique<monitoring::NodeMonitoringData>(&mAllocator);

        if (auto err = mMonitorProvider->GetAverageMonitoring(nodeID, *nodeMonitoring); !err.IsNone()) {
            mInstanceManager.UpdateMonitoringData(nodeMonitoring->mInstances);
        }

        node.UpdateAvailableResources(*nodeMonitoring, rebalancing);
        node.UpdateConfig();
    }
}

void Launcher::FailActivatingInstances()
{
    for (auto& instance : mInstanceManager.GetActiveInstances()) {
        if (instance->GetStatus().mState == aos::InstanceStateEnum::eActivating) {
            instance->SetError(AOS_ERROR_WRAP(ErrorEnum::eTimeout));
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

    UpdateData(true);

    auto runErr = mBalancer.RunInstances(mRunRequests, lock, false);

    FailActivatingInstances();

    if (!runErr.IsNone()) {
        return AOS_ERROR_WRAP(runErr);
    }

    NotifyInstanceStatusListeners(mInstanceStatuses);

    return ErrorEnum::eNone;
}

void Launcher::ProcessUpdate()
{
    while (true) {
        UniqueLock lock {mMutex};

        mProcessUpdatesCondVar.Wait(lock, [this]() {
            // Wake up on:
            // - node updates (need resend),
            // - subject updates (need rebalance),
            // - stop request.
            return (!mUpdatedNodes.IsEmpty() || mNewSubjects.HasValue() || !mIsRunning) && !mDisableProcessUpdates;
        });

        if (!mIsRunning) {
            return;
        }

        // Update subjects.
        Error err;
        bool  doRebalance = false;

        {
            LockGuard subjectsLock {mNewSubjectsMutex};

            if (mNewSubjects.HasValue()) {
                Tie(doRebalance, err) = mBalancer.SetSubjects(mNewSubjects.GetValue());
                if (!err.IsNone()) {
                    LOG_ERR() << "Failed to set subjects" << Log::Field(AOS_ERROR_WRAP(err));
                }

                mNewSubjects.Reset();
            }
        }

        // Resend instances.
        if (!mUpdatedNodes.IsEmpty()) {
            if (!doRebalance) {
                if (err = mNodeManager.ResendInstances(lock, mUpdatedNodes); !err.IsNone()) {
                    LOG_ERR() << "Failed to resend instances" << Log::Field(AOS_ERROR_WRAP(err));
                }
            } else {
                LOG_INF() << "Rebalancing will be performed, skip resending instances";
            }

            mUpdatedNodes.Clear();
        }

        // Rebalance.
        if (doRebalance) {
            if (err = Rebalance(lock); !err.IsNone()) {
                LOG_ERR() << "Rebalancing failed" << Log::Field(AOS_ERROR_WRAP(err));
            }
        }
    }
}

Error Launcher::OnInstanceStatusReceived(const InstanceStatus& status)
{
    LOG_INF() << "Instance status received" << Log::Field("instance", static_cast<const InstanceIdent&>(status));

    LockGuard lock {mMutex};

    if (auto err = mInstanceManager.UpdateStatus(status); !err.IsNone()) {
        return err;
    }

    NotifyInstanceStatusListeners(mInstanceStatuses);

    return ErrorEnum::eNone;
}

Error Launcher::OnNodeInstancesStatusesReceived(const String& nodeID, const Array<InstanceStatus>& statuses)
{
    LOG_INF() << "Node instances statuses received" << Log::Field("nodeID", nodeID);

    LockGuard lock {mMutex};

    Error firstErr;

    // Update instance manager.
    for (const auto& status : statuses) {
        if (auto err = mInstanceManager.UpdateStatus(status); !err.IsNone()) {
            if (firstErr.IsNone()) {
                firstErr = err;
            }
        }
    }

    // Update node manager.
    if (auto err = mNodeManager.UpdateRunnigInstances(nodeID, statuses); !err.IsNone()) {
        if (firstErr.IsNone()) {
            firstErr = err;
        }
    }

    if (!firstErr.IsNone()) {
        return firstErr;
    }

    NotifyInstanceStatusListeners(mInstanceStatuses);

    // Notify nodes monitor.
    if (auto err = mUpdatedNodes.PushBack(nodeID); !err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    mProcessUpdatesCondVar.NotifyAll();

    return ErrorEnum::eNone;
}

void Launcher::OnNodeInfoChanged(const UnitNodeInfo& info)
{
    LOG_DBG() << "Node info changed" << Log::Field("nodeID", info.mNodeID);

    UniqueLock lock {mMutex};

    if (mNodeManager.UpdateNodeInfo(info)) {
        if (auto err = Rebalance(lock); !err.IsNone()) {
            LOG_ERR() << "Rebalance" << Log::Field(AOS_ERROR_WRAP(err));
        }
    }
}

Error Launcher::OnAlertReceived(const AlertVariant& alert)
{
    LOG_DBG() << "Alert received" << Log::Field("alert", alert.ApplyVisitor(GetAlertTagVisitor()));

    UniqueLock lock {mMutex};

    if (!alert.ApplyVisitor(ShouldRebalanceVisitor())) {
        return ErrorEnum::eNone;
    }

    return Rebalance(lock);
}

void Launcher::SubjectsChanged(const Array<StaticString<cIDLen>>& subjects)
{
    LOG_DBG() << "Subjects changed";

    UniqueLock lock {mNewSubjectsMutex};

    mNewSubjects.SetValue(subjects);

    mProcessUpdatesCondVar.NotifyAll();
}

} // namespace aos::cm::launcher
