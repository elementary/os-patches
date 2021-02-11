/* vim: set et ts=8 sw=8: */
/* gclue-service-manager.c
 *
 * Copyright 2013 Red Hat, Inc.
 *
 * Geoclue is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Geoclue is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Geoclue; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#include <glib/gi18n.h>
#include <config.h>

#include "gclue-service-manager.h"
#include "gclue-service-client.h"
#include "gclue-client-info.h"
#include "geoclue-agent-interface.h"
#include "gclue-enums.h"
#include "gclue-locator.h"
#include "gclue-config.h"

/* 20 seconds as milliseconds */
#define AGENT_WAIT_TIMEOUT 20000
/*  20 seconds as microseconds */
#define AGENT_WAIT_TIMEOUT_USEC (20 * G_USEC_PER_SEC)

static void
gclue_service_manager_manager_iface_init (GClueDBusManagerIface *iface);
static void
gclue_service_manager_initable_iface_init (GInitableIface *iface);

struct _GClueServiceManagerPrivate
{
        GDBusConnection *connection;
        GList *clients;
        GHashTable *agents;

        guint last_client_id;
        guint num_clients;
        gint64 init_time;

        GClueLocator *locator;
};

G_DEFINE_TYPE_WITH_CODE (GClueServiceManager,
                         gclue_service_manager,
                         GCLUE_DBUS_TYPE_MANAGER_SKELETON,
                         G_IMPLEMENT_INTERFACE (GCLUE_DBUS_TYPE_MANAGER,
                                                gclue_service_manager_manager_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                gclue_service_manager_initable_iface_init)
                         G_ADD_PRIVATE (GClueServiceManager))

