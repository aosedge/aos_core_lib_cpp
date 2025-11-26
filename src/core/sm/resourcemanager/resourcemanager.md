# Resource manager

Provides resource info for other modules.

It implements the following interfaces:

* [aos::sm::smclient::ResourceInfoProviderItf](../smclient/itf/resourceinfoprovider.hpp) - provides resources info.

```mermaid
classDiagram
    class ResourceManager ["aos::sm:resourcemanager::ResourceManager"] {
    }

    class ResourceInfoProvider ["aos::sm::smclient::ResourceInfoProviderItf"] {
        <<interface>>
    }

    ResourceManager ..|> ResourceInfoProvider
```
