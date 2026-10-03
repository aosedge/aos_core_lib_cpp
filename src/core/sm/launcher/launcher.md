# Launcher

Launcher module aims to launch different types of Aos items, such as Aos services, components etc. This module
is responsible for launching items with respect to runtime dependencies on node start
and supervising items at runtime.

It uses different runtimes to run appropriate update item.

It implements the following interfaces:

* [aos::sm::launcher::LauncherItf](itf/launcher.hpp) - implements main launcher functionality to update Aos items;
* [aos::sm::launcher::InstanceStatusReceiverItf](itf/instancestatusreceiver.hpp) - to receive instances statuses
  from runtimes;
* [aos::sm::launcher::RuntimeInfoProviderItf](itf/runtimeinfoprovider.hpp) - to provide runtime info.
* [aos::monitoring::InstanceInfoProviderItf](../../common/monitoring/itf/instanceinfoprovider.hpp) - provides
  instance info: monitoring parameters, monitoring data, instance statuses. Also, it's capable of managing instance
  status events subscription and notifications and inherits the following interfaces:
  * [aos::instancestatusprovider::ProviderItf](../../common/instancestatusprovider/itf/instancestatusprovider.hpp) -
    providers instance statuses;
  * [aos::monitoring::InstanceParamsProviderItf](../../common/monitoring/itf/instanceparamsprovider.hpp) - provides
    instance monitoring parameters;
  * [aos::monitoring::InstanceMonitoringProviderItf](../../common/monitoring/itf/instancemonitoringprovider.hpp) -
    provides instance monitoring data.

It requires the following interfaces:

* [aos::sm::launcher::RuntimeItf](itf/runtime.hpp) - launch different kind of items on different runtimes, and
  provides instance monitoring data (implements `aos::monitoring::InstanceMonitoringProviderItf` interface);
* [aos::sm::launcher::StorageItf](itf/storage.hpp) - persistently store current instances;
* [aos::sm::launcher::SenderItf](itf/sender.hpp) - sends node and updated instances statuses.

```mermaid
classDiagram
    direction LR

    class Launcher ["aos::sm::launcher::Launcher"] {
    }

    class LauncherItf ["aos::sm::launcher::LauncherItf"] {
        <<interface>>
    }

    class InstanceStatusReceiverItf ["aos::sm::launcher::InstanceStatusReceiverItf"] {
        <<interface>>
    }

    class RuntimeInfoProviderItf ["aos::sm::launcher::RuntimeInfoProviderItf"] {
        <<interface>>
    }

    class InstanceInfoProviderItf ["aos::monitoring::InstanceInfoProviderItf"] {
        <<interface>>
    }

    class InstanceStatusProviderItf["aos::instancestatusprovider::ProviderItf"] {
        <<interface>>
    }

    class InstanceParamsProviderItf["aos::monitoring::InstanceParamsProviderItf"] {
        <<interface>>
    }

    class InstanceMonitoringProviderItf["aos::monitoring::InstanceMonitoringProviderItf"] {
        <<interface>>
    }

    class RuntimeItf ["aos::sm::launcher::RuntimeItf"] {
        <<interface>>
    }

    class SenderItf ["aos::sm::launcher::SenderItf"] {
        <<interface>>
    }

    class StorageItf ["aos::sm::launcher::StorageItf"] {
        <<interface>>
    }

    Launcher ..|> LauncherItf
    Launcher ..|> InstanceStatusReceiverItf
    Launcher ..|> RuntimeInfoProviderItf
    Launcher ..|> InstanceInfoProviderItf
    InstanceInfoProviderItf ..|> InstanceStatusProviderItf
    InstanceInfoProviderItf ..|> InstanceParamsProviderItf
    InstanceInfoProviderItf ..|> InstanceMonitoringProviderItf
    RuntimeItf ..|> InstanceMonitoringProviderItf

    Launcher "1" --> "*" RuntimeItf
    Launcher ..> SenderItf
    Launcher ..> StorageItf
```