enum
{
        PROP_0,
        PROP_CONNECTION,
        PROP_ACTIVE,
        LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

static void
sync_in_use_property (GClueServiceManager *manager)
{
        gboolean in_use = FALSE;
        GList *l;
        GClueDBusManager *gdbus_manager;

        for (l = manager->priv->clients; l != NULL; l = l->next) {
                GClueDBusClient *client = GCLUE_DBUS_CLIENT (l->data);
                GClueConfig *config;
                const char *id;

                id = gclue_dbus_client_get_desktop_id (client);
                config = gclue_config_get_singleton ();

                if (gclue_dbus_client_get_active (client) &&
                    !gclue_config_is_system_component (config, id)) {
                        in_use = TRUE;

                        break;
                }
        }

        gdbus_manager = GCLUE_DBUS_MANAGER (manager);
        if (in_use != gclue_dbus_manager_get_in_use (gdbus_manager))
                gclue_dbus_manager_set_in_use (gdbus_manager, in_use);
}

static int
compare_client_bus_name (gconstpointer a,
                         gconstpointer b)
{
        GClueClientInfo *info;
        const char *bus_name = (const char *) b;

        info = gclue_service_client_get_client_info (GCLUE_SERVICE_CLIENT (a));

        return g_strcmp0 (bus_name, gclue_client_info_get_bus_name (info));
}

static void
delete_client (GClueServiceManager *manager,
                GCompareFunc        compare_func,
                gpointer            compare_func_data)
{
        GClueServiceManagerPrivate *priv = manager->priv;
        GList *l;

        l = priv->clients;
        while (l != NULL) {
                GList *next = l->next;

                if (compare_func (l->data, compare_func_data) == 0) {
                        g_object_unref (G_OBJECT (l->data));
                        priv->clients = g_list_remove_link (priv->clients, l);
                        priv->num_clients--;
                        if (priv->num_clients == 0) {
                                g_object_notify (G_OBJECT (manager), "active");
                        }
                }
                l = next;
        }

        g_debug ("Number of connected clients: %u", priv->num_clients);
        sync_in_use_property (manager);
}

static void
on_peer_vanished (GClueClientInfo *info,
                  gpointer         user_data)
{
        const char *bus_name;

        bus_name = gclue_client_info_get_bus_name (info);
        g_debug ("Client `%s` vanished. Dropping associated client objects",
                 bus_name);

        delete_client (GCLUE_SERVICE_MANAGER (user_data),
                       compare_client_bus_name,
                       (char *) bus_name);
}

typedef struct
{
        GClueDBusManager *manager;
        GDBusMethodInvocation *invocation;
        GClueClientInfo *client_info;
        gboolean reuse_client;

} OnClientInfoNewReadyData;

static gboolean
complete_get_client (OnClientInfoNewReadyData *data)
{
        GClueServiceManagerPrivate *priv = GCLUE_SERVICE_MANAGER (data->manager)->priv;
        GClueServiceClient *client;
        GClueClientInfo *info = data->client_info;
        GClueAgent *agent_proxy = NULL;
        GError *error = NULL;
        char *path;
        guint32 user_id;

        user_id = gclue_client_info_get_user_id (info);
        agent_proxy = g_hash_table_lookup (priv->agents,
                                           GINT_TO_POINTER (user_id));

        if (data->reuse_client) {
                GList *node;
                const char *peer;

                peer = g_dbus_method_invocation_get_sender (data->invocation);
                node = g_list_find_custom (priv->clients,
                                           peer,
                                           compare_client_bus_name);
                if (node != NULL) {
                        GClueServiceClient *client;

                        client = GCLUE_SERVICE_CLIENT (node->data);
                        path = g_strdup
                                (gclue_service_client_get_path (client));

                        goto client_created;
                }
        }

        path = g_strdup_printf ("/org/freedesktop/GeoClue2/Client/%u",
                                ++priv->last_client_id);

        client = gclue_service_client_new (info,
                                           path,
                                           priv->connection,
                                           agent_proxy,
                                           &error);
        if (client == NULL)
                goto error_out;

        priv->clients = g_list_prepend (priv->clients, client);
        priv->num_clients++;
        if (priv->num_clients == 1) {
                g_object_notify (G_OBJECT (data->manager), "active");
        }
        g_debug ("Number of connected clients: %u", priv->num_clients);

        g_signal_connect (info,
                          "peer-vanished",
                          G_CALLBACK (on_peer_vanished),
                          data->manager);

client_created:
        if (data->reuse_client)
                gclue_dbus_manager_complete_get_client (data->manager,
                                                        data->invocation,
                                                        path);
        else
                gclue_dbus_manager_complete_create_client (data->manager,
                                                           data->invocation,
                                                           path);
        goto out;

error_out:
        g_dbus_method_invocation_return_error_literal (data->invocation,
                                                       G_DBUS_ERROR,
                                                       G_DBUS_ERROR_FAILED,
                                                       error->message);
out:
        g_clear_error (&error);
        g_clear_object (&info);
        g_slice_free (OnClientInfoNewReadyData, data);
        g_free (path);

        return FALSE;
}

static void
on_client_info_new_ready (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
        OnClientInfoNewReadyData *data = (OnClientInfoNewReadyData *) user_data;
        GClueServiceManagerPrivate *priv = GCLUE_SERVICE_MANAGER (data->manager)->priv;
        GClueClientInfo *info = NULL;
        GClueAgent *agent_proxy;
        GError *error = NULL;
        guint32 user_id;
        gint64 now;
        gboolean system_app;

        info = gclue_client_info_new_finish (res, &error);
        if (info == NULL) {
                g_dbus_method_invocation_return_error_literal (data->invocation,
                                                               G_DBUS_ERROR,
                                                               G_DBUS_ERROR_FAILED,
                                                               error->message);
                g_error_free (error);
                g_slice_free (OnClientInfoNewReadyData, data);

                return;
        }

        data->client_info = info;

        user_id = gclue_client_info_get_user_id (info);
        agent_proxy = g_hash_table_lookup (priv->agents,
                                           GINT_TO_POINTER (user_id));
        now = g_get_monotonic_time ();

        system_app = (gclue_client_info_get_xdg_id (info) == NULL);
        if (agent_proxy == NULL &&
            !system_app &&
            now < (priv->init_time + AGENT_WAIT_TIMEOUT_USEC)) {
                /* Its possible that geoclue was just launched on GetClient
                 * call, in which case agents need some time to register
                 * themselves to us.
                 */
                g_timeout_add (AGENT_WAIT_TIMEOUT,
                               (GSourceFunc) complete_get_client,
                               user_data);
                return;
        }

        complete_get_client (data);
}

static void
handle_get_client (GClueDBusManager      *manager,
                   GDBusMethodInvocation *invocation,
                   gboolean               reuse_client)
{
        GClueServiceManager *self = GCLUE_SERVICE_MANAGER (manager);
        GClueServiceManagerPrivate *priv = self->priv;
        const char *peer;
        OnClientInfoNewReadyData *data;

        peer = g_dbus_method_invocation_get_sender (invocation);

        data = g_slice_new (OnClientInfoNewReadyData);
        data->manager = manager;
        data->invocation = invocation;
        data->reuse_client = reuse_client;
        gclue_client_info_new_async (peer,
                                     priv->connection,
                                     NULL,
                                     on_client_info_new_ready,
                                     data);
}

static gboolean
gclue_service_manager_handle_get_client (GClueDBusManager      *manager,
                                         GDBusMethodInvocation *invocation)
{
        handle_get_client (manager, invocation, TRUE);

        return TRUE;
}

static gboolean
gclue_service_manager_handle_create_client (GClueDBusManager      *manager,
                                            GDBusMethodInvocation *invocation)
{
        handle_get_client (manager, invocation, FALSE);

        return TRUE;
}

typedef struct
{
        const char *path;
        const char *bus_name;
} CompareClientPathNBusNameData;

static int
compare_client_path_n_bus_name (gconstpointer a,
                                gconstpointer b)
{
        GClueServiceClient *client = GCLUE_SERVICE_CLIENT (a);
        CompareClientPathNBusNameData *data =
                (CompareClientPathNBusNameData *) b;
        GClueClientInfo *info;

        info = gclue_service_client_get_client_info (client);

        if (g_strcmp0 (data->bus_name,
                       gclue_client_info_get_bus_name (info)) == 0 &&
            g_strcmp0 (data->path,
                       gclue_service_client_get_path (client)) == 0)
                return 0;
        else
                return 1;
}

static gboolean
gclue_service_manager_handle_delete_client (GClueDBusManager      *manager,
                                            GDBusMethodInvocation *invocation,
                                            const char            *path)
{
        CompareClientPathNBusNameData data;

        data.path = path;
        data.bus_name = g_dbus_method_invocation_get_sender (invocation);

        delete_client (GCLUE_SERVICE_MANAGER (manager),
                       compare_client_path_n_bus_name,
                       &data);

        gclue_dbus_manager_complete_delete_client (manager, invocation);

        return TRUE;
}

typedef struct
{
        GClueDBusManager *manager;
        GDBusMethodInvocation *invocation;
        GClueClientInfo *info;
        char *desktop_id;
} AddAgentData;

static void
add_agent_data_free (AddAgentData *data)
{
        g_clear_pointer (&data->desktop_id, g_free);
        g_slice_free (AddAgentData, data);
}

static void
on_agent_vanished (GClueClientInfo *info,
                   gpointer         user_data)
{
        GClueServiceManager *manager = GCLUE_SERVICE_MANAGER (user_data);
        guint32 user_id;

        user_id = gclue_client_info_get_user_id (info);
        g_debug ("Agent for user '%u' vanished", user_id);
        g_hash_table_remove (manager->priv->agents, GINT_TO_POINTER (user_id));
        g_object_unref (info);
}

static void
on_agent_proxy_ready (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
        AddAgentData *data = (AddAgentData *) user_data;
        GClueServiceManagerPrivate *priv = GCLUE_SERVICE_MANAGER (data->manager)->priv;
        guint32 user_id;
        GClueAgent *agent;
        GError *error = NULL;

        agent = gclue_agent_proxy_new_for_bus_finish (res, &error);
        if (agent == NULL)
            goto error_out;

        user_id = gclue_client_info_get_user_id (data->info);
        g_debug ("New agent for user ID '%u'", user_id);
        g_hash_table_replace (priv->agents, GINT_TO_POINTER (user_id), agent);

        g_signal_connect (data->info,
                          "peer-vanished",
                          G_CALLBACK (on_agent_vanished),
                          data->manager);

        gclue_dbus_manager_complete_add_agent (data->manager, data->invocation);

        goto out;

error_out:
        g_dbus_method_invocation_return_error_literal (data->invocation,
                                                       G_DBUS_ERROR,
                                                       G_DBUS_ERROR_FAILED,
                                                       error->message);
out:
        g_clear_error (&error);
        add_agent_data_free (data);
}

#define AGENT_PATH "/org/freedesktop/GeoClue2/Agent"

static void
on_agent_info_new_ready (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
        AddAgentData *data = (AddAgentData *) user_data;
        GError *error = NULL;
        GClueConfig *config;
        const char *xdg_id;

        data->info = gclue_client_info_new_finish (res, &error);
        if (data->info == NULL) {
                g_dbus_method_invocation_return_error_literal (data->invocation,
                                                               G_DBUS_ERROR,
                                                               G_DBUS_ERROR_FAILED,
                                                               error->message);
                g_error_free (error);
                add_agent_data_free (data);

                return;
        }

        xdg_id = gclue_client_info_get_xdg_id (data->info);
        config = gclue_config_get_singleton ();
        if (xdg_id != NULL ||
            !gclue_config_is_agent_allowed (config,
                                            data->desktop_id,
                                            data->info)) {
                g_dbus_method_invocation_return_error (data->invocation,
                                                       G_DBUS_ERROR,
                                                       G_DBUS_ERROR_ACCESS_DENIED,
                                                       "%s not allowed to act as agent",
                                                       data->desktop_id);
                add_agent_data_free (data);

                return;
        }

        gclue_agent_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       gclue_client_info_get_bus_name (data->info),
                                       AGENT_PATH,
                                       NULL,
                                       on_agent_proxy_ready,
                                       user_data);
}

