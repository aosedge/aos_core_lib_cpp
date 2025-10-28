# Node info provider

Provides node information. Since node information is collected from two sources: IAM and SM, this module collects,
combines and provides combined node information for other CM modules.

It implements the following interfaces:

* [aos::cm::nodeinfoprovider::NodeInfoProviderItf]() - provides combined node info for other modules;
* [aos::cm::smcontroller::SMInfoReceiverItf]() - receives SM statuses.

It requires the following interfaces:

* [aos::cm::iamclient::NodeInfoProviderItf]() - retrieves IAM nodes information.

```mermaid
classDiagram
    direction LR

    class NodeInfoProvider["aos::cm::nodeinfoprovider::NodeInfoProvider"] {
    }

    class NodeInfoProviderItf["aos::cm::nodeinfoprovider::NodeInfoProviderItf"] {
        <<interface>>
    }

    class SMInfoReceiverItf ["aos::cm::smstatusreceiver::SMInfoReceiverItf"] {
        <<interface>>
    }

    class IAMNodeInfoProviderItf ["aos::cm::iamclient::NodeInfoProviderItf"] {
        <<interface>>
    }

    NodeInfoProvider ..|> NodeInfoProviderItf
    NodeInfoProvider ..|> SMInfoReceiverItf

    NodeInfoProvider ..> IAMNodeInfoProviderItf
```

## Workflow

### Combining node info and SM info

At initialization `nodeinfoprovider` gets all nodes infos from IAM and caches them internally. When new SM info
received, it updates runtime and resource infos for the corresponding node infoo.

`nodeinfoprovider` may override node state received from IAM node info if this node has SM Aos service:

* if node state in IAM node info is `online` or `paused` and corresponding SM is not connected and SM info is not
  received then combined node state is set to `offline`;

* if node state in IAM node info is `online` or `paused` and corresponding SM is not connected or SM info is not
  received during configurable timeout then combined node state is set to `error`. It is changed to IAM node info state
  once the corresponding SM is connected and SM info is received.

```mermaid
sequenceDiagram
    nodeinfoprovider ->> iamclient: GetAllNodeIDs
    iamclient -->> nodeinfoprovider: node ID's

    nodeinfoprovider ->> iamclient: SubscribeListener
    iamclient -->> nodeinfoprovider: OK

    loop node ID's
        nodeinfoprovider ->> iamclient: GetNodeInfo(state = online || paused)
        iamclient -->> nodeinfoprovider: node info

        nodeinfoprovider ->> nodeinfoprovider: state = offline
    end

    alt Receive SM info
        smcontroller -->> nodeinfoprovider: OnSMConnected
        smcontroller -->> nodeinfoprovider: OnSMInfoReceived

        nodeinfoprovider ->> nodeinfoprovider: state = online || paused

        alt SM connection lost
            smcontroller -->> nodeinfoprovider: OnSMDisconnected
            nodeinfoprovider ->> nodeinfoprovider: state = error

            smcontroller -->> nodeinfoprovider: OnSMDConnected
            nodeinfoprovider ->> nodeinfoprovider: state = online || paused
        end
    else
        nodeinfoprovider ->> nodeinfoprovider: wait SM info timeout
        nodeinfoprovider ->> nodeinfoprovider: state = error

        alt SM connected
            smcontroller -->> nodeinfoprovider: OnSMConnected
            smcontroller -->> nodeinfoprovider: OnSMInfoReceived
            nodeinfoprovider ->> nodeinfoprovider: state = online || paused
        end
    end
```

## aos::cm::nodeinfoprovider::nodeinfoprovider

### GetAllNodeIDs

Returns all available node ID's.

### GetNodeInfo

Returns node info of specified node.

### SubscribeListener

Subscribes to node info change.

### UnsubscribeListener

Unsubscribes from node info change.

## aos::cm::smcontroller::SMInfoReceiverItf

## OnSMConnected

This method is called when corresponding SM connects to CM.

## OnSMDisconnected

This method is called when corresponding SM connects to CM.

## OnSMInfoReceived

This method is called when SM info is received.