## Initialization

On SM start, launcher gets items that should be started on this node from the storage, checks current item status and
starts or stops corresponding item depends on its status. After all items updated, it sends node instances status using
`InstanceStatusSenderItf`.

```mermaid
sequenceDiagram
    participant smclient
    participant launcher
    participant runtimes@{ "type" : "collections" }
    participant storage

    launcher ->> storage: GetAllInstancesInfos
    storage -->> launcher: instances

    loop instances
        launcher -->> launcher: addToCache

        launcher ->> runtimes: StartInstance
        runtimes -->> launcher: eActive
    end

    launcher ->> smclient: SendNodeInstancesStatuses
```

Launcher processes instances in parallel using a thread pool.

## Update instances

On update instances request, launcher checks if previous launch is still in progress. If so, it returns
`eWrongState` error. Otherwise, it stops the requested instances and removes them from storage and local cache.
It then removes unused update items and installs required items, once per item ID and version.
If reading installed items fails, launcher logs the error and attempts to install all required items.

An installation failure marks every dependent instance as `eFailed` with the installation error. These instances
remain in the local cache for status reporting, but are not prepared, persisted, or started. Other items can still
start normally. On a subsequent request, cached `eFailed` instances are prepared again after successful installation.
This retry applies to all failed instances, including failures during preparation or runtime startup.

After preparing eligible instances, launcher starts their networks and runtimes and sends node instances statuses.

```mermaid
sequenceDiagram
    participant smclient
    participant launcher
    participant imagemanager
    participant runtimes@{ "type" : "collections" }
    participant storage

    smclient ->> launcher: UpdateInstances
    alt Previous launch is in progress
        launcher ->> smclient: eWrongState
    else Previous launch completed
        loop Stop instances
            launcher ->> runtimes: StopInstance
            launcher ->> launcher: Stop instance network
            launcher ->> storage: RemoveInstanceInfo (ignore eNotFound)
            launcher ->> launcher: Release network and remove from cache
        end

        launcher ->> imagemanager: Remove unused update items
        launcher ->> imagemanager: GetAllInstalledItems
        loop Required items not known to be installed
            launcher ->> imagemanager: InstallUpdateItem
            imagemanager -->> launcher: Installation result
        end

        loop Start instances
            launcher ->> launcher: Find or add to cache
            alt Item installation failed
                launcher ->> launcher: Set eFailed with installation error
            else Item available
                opt New or previously failed instance
                    launcher ->> storage: UpdateInstanceInfo
                    launcher ->> launcher: Prepare configs and network
                end
                alt Preparation succeeded
                    launcher ->> launcher: Start instance network
                    opt Network startup succeeded
                        launcher ->> runtimes: StartInstance
                        runtimes -->> launcher: status
                    end
                else Preparation failed
                    launcher ->> launcher: Set eFailed
                end
            end
        end
        launcher ->> smclient: SendNodeInstancesStatuses
    end
```

Launcher processes instances in parallel using a thread pool. Installation, preparation, network startup, and
runtime startup are separate stages; each stage completes before the next one starts.

## aos::sm::launcher::InstanceStatusReceiverItf

### OnInstancesStatusesReceived

Receive instances statuses.

### RebootRequired

Marks runtime for reboot. Reboot will be performed after current launch is completed.

## aos::sm::launcher::InstanceStatusSenderItf

### SendNodeInstancesStatuses

Sends node instances statuses once update is completed.

### SendUpdateInstancesStatuses

Propagates updated instances statuses received from runtimes.

## aos::sm::launcher::LauncherItf

Implements main launcher functionality to start and stop node instances.

### UpdateInstances

Start and stop node instances.

## aos::sm::launcher::RebooterItf

### Reboot

Reboots the system.

## aos::sm::launcher::UpdateCheckerItf

### Check

Checks if update succeeded.