static gboolean
gclue_service_manager_handle_add_agent (GClueDBusManager      *manager,
                                        GDBusMethodInvocation *invocation,
                                        const char            *id)
{
        GClueServiceManager *self = GCLUE_SERVICE_MANAGER (manager);
        GClueServiceManagerPrivate *priv = self->priv;
        const char *peer;
        AddAgentData *data;

        peer = g_dbus_method_invocation_get_sender (invocation);

        data = g_slice_new0 (AddAgentData);
        data->manager = manager;
        data->invocation = invocation;
        data->desktop_id = g_strdup (id);
        gclue_client_info_new_async (peer,
                                     priv->connection,
                                     NULL,
                                     on_agent_info_new_ready,
                                     data);
        return TRUE;
}

static void
gclue_service_manager_finalize (GObject *object)
{
        GClueServiceManagerPrivate *priv = GCLUE_SERVICE_MANAGER (object)->priv;

        g_clear_object (&priv->locator);
        g_clear_object (&priv->connection);
        if (priv->clients != NULL) {
                g_list_free_full (priv->clients, g_object_unref);
                priv->clients = NULL;
        }
        g_clear_pointer (&priv->agents, g_hash_table_unref);

        /* Chain up to the parent class */
        G_OBJECT_CLASS (gclue_service_manager_parent_class)->finalize (object);
}

