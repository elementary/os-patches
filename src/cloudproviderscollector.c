/* cloudproviders.c
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

#include "cloudproviderscollector.h"
#include "cloudprovidersaccount.h"
#include "cloudprovidersprovider.h"
#include "cloudproviders-generated.h"
#include <glib.h>
#include <glib/gprintf.h>
#include <gio/gio.h>

#define KEY_FILE_GROUP "Cloud Providers"

struct _CloudProvidersCollector
{
    GObject parent;

    GList *providers;
    GHashTable* provider_object_managers;
    GDBusConnection *bus;
    GCancellable *cancellable;
    GList *monitors;
};

G_DEFINE_TYPE (CloudProvidersCollector, cloud_providers_collector, G_TYPE_OBJECT)

/**
 * SECTION:cloudproviderscollector
 * @title: CloudProvidersCollector
 * @short_description: Singleton for tracking all providers.
 * @include: src/cloudproviders.h
 *
 * #CloudProvidersCollector is a singleton to track all the changes in all providers.
 * Using a #CloudProvidersCollector you can implement integration for all of them at once
 * and represent them in the UI, track new providers added or removed and their
 * status.
 */

enum
{
  PROVIDERS_CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static void
update_cloud_providers (CloudProvidersCollector *self);

static void
on_bus_acquired (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GError *error = NULL;
  CloudProvidersCollector *self;
  GDBusConnection *bus;

  bus = g_bus_get_finish (res, &error);
  if (error != NULL)
    {
      if (error->code != G_IO_ERROR_CANCELLED)
        g_warning ("Error acdquiring bus for cloud provider %s", error->message);
      return;
    }

  self = CLOUD_PROVIDERS_COLLECTOR (user_data);
  self->bus = bus;
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  update_cloud_providers (self);
}

static void
cloud_providers_collector_finalize (GObject *object)
{
    CloudProvidersCollector *self = (CloudProvidersCollector *)object;
    GList *l;

    g_cancellable_cancel (self->cancellable);
    for (l = self->monitors; l != NULL; l = l->next)
    {
        g_signal_handlers_disconnect_by_data (G_OBJECT (l->data), self);
    }
    g_list_free_full (self->providers, g_object_unref);
    g_list_free_full (self->monitors, g_object_unref);

    G_OBJECT_CLASS (cloud_providers_collector_parent_class)->finalize (object);
}

static void
cloud_providers_collector_class_init (CloudProvidersCollectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cloud_providers_collector_finalize;

  /**
   * CloudProviderCollector::providers-changed
   *
   * This signal is emmited by the ammount of providers changed.
   */
  signals [PROVIDERS_CHANGED] = g_signal_new ("providers-changed",
                                              G_TYPE_FROM_CLASS (klass),
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL,
                                              NULL,
                                              g_cclosure_marshal_generic,
                                              G_TYPE_NONE,
                                              0);
}

static void
cloud_providers_collector_init (CloudProvidersCollector *self)
{
}

/**
 * cloud_providers_collector_get_providers
 * @self: A CloudProvidersCollector
 * Returns: (transfer none): A GList* of #CloudProvidersAccount objects.
 */
GList*
cloud_providers_collector_get_providers (CloudProvidersCollector *self)
{
  return self->providers;
}

static void
load_cloud_provider (CloudProvidersCollector *self,
                     GFile                   *file)
{
    GKeyFile *key_file;
    gchar *path;
    GError *error = NULL;
    gchar *bus_name;
    gchar *object_path;
    gboolean success = FALSE;
    CloudProvidersProvider *provider;

    key_file = g_key_file_new ();
    path = g_file_get_path (file);
    g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, &error);
    if (error != NULL)
    {
        g_debug ("test 1");
        goto out;
    }

    if (!g_key_file_has_group (key_file, KEY_FILE_GROUP))
    {
        g_debug ("test 2");
        goto out;
    }

    bus_name = g_key_file_get_string (key_file, KEY_FILE_GROUP, "BusName", &error);
    if (error != NULL)
    {
        g_debug ("test 3");
        goto out;
    }
    object_path = g_key_file_get_string (key_file, KEY_FILE_GROUP, "ObjectPath", &error);
    if (error != NULL)
    {
        g_debug ("test 4");
        goto out;
    }

