/* cloudprovidersexporter.c
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 * Copyright (C) 2017 Julius Haertl <jus@bitgrid.net>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cloudprovidersproviderexporter.h"
#include "cloudprovidersaccountexporterpriv.h"
#include "cloudprovidersaccountexporter.h"
#include "cloudproviders-generated.h"
#include <gio/gio.h>

struct _CloudProvidersProviderExporter
{
  GObject parent;

  CloudProvidersDbusProvider *skeleton;
  GDBusConnection *bus;
  GDBusObjectManagerServer *manager;
  gchar *manager_bus_name;
  gchar *manager_bus_path;

  gchar *bus_name;
  gchar *bus_path;
  gchar *provider_bus_path;

  gchar *name;
  GList *accounts;
  GVariant *dbus_accounts;
};

G_DEFINE_TYPE (CloudProvidersProviderExporter, cloud_providers_provider_exporter, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_NAME,
    PROP_BUS_NAME,
    PROP_BUS_PATH,
    PROP_BUS,
    N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * SECTION:cloudprovidersexporter
 * @title: CloudProvidersProviderExporter
 * @short_description: Base object for representing a single provider
 * @include: src/cloudprovidersexporter.h
 *
 * #CloudProvidersProviderExporter is the base object representing a single cloud provider.
 * Multiple #CloudProvidersAccountExporter objects can be added with cloud_providers_provider_exporter_add_account()
 * After adding the initial set of accounts cloud_providers_provider_exporter_export_objects() must be called.
 */

#if 0
static void
update_accounts_in_dbus (CloudProvidersProviderExporter *self)
{
    return;
    g_autoptr (GVariantBuilder) accounts_dbus_objects;
    GVariant *accounts_variant;
    GList *l;

    accounts_dbus_objects = g_variant_builder_new (G_VARIANT_TYPE("a(so)"));
    for (l = self->accounts; l != NULL; l = l->next)
    {
        const gchar *object_path;

        object_path = cloud_providers_account_exporter_get_object_path (CLOUD_PROVIDERS_ACCOUNT_EXPORTER (l->data));
        g_variant_builder_add(accounts_dbus_objects, "(so)", self->provider_bus_name, object_path);
    }

   accounts_variant = g_variant_builder_end (accounts_dbus_objects);

   g_autofree gchar *provider_debug = g_variant_print(accounts_variant, TRUE);
   g_debug("Update accounts in dbus: %s\n", provider_debug);

   cloud_providers_dbus_provider_set_accounts (self->skeleton, accounts_variant);
}
#endif

static void
export_provider (CloudProvidersProviderExporter *self)
{
    CloudProvidersDbusObjectSkeleton *provider_object_skeleton;

    provider_object_skeleton = cloud_providers_dbus_object_skeleton_new (self->provider_bus_path);
    cloud_providers_dbus_object_skeleton_set_provider (provider_object_skeleton, self->skeleton);
    g_dbus_object_manager_server_export (self->manager,
                                         G_DBUS_OBJECT_SKELETON (provider_object_skeleton));

    g_debug ("provider object path: %s %s\n", self->provider_bus_path,
             g_dbus_object_manager_get_object_path (G_DBUS_OBJECT_MANAGER (self->manager)));
}

static void
export_account (CloudProvidersProviderExporter *self,
                CloudProvidersAccountExporter  *account)
{
    CloudProvidersDbusAccount *account_skeleton;
    const gchar *account_object_path;
    CloudProvidersDbusObjectSkeleton *account_object_skeleton;

    account_object_path = cloud_providers_account_exporter_get_object_path (account);
    account_skeleton = cloud_providers_account_exporter_get_skeleton (account);
    account_object_skeleton = cloud_providers_dbus_object_skeleton_new (account_object_path);
    cloud_providers_dbus_object_skeleton_set_account (account_object_skeleton, account_skeleton);
    g_dbus_object_manager_server_export (self->manager, G_DBUS_OBJECT_SKELETON (account_object_skeleton));
    g_debug ("account object path: %s %s\n", account_object_path,
             g_dbus_object_manager_get_object_path (G_DBUS_OBJECT_MANAGER (self->manager)));
}

