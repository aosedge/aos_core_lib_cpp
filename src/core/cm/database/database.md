# Database

The database module provides persistent storage for various CM modules. It implements storage interfaces for the
following modules:

- [aos::cm::storagestate::StorageItf](../storagestate/itf/storage.hpp) - storage state storage interface;
- [aos::cm::imagemanager::StorageItf](../imagemanager/itf/storage.hpp) - image manager storage interface;
- [aos::cm::launcher::StorageItf](../launcher/itf/storage.hpp) - launcher storage interface.

```mermaid
classDiagram
    class DatabaseItf ["aos::cm::database::DatabaseItf"] {
        <<interface>>
    }

    class StorageStateStorageItf ["aos::cm::storagestate::StorageItf"] {
        <<interface>>
    }

    class ImageManagerStorageItf ["aos::cm::imagemanager::StorageItf"] {
        <<interface>>
    }

    class LauncherStorageItf ["aos::cm::launcher::StorageItf"] {
        <<interface>>
    }

    DatabaseItf ..|> StorageStateStorageItf
    DatabaseItf ..|> ImageManagerStorageItf
    DatabaseItf ..|> LauncherStorageItf
```
