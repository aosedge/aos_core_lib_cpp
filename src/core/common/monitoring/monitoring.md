# Monitoring

Monitoring provides node and instances monitoring data.

It implements the following interfaces:

* [aos::monitoring::MonitoringItf](itf/monitoring.hpp) - provides average monitoring data.

It uses the following interfaces:

* [aos::monitoring::SenderItf](itf/sender.hpp) - to send monitoring date;
* [aos::monitoring::NodeMonitoringProviderItf](itf/nodemonitoringprovider.hpp) - to retrieve node monitoring data;
* [aos::nodeconfig::NodeConfigProviderItf](../nodeconfig/itf/nodeconfigprovider.hpp) - to get current node config;
* [aos::monitoring::InstanceInfoProviderItf](itf/instanceinfoprovider.hpp) - to retrieve instance monitoring data,
  monitoring params and instance states. This interface is optional and if not provided monitoring module processes and
  sends only node monitoring data. This interface includes the following interfaces:
  * [aos::monitoring::InstanceMonitoringProviderItf](itf/instanceinfoprovider.hpp) - to retrieve instance monitoring
    data;
  * [aos::monitoring::InstanceParamsProviderItf](itf/instanceparamsprovider.hpp) - to get instance monitoring params;
  * [aos::instancestatusprovider::ProviderItf](../instancestatusprovider/itf/instancestatusprovider.hpp) - to monitor
    instance states.