static void
unexport_account(CloudProvidersProviderExporter *self,
                 CloudProvidersAccountExporter  *account)
{
    const gchar *object_path;
    CloudProvidersDbusAccount *account_skeleton;

    account_skeleton = cloud_providers_account_exporter_get_skeleton (account);
    object_path = g_dbus_object_get_object_path (G_DBUS_OBJECT (account_skeleton));
    g_dbus_object_manager_server_unexport (self->manager, object_path);
}

/**
 * cloud_providers_provider_exporter_add_account:
 * @self: The cloud provider exporter
 * @account: The account object
 *
 * Each cloud provider can have a variety of account associated with it. Use this
 * function to add the accounts the user set up. Calling cloud_providers_provider_exporter_export_objects()
 * once is required after adding the initial set of accounts.
 */
void
cloud_providers_provider_exporter_add_account (CloudProvidersProviderExporter *self,
                                               CloudProvidersAccountExporter  *account)
{
  export_account (self, account);
  //update_accounts_in_dbus (self);
  self->accounts = g_list_append (self->accounts, g_object_ref (account));
}

/**
 * cloud_providers_provider_exporter_remove_account:
 * @self: The cloud provider exporter
 * @account: The account object
 *
 * Each cloud provider can have a variety of account associated with it. Use this
 * function to remove the accounts added by cloud_providers_provider_exporter_add_account().
 */
void
cloud_providers_provider_exporter_remove_account (CloudProvidersProviderExporter *self,
                                                  CloudProvidersAccountExporter  *account)
{
    GList *removed_account;

    unexport_account (self, account);
    removed_account = g_list_find (self->accounts, account);
    g_return_if_fail (removed_account != NULL);
    self->accounts = g_list_remove (self->accounts, removed_account);
    g_object_unref (account);
}

#if 0

/**
 * cloud_providers_provider_exporter_export_objects:
 * @self: The cloud provider
 *
 * Export all objects assigned previously with cloud_providers_provider_exporter_add_account()
 * to DBUS.
 * Use this function after adding all the required objects to avoid multiple signals
 * being emitted in a short time. This function needs to be called only once.
 * Objects added after the call will be propagated to DBus automatically.
 */
static void
cloud_providers_provider_exporter_export_objects(CloudProvidersProviderExporter *self)
{
}

#endif

static void
cloud_providers_provider_exporter_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
    CloudProvidersProviderExporter *self = CLOUD_PROVIDERS_PROVIDER_EXPORTER (object);

    switch (prop_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, self->name);
        }
        break;

        case PROP_BUS_NAME:
        {
            g_value_set_string (value, self->bus_name);
        }
        break;

        case PROP_BUS_PATH:
        {
            g_value_set_string (value, self->bus_path);
        }
        break;

        case PROP_BUS:
        {
            g_value_set_object (value, self->bus);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }

    }
}

