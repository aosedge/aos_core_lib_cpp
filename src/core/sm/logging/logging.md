# Logging

Provides system and instances logs.

It implements the following interfaces:

* [aos::sm::logging::LogProviderItf](itf/logprovider.hpp) - collects and compresses system and instances logs.

It uses the following interfaces:

* [aos::sm::logging::SenderItf](itf/sender.hpp) - sends collected logs to CM.

```mermaid
classDiagram
    class Logging ["aos::sm:logging::Logging"] {
    }

    class LogProviderItf ["aos::sm::logging::LogProviderItf"] {
        <<interface>>
    }

    class SenderItf ["aos::sm::logging::SenderItf"] {
        <<interface>>
    }

    Logging ..|> LogProviderItf

    Logging ..> SenderItf
```
