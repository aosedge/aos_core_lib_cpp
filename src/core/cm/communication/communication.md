# Communication

Communication is responsible for communication between unit and Aos cloud.

It implements the following interfaces:

* [aos::connectionprovider::CloudConnectionItf](../../common/connectionprovider/itf/cloudconnection.hpp) -
  provides cloud connection state to other modules;
* [aos::cm::alerts::SenderItf](../alerts/itf/sender.hpp) - sends alerts to the cloud;
* [aos::cm::monitoring::SenderItf](../monitoring/itf/sender.hpp) - sends monitoring to the cloud;
* [aos::cm::updatemanager::SenderItf](../updatemanager/itf/sender.hpp) - sends unit status to the cloud;
* [aos::cm::storagestate::SenderItf](../storagestate/itf/sender.hpp) - sends instance state request and new state
  notification;
* [aos::cm::smcontroller::SenderItf](../smcontroller/itf/sender.hpp) - sends log messages;
* [aos::cm::launcher::SenderItf](../launcher/itf/senderitf.hpp) - sends override env vars statuses;
* [aos::blobinfoprovider::ProviderItf](../../common/blobinfoprovider/itf/blobinfoprovider.hpp) - provides blobs info.

It requires the following interfaces:

* [aos::cm::updatemanager::UpdateManagerItf](../updatemanager/itf/updatemanager.hpp) - processes desired status received
  from cloud;
* [aos::cm::storagestate::StateHandlerItf](../storagestate/itf/statehandler.hpp) - updates and accepts instance state;
* [aos::cm::smcontroller::LogProviderItf](../smcontroller/itf/logprovider.hpp) - requests logs;
* [aos::cm::launcher::EnvVarHandlerItf](../launcher/itf/envvarhandler.hpp) - overrides instances env vars;
* [aos::common::iamclient::CertHandlerItf](../../common/iamclient/itf/certhandler.hpp) - renews nodes certificates;
* [aos::common::iamclient::ProvisioningItf](../../common/iamclient/itf/provisioning.hpp) - performs node provisioning.

```mermaid
classDiagram
    direction LR

    class Communication["aos::cm::communication::Communication"] {
    }

    class CloudConnectionItf["aos::cloudconnection::CloudConnectionItf"] {
        <<interface>>
    }

    class AlertsSenderItf["aos::cm::alerts::SenderItf"] {
        <<interface>>
    }

    class MonitoringSenderItf["aos::cm::monitoring::SenderItf"] {
        <<interface>>
    }

    class UpdateManagerSenderItf["aos::cm::updatemanager::SenderItf"] {
        <<interface>>
    }

    class StorageStateSenderItf["aos::cm::storagestate::SenderItf"] {
        <<interface>>
    }

    class SMControllerSenderItf["aos::cm::smcontroller::SenderItf"] {
        <<interface>>
    }

    class LauncherSenderItf["aos::cm::launcher::SenderItf"] {
        <<interface>>
    }

    class BlobInfoProviderItf["aos::blobinfoprovider::ProviderItf"] {
        <<interface>>
    }

    class UpdateManagerItf["aos::cm::updatemanager::UpdateManagerItf"] {
        <<interface>>
    }

    class StateHandlerItf["aos::cm::storagestate::StateHandlerItf"] {
        <<interface>>
    }

    class LogProviderItf ["aos::cm::smcontroller::LogProviderItf"] {
        <<interface>>
    }

    class EnvVarHandlerItf ["aos::cm::launcher::EnvVarHandlerItf"] {
        <<interface>>
    }

    class CertHandlerItf ["aos::common::iamclient::CertHandlerItf"] {
        <<interface>>
    }

    class ProvisioningItf ["aos::common::iamclient::ProvisioningItf"] {
        <<interface>>
    }


    Communication ..|> CloudConnectionItf
    Communication ..|> AlertsSenderItf
    Communication ..|> MonitoringSenderItf
    Communication ..|> UpdateManagerSenderItf
    Communication ..|> StorageStateSenderItf
    Communication ..|> SMControllerSenderItf
    Communication ..|> LauncherSenderItf
    Communication ..|> BlobInfoProviderItf

    Communication ..> UpdateManagerItf
    Communication ..> StateHandlerItf
    Communication ..> LogProviderItf
    Communication ..> EnvVarHandlerItf
    Communication ..> CertHandlerItf
    Communication ..> ProvisioningItf
```