static void
cloud_providers_provider_exporter_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
    CloudProvidersProviderExporter *self = CLOUD_PROVIDERS_PROVIDER_EXPORTER (object);

    switch (prop_id)
    {
        case PROP_NAME:
        {
            g_free (self->name);
            self->name = g_strdup (g_value_get_string (value));
            g_debug ("setting name %s\n", self->name);
            cloud_providers_dbus_provider_set_name (self->skeleton, self->name);
        }
        break;

        case PROP_BUS_NAME:
        {
            g_return_if_fail (self->bus_name == NULL);
            self->bus_name = g_strdup (g_value_get_string (value));
        }
        break;

        case PROP_BUS_PATH:
        {
            g_return_if_fail (self->bus_path == NULL);
            self->bus_path = g_strdup (g_value_get_string (value));
        }
        break;

        case PROP_BUS:
        {
            g_return_if_fail (self->bus == NULL);
            self->bus = g_object_ref (g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
cloud_providers_provider_exporter_finalize (GObject *object)
{
  CloudProvidersProviderExporter *self = (CloudProvidersProviderExporter *)object;

    g_debug ("finalize provider");
  g_clear_object (&self->skeleton);
  g_clear_object (&self->bus);
  g_free (self->bus_name);
  g_free (self->bus_path);
  g_free (self->provider_bus_path);
  g_clear_object (&self->manager);
  g_free (self->name);

  g_list_free_full (self->accounts, g_object_unref);

  G_OBJECT_CLASS (cloud_providers_provider_exporter_parent_class)->finalize (object);
}

static void
cloud_providers_provider_exporter_constructed (GObject *object)
{
    CloudProvidersProviderExporter *self = CLOUD_PROVIDERS_PROVIDER_EXPORTER (object);

    self->manager = g_dbus_object_manager_server_new (self->bus_path);
    self->provider_bus_path = g_strconcat (self->bus_path, "/Provider", NULL);
    g_debug ("constructed, manager %s", self->bus_path);
    self->skeleton = cloud_providers_dbus_provider_skeleton_new ();
    g_dbus_object_manager_server_set_connection (self->manager, self->bus);
    export_provider (self);
#if 0
    g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                      self->bus, self->bus_path, NULL);
#endif
}

static void
cloud_providers_provider_exporter_init (CloudProvidersProviderExporter *self)
{
}

static void
cloud_providers_provider_exporter_class_init (CloudProvidersProviderExporterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cloud_providers_provider_exporter_set_property;
  object_class->get_property = cloud_providers_provider_exporter_get_property;
  object_class->constructed = cloud_providers_provider_exporter_constructed;
  object_class->finalize = cloud_providers_provider_exporter_finalize;

    properties [PROP_NAME] =
        g_param_spec_string ("name",
                             "Name",
                             "The name of the cloud provider",
                             NULL,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (object_class, PROP_NAME,
                                     properties [PROP_NAME]);

    properties [PROP_BUS_NAME] =
        g_param_spec_string ("bus-name",
                             "BusName",
                             "BusName",
                             NULL,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS |
                              G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (object_class, PROP_BUS_NAME,
                                     properties [PROP_BUS_NAME]);

    properties [PROP_BUS_PATH] =
        g_param_spec_string ("bus-path",
                             "BusPath",
                             "BusPath",
                             NULL,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS |
                              G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (object_class, PROP_BUS_PATH,
                                     properties [PROP_BUS_PATH]);

    properties [PROP_BUS] =
        g_param_spec_object ("bus",
                             "Bus",
                             "Bus",
                             G_TYPE_DBUS_CONNECTION,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS |
                              G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (object_class, PROP_BUS,
                                     properties [PROP_BUS]);
}

void
cloud_providers_provider_exporter_set_name (CloudProvidersProviderExporter *self,
                                            const gchar                    *name)
{
    g_object_set (self, "name", name, NULL);
}

const gchar*
cloud_providers_provider_exporter_get_name (CloudProvidersProviderExporter *self)
{
    return self->name;
}

/**
 * cloud_providers_provider_exporter_new:
 * @bus: A #GDbusConnection to export the objects to
 * @bus_name: A DBus name to bind to
 * @bus_path: A DBus object path
 */
CloudProvidersProviderExporter *
cloud_providers_provider_exporter_new (GDBusConnection *bus,
                                       const gchar     *bus_name,
                                       const gchar     *bus_path)
{
  CloudProvidersProviderExporter *self;

  self = g_object_new (CLOUD_PROVIDERS_TYPE_PROVIDER_EXPORTER,
                       "bus", bus,
                       "bus-name", bus_name,
                       "bus-path", bus_path,
                       NULL);

  return self;
}

GDBusConnection*
cloud_providers_provider_exporter_get_bus (CloudProvidersProviderExporter *self)
{
    return self->bus;
}

const gchar*
cloud_providers_provider_exporter_get_object_path (CloudProvidersProviderExporter *self)
{
    return self->bus_path;
}
    