    provider = cloud_providers_provider_new (bus_name, object_path);
    self->providers = g_list_append (self->providers, provider);

    g_debug("Client loading provider: %s %s\n", bus_name, object_path);

    success = TRUE;
    g_free (bus_name);
    g_free (object_path);
out:
    if (!success)
    {
        g_warning ("Error while loading cloud provider key file at %s with error %s", path, error != NULL ? error->message : NULL);
    }
    g_key_file_free (key_file);
    g_object_unref (file);
    g_free (path);
}

static void
on_providers_file_changed (CloudProvidersCollector *self)
{
    update_cloud_providers (self);
}

static void
load_cloud_providers (CloudProvidersCollector *self)
{
    const gchar* const *data_dirs;
    gint i;
    gint len;
    gchar *key_files_directory_path;
    GFile *key_files_directory_file;
    GError *error = NULL;
    GFileEnumerator *file_enumerator;

    data_dirs = g_get_system_data_dirs ();
    len = g_strv_length ((gchar **)data_dirs);
    for (i = 0; i < len; i++)
    {
        GFileInfo *info;
        GFileMonitor *monitor;

        key_files_directory_path = g_build_filename (data_dirs[i], "cloud-providers", NULL);
        key_files_directory_file = g_file_new_for_path (key_files_directory_path);
        monitor = g_file_monitor (key_files_directory_file, G_FILE_MONITOR_WATCH_MOVES,
                                  self->cancellable, NULL);
        g_signal_connect_swapped (monitor, "changed", G_CALLBACK (on_providers_file_changed), self);
        self->monitors = g_list_append (self->monitors, monitor);
        file_enumerator = g_file_enumerate_children (key_files_directory_file,
                                                     "standard::name,standard::type",
                                                     G_FILE_QUERY_INFO_NONE,
                                                     NULL,
                                                     &error);
        if (error)
        {
            error = NULL;
            continue;
        }

        info = g_file_enumerator_next_file (file_enumerator, NULL, &error);
        if (error)
        {
             g_warning ("Error while enumerating file %s error: %s\n", key_files_directory_path, error->message);
             error = NULL;
             continue;
        }
        while (info != NULL && error == NULL)
        {
            load_cloud_provider (self, g_file_enumerator_get_child (file_enumerator, info));
            g_object_unref (info);
            info = g_file_enumerator_next_file (file_enumerator, NULL, &error);
        }
        g_object_unref (file_enumerator);
        g_free (key_files_directory_path);
        g_object_unref (key_files_directory_file);
    }
}

static void
update_cloud_providers (CloudProvidersCollector *self)
{
    GList *l;

    g_cancellable_cancel (self->cancellable);
    self->cancellable = g_cancellable_new ();
    for (l = self->monitors; l != NULL; l = l->next)
    {
        g_signal_handlers_disconnect_by_data (G_OBJECT (l->data), self);
    }
    g_list_free_full (self->providers, g_object_unref);
    g_list_free_full (self->monitors, g_object_unref);
    self->providers = NULL;
    self->monitors = NULL;

    load_cloud_providers (self);

    g_signal_emit_by_name (G_OBJECT (self), "providers-changed");
}

/**
 * cloud_providers_collector_dup_singleton:
 * Main object to track changes in all providers.
 *
 * Returns: (transfer full): A manager singleton
 */
CloudProvidersCollector *
cloud_providers_collector_dup_singleton (void)
{
  static CloudProvidersCollector *self = NULL;

  if (self == NULL)
    {
      self = CLOUD_PROVIDERS_COLLECTOR (g_object_new (CLOUD_PROVIDERS_TYPE_COLLECTOR, NULL));
      self->provider_object_managers = g_hash_table_new(g_str_hash, g_str_equal);

      g_bus_get (G_BUS_TYPE_SESSION,
                 self->cancellable,
                 on_bus_acquired,
                 self);

      return self;
    }
  else
    {
      return g_object_ref (self);
    }
}

