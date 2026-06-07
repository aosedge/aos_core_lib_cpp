# Network Manager

This document describes the network manager interfaces in the AOS Core Library common.

## Overview

The network manager provides interfaces for network resource management between CM (Cloud Manager) and
SM (Service Manager). It defines the communication contract for allocating/releasing network resources
and handling deferred firewall rule updates.

These interfaces are placed in `common` because they are used by both CM and SM:

- **CM side**: CM NetworkManager implements `NetworkProviderItf` to serve network allocation requests.
  CM SMController implements `PendingUpdateHandlerItf` to push resolved firewall updates to SM via gRPC stream.
- **SM side**: SM Client uses `NetworkProviderItf` to call CM for network allocation.
  SM NetworkManager implements `PendingUpdateHandlerItf` to receive and apply resolved firewall rules.

## Interface Descriptions

### NetworkProviderItf

Network provider interface used by SM to communicate with CM for network resource management.

**Methods:**

- `GetNodeNetworkParams(networkID, nodeID, result)`: Gets node network parameters (subnet, IP, VLAN) from CM.
- `AllocateInstanceNetwork(instance, networkID, nodeID, serviceData, result)`: Allocates network resources for an
  instance on CM. Returns allocated IP, DNS servers, and firewall rules. If dependent instances are not yet available,
  partial firewall rules are returned (deferred firewall).
- `ReleaseInstanceNetwork(instance, nodeID)`: Releases instance network resources on CM.
- `ReleaseNodeNetwork(networkID, nodeID)`: Releases node-level network resources on CM.

### PendingUpdateHandlerItf

Handler interface for deferred firewall update notifications. Used on both CM and SM sides:

- **CM side**: SMController implements this to push resolved pending firewall rules to SM via gRPC stream.
- **SM side**: SM NetworkManager implements this to receive and apply resolved firewall rules.

**Methods:**

- `OnPendingFirewallUpdate(nodeID, update)`: Called when pending firewall rules are resolved for an instance.
  The update contains the instance identifier and the resolved firewall rules.

## Deferred Firewall Rules

When an instance has `allowedConnections` to another instance that is not yet allocated, CM returns partial firewall
rules (without the missing dependency). CM tracks these as pending connections. When the dependency instance is later
allocated, CM resolves the pending rules and pushes them via `PendingUpdateHandlerItf::OnPendingFirewallUpdate`.
SM then applies only the firewall CNI plugin without restarting other network plugins.
