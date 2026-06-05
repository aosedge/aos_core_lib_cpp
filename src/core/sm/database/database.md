# Database

The database module provides persistent storage for various SM modules. It implements storage interfaces for the
following modules:

- [aos::sm::imagemanager::StorageItf](../imagemanager/itf/storage.hpp) - image manager storage interface;
- [aos::sm::launcher::StorageItf](../launcher/itf/storage.hpp) - launcher storage interface.
- [aos::sm::networkmanager::StorageItf](../networkmanager/itf/storage.hpp) - network manager storage interface;

```mermaid
classDiagram
    class DatabaseItf ["aos::sm::database::DatabaseItf"] {
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

    DatabaseItf ..|> ImageManagerStorageItf
    DatabaseItf ..|> LauncherStorageItf
    DatabaseItf ..|> NetworkManagerStorageItf
```
