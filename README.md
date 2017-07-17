# cloudproviders integration API

libcloudproviders is a DBus API that allows cloud storage sync clients to expose
their services. Clients such as file managers and desktop environments can then
provide integrated access to the cloud providers services.

## cloudproviderd

cloudproviderd is a daemon which keeps track of existing cloud providers and their accounts.
It provides the org.freedesktop.CloudProviderManager DBus object to interact with.

## libcloudprovider

libcloudproviders is a glib based library that makes it easier to implement
a cloud provider server or client.

## Implementing support for sync clients

Any cloud provider clients need to own a DBus name and export a
org.freedesktop.DBus.ObjectManager object to it. For each account the cloud
provider needs to export an individual object implementing the
org.freedesktop.CloudProvider.Account1 interface and register it with the
object manager.

An example implementation using libcloudprovider can be found in
test/testcloudproviderserver.c.

## Implementing support for integrators

Integrators should connect via to the org.freedesktop.CloudProviderManager object
and call the GetCloudProviders method on the org.freedesktop.CloudProvider.Manager1
interface. That method will return a tuple of the registered cloud providers
bus name and the object path of their object manager.

With the bus name and object path they can query the object manager for a list
of accounts and can then interact with these though the
org.freedesktop.CloudProvider.Account1 interface.

An example implementation using libcloudprovider can be found in
test/testcloudproviderclient.c.
