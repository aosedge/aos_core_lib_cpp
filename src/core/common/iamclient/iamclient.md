# IAM client

Defines IAM client interfaces to access IAM functionality.

It defines the following interfaces:

* [aos::iamclient::CertHandlerItf](itf/certhandler.hpp) - handles keys and certificates (renew, provisioning);
* [aos::iamclient::CertProviderItf](itf/certprovider.hpp) - provides info about current keys and certificates;
* [aos::iamclient::CurrentNodeInfoProviderItf](itf/currentnodeinfoprovider.hpp) - provides current node info;
* [aos::iamclient::IdentProviderItf](itf/identprovider.hpp) - provides system identification info;
* [aos::iamclient::NodeHandlerItf](itf/nodehandler.hpp) - handles nodes states (pause, resume);
* [aos::iamclient::NodeInfoProviderItf](itf/nodeinfoprovider.hpp) - provides nodes info;
* [aos::iamclient::PermHandlerItf](itf/permhandler.hpp) - registers/unregisters service instances permissions;
* [aos::iamclient::PermProviderItf](itf/permprovider.hpp) - provides service instances permissions;
* [aos::iamclient::ProvisioningItf](itf/provisioning.hpp) - performs new nod provisioning.
