# Database

The database module provides persistent storage for various SM modules. It implements storage interfaces for the
following modules:

- [aos::common::imagemanager::StorageItf](../../common/imagemanager/itf/storage.hpp) - common image manager storage
  interface, inherited virtually so that the implementation may share it with a common image manager storage base
  class;
- [aos::sm::imagemanager::StorageItf](../imagemanager/itf/storage.hpp) - SM image manager storage interface, which
  extends the common one;
- [aos::sm::launcher::StorageItf](../launcher/itf/storage.hpp) - launcher storage interface.
- [aos::sm::networkmanager::StorageItf](../networkmanager/itf/storage.hpp) - network manager storage interface;

```mermaid
classDiagram
    class DatabaseItf ["aos::sm::database::DatabaseItf"] {
        <<interface>>
    }

    class CommonImageManagerStorageItf ["aos::common::imagemanager::StorageItf"] {
        <<interface>>
    }

    class ImageManagerStorageItf ["aos::sm::imagemanager::StorageItf"] {
        <<interface>>
    }

    class LauncherStorageItf ["aos::sm::launcher::StorageItf"] {
        <<interface>>
    }

    class NetworkManagerStorageItf ["aos::sm::networkmanager::StorageItf"] {
        <<interface>>
    }

    DatabaseItf ..|> CommonImageManagerStorageItf
    DatabaseItf ..|> ImageManagerStorageItf
    ImageManagerStorageItf ..|> CommonImageManagerStorageItf
    DatabaseItf ..|> LauncherStorageItf
    DatabaseItf ..|> NetworkManagerStorageItf
```
