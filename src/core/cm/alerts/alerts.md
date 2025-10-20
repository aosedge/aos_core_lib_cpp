# Alerts

Alerts module receives alerts from SM's, buffers and aggregates them into suitable for Aos cloud messages.

It implements the following interfaces:

* [aos::cm::alerts::ReceiverItf](itf/receiver.hpp) - receives SM's alerts.

It requires the following interfaces:

* [aos::cm::alerts::SenderItf](itf/sender.hpp) - sends aggregated alert messages to Aos cloud.

```mermaid
classDiagram
    class Alerts ["aos::cm::alerts::Alerts"] {
    }

    class ReceiverItf ["aos::cm::alerts::ReceiverItf"] {
        <<interface>>
    }

    class SenderItf ["aos::cm::alerts::SenderItf"] {
        <<interface>>
    }

    Alerts ..|> ReceiverItf

    Alerts ..> SenderItf
```