static void
gclue_service_manager_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        GClueServiceManager *manager = GCLUE_SERVICE_MANAGER (object);

        switch (prop_id) {
        case PROP_CONNECTION:
                g_value_set_object (value, manager->priv->connection);
                break;

        case PROP_ACTIVE:
                g_value_set_boolean (value, (manager->priv->num_clients != 0));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_service_manager_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        GClueServiceManager *manager = GCLUE_SERVICE_MANAGER (object);

        switch (prop_id) {
        case PROP_CONNECTION:
                manager->priv->connection = g_value_dup_object (value);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
on_avail_accuracy_level_changed (GObject    *object,
                                 GParamSpec *pspec,
                                 gpointer    user_data)
{
        GClueServiceManagerPrivate *priv = GCLUE_SERVICE_MANAGER (user_data)->priv;
        GClueAccuracyLevel level;

        level = gclue_location_source_get_available_accuracy_level
                        (GCLUE_LOCATION_SOURCE (priv->locator));
        gclue_dbus_manager_set_available_accuracy_level
                (GCLUE_DBUS_MANAGER (user_data), level);
}

static void
gclue_service_manager_constructed (GObject *object)
{
        GClueServiceManagerPrivate *priv = GCLUE_SERVICE_MANAGER (object)->priv;

        G_OBJECT_CLASS (gclue_service_manager_parent_class)->constructed (object);

        priv->locator = gclue_locator_new (GCLUE_ACCURACY_LEVEL_EXACT);
        g_signal_connect (G_OBJECT (priv->locator),
                          "notify::available-accuracy-level",
                          G_CALLBACK (on_avail_accuracy_level_changed),
                          object);
        on_avail_accuracy_level_changed (G_OBJECT (priv->locator),
                                         NULL,
                                         object);
}

static void
gclue_service_manager_class_init (GClueServiceManagerClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gclue_service_manager_finalize;
        object_class->get_property = gclue_service_manager_get_property;
        object_class->set_property = gclue_service_manager_set_property;
        object_class->constructed = gclue_service_manager_constructed;

        gParamSpecs[PROP_CONNECTION] = g_param_spec_object ("connection",
                                                            "Connection",
                                                            "DBus Connection",
                                                            G_TYPE_DBUS_CONNECTION,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class,
                                         PROP_CONNECTION,
                                         gParamSpecs[PROP_CONNECTION]);

        /**
         * GClueServiceManager:active:
         *
         * Unlike the D-Bus 'InUse' property, this indicates wether or not
         * currently we have connected clients.
         */
        gParamSpecs[PROP_ACTIVE] = g_param_spec_boolean ("active",
                                                         "Active",
                                                         "If manager is active",
                                                         FALSE,
                                                         G_PARAM_READABLE);
        g_object_class_install_property (object_class,
                                         PROP_ACTIVE,
                                         gParamSpecs[PROP_ACTIVE]);
}

static void
gclue_service_manager_init (GClueServiceManager *manager)
{
        manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
                                                     GCLUE_TYPE_SERVICE_MANAGER,
                                                     GClueServiceManagerPrivate);

        manager->priv->agents = g_hash_table_new_full (g_direct_hash,
                                                       g_direct_equal,
                                                       NULL,
                                                       g_object_unref);
        manager->priv->init_time = g_get_monotonic_time ();
}

static gboolean
gclue_service_manager_initable_init (GInitable    *initable,
                                     GCancellable *cancellable,
                                     GError      **error)
{
        return g_dbus_interface_skeleton_export
                (G_DBUS_INTERFACE_SKELETON (initable),
                 GCLUE_SERVICE_MANAGER (initable)->priv->connection,
                 "/org/freedesktop/GeoClue2/Manager",
                 error);
}

static void
gclue_service_manager_manager_iface_init (GClueDBusManagerIface *iface)
{
        iface->handle_get_client = gclue_service_manager_handle_get_client;
        iface->handle_create_client =
                gclue_service_manager_handle_create_client;
        iface->handle_delete_client =
                gclue_service_manager_handle_delete_client;
        iface->handle_add_agent = gclue_service_manager_handle_add_agent;
}

static void
gclue_service_manager_initable_iface_init (GInitableIface *iface)
{
        iface->init = gclue_service_manager_initable_init;
}

GClueServiceManager *
gclue_service_manager_new (GDBusConnection *connection,
                           GError         **error)
{
        return g_initable_new (GCLUE_TYPE_SERVICE_MANAGER,
                               NULL,
                               error,
                               "connection", connection,
                               NULL);
}

gboolean
gclue_service_manager_get_active (GClueServiceManager *manager)
{
        return (manager->priv->num_clients != 0);
}
