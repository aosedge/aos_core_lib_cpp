# Node config

Implements node config update and provides node config for other modules.

It implements the following interfaces:

* [aos::sm::nodeconfig::NodeConfigHandlerItf](itf/nodeconfighandler.hpp) - updates node config;
* [aos::nodeconfig::NodeConfigProviderItf](../../common/nodeconfig/itf/nodeconfigprovider.hpp) - provides node config
  for other modules.

It uses the following interfaces:

* [aos::nodeconfig::JSONProviderItf](../../common/nodeconfig/itf/jsonprovider.hpp) - converts node config to/from JSON.

```mermaid
classDiagram
    class NodeConfig ["aos::sm:nodeconfig::NodeConfig"] {
    }

    class NodeConfigHandlerItf ["aos::sm::nodeconfig::NodeConfigHandlerItf"] {
        <<interface>>
    }

    class NodeConfigProviderItf ["aos::nodeconfig::NodeConfigProviderItf"] {
        <<interface>>
    }

    class JSONProviderItf ["aos::nodeconfig::JSONProviderItf"] {
        <<interface>>
    }

    NodeConfig ..|> NodeConfigHandlerItf
    NodeConfig ..|> NodeConfigProviderItf

    NodeConfig ..> JSONProviderItf
```
