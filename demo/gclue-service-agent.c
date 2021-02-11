/* vim: set et ts=8 sw=8: */
/* gclue-service-agent.c
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
#include <libnotify/notify.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include <gclue-enums.h>

#include "gclue-service-agent.h"

#define AGENT_PATH "/org/freedesktop/GeoClue2/Agent"

#define SERVICE           "org.freedesktop.GeoClue2"
#define MANAGER_PATH      "/org/freedesktop/GeoClue2/Manager"
#define MANAGER_INTERFACE "org.freedesktop.GeoClue2.Manager"

static void
gclue_service_agent_agent_iface_init (GClueAgentIface *iface);
static void
gclue_service_agent_constructed (GObject *object);

struct _GClueServiceAgentPrivate
{
        GDBusConnection *connection;
        GDBusProxy *manager_proxy;
};

G_DEFINE_TYPE_WITH_CODE (GClueServiceAgent,
                         gclue_service_agent,
                         GCLUE_TYPE_AGENT_SKELETON,
                         G_IMPLEMENT_INTERFACE (GCLUE_TYPE_AGENT,
                                                gclue_service_agent_agent_iface_init)
                         G_ADD_PRIVATE (GClueServiceAgent))

enum
{
        PROP_0,
        PROP_CONNECTION,
        LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

static void
gclue_service_agent_finalize (GObject *object)
{
        GClueServiceAgentPrivate *priv = GCLUE_SERVICE_AGENT (object)->priv;

        g_clear_object (&priv->connection);

        /* Chain up to the parent class */
        G_OBJECT_CLASS (gclue_service_agent_parent_class)->finalize (object);
}

static void
gclue_service_agent_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        GClueServiceAgent *agent = GCLUE_SERVICE_AGENT (object);

        switch (prop_id) {
        case PROP_CONNECTION:
                g_value_set_object (value, agent->priv->connection);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_service_agent_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        GClueServiceAgent *agent = GCLUE_SERVICE_AGENT (object);

        switch (prop_id) {
        case PROP_CONNECTION:
                agent->priv->connection = g_value_dup_object (value);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_service_agent_class_init (GClueServiceAgentClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gclue_service_agent_finalize;
        object_class->get_property = gclue_service_agent_get_property;
        object_class->set_property = gclue_service_agent_set_property;
        object_class->constructed = gclue_service_agent_constructed;

        gParamSpecs[PROP_CONNECTION] = g_param_spec_object ("connection",
                                                            "Connection",
                                                            "DBus Connection",
                                                            G_TYPE_DBUS_CONNECTION,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class,
                                         PROP_CONNECTION,
                                         gParamSpecs[PROP_CONNECTION]);
}

static void
gclue_service_agent_init (GClueServiceAgent *agent)
{
        agent->priv = G_TYPE_INSTANCE_GET_PRIVATE (agent,
                                                   GCLUE_TYPE_SERVICE_AGENT,
                                                   GClueServiceAgentPrivate);
}

static void
on_add_agent_ready (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
        GError *error = NULL;

        g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                  res,
                                  &error);
}

static void
print_in_use_info (GDBusProxy *manager_proxy)
{
        GVariant *variant;

        variant = g_dbus_proxy_get_cached_property (manager_proxy, "InUse");

        if (g_variant_get_boolean (variant))
                g_print (_("Geolocation service in use\n"));
        else
                g_print (_("Geolocation service not in use\n"));
}

static void
on_manager_props_changed (GDBusProxy *manager_proxy,
                          GVariant   *changed_properties,
                          GStrv       invalidated_properties,
                          gpointer    user_data)
{
        GVariantIter *iter;
        GVariant *value;
        gchar *key;

        if (g_variant_n_children (changed_properties) < 0)
                return;

        g_variant_get (changed_properties, "a{sv}", &iter);
        while (g_variant_iter_loop (iter, "{&sv}", &key, &value)) {
                if (strcmp (key, "InUse") != 0)
                        continue;

                print_in_use_info (manager_proxy);
                break;
        }
        g_variant_iter_free (iter);
}

static void
on_manager_proxy_ready (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
        GClueServiceAgent *agent;
        GError *error = NULL;
        agent = GCLUE_SERVICE_AGENT (user_data);

        agent->priv->manager_proxy = g_dbus_proxy_new_for_bus_finish (res,
                                                                      &error);
        if (agent->priv->manager_proxy == NULL) {
                g_critical ("Failed to create proxy to %s: %s",
                            MANAGER_PATH,
                            error->message);
                g_error_free (error);
                return;
        }

        g_dbus_proxy_call (agent->priv->manager_proxy,
                           "AddAgent",
                           g_variant_new ("(s)",
                                          "geoclue-demo-agent"),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           on_add_agent_ready,
                           NULL);
        print_in_use_info (agent->priv->manager_proxy);
        g_signal_connect (agent->priv->manager_proxy,
                          "g-properties-changed",
                          G_CALLBACK (on_manager_props_changed),
                          NULL);
}

static void
on_name_appeared (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  SERVICE,
                                  MANAGER_PATH,
                                  MANAGER_INTERFACE,
                                  NULL,
                                  on_manager_proxy_ready,
                                  user_data);
}

static void
on_name_vanished (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
        GClueServiceAgent *agent = GCLUE_SERVICE_AGENT (user_data);

        g_clear_object (&agent->priv->manager_proxy);
}

static void
gclue_service_agent_constructed (GObject *object)
{
        GError *error = NULL;
        GClueServiceAgent *agent = GCLUE_SERVICE_AGENT (object);
        if (!g_dbus_interface_skeleton_export
                (G_DBUS_INTERFACE_SKELETON (agent),
                 agent->priv->connection,
                 AGENT_PATH,
                 &error)) {
                return;
        }
        gclue_agent_set_max_accuracy_level (GCLUE_AGENT (agent),
                                            GCLUE_ACCURACY_LEVEL_EXACT);
        g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                          SERVICE,
                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                          on_name_appeared,
                          on_name_vanished,
                          agent,
                          NULL);

}

typedef struct
{
        GClueAgent *agent;
        GDBusMethodInvocation *invocation;
        NotifyNotification *notification;
        GAppInfo *app_info;
        gboolean authorized;
        GClueAccuracyLevel accuracy_level;
} NotificationData;

static void
notification_data_free (NotificationData *data)
{
        g_object_unref (data->app_info);
        g_object_unref (data->notification);
        g_slice_free (NotificationData, data);
}

#define ACTION_YES "yes"
#define ACTION_NO "NO"

static void
on_notify_action (NotifyNotification *notification,
                  char               *action,
                  gpointer            user_data)
{
        NotificationData *data = (NotificationData *) user_data;
        GError *error = NULL;

        data->authorized = (g_strcmp0 (action, ACTION_YES) == 0);

        if (!notify_notification_close (notification, &error)) {
                g_dbus_method_invocation_take_error (data->invocation, error);
                notification_data_free (data);
        }
}

static void
on_notify_closed (NotifyNotification *notification,
                  gpointer            user_data)
{
        NotificationData *data = (NotificationData *) user_data;

        if (data->authorized)
                g_debug ("Authorized '%s'", g_app_info_get_display_name (data->app_info));
        else
                g_debug ("'%s' not authorized",  g_app_info_get_display_name (data->app_info));
        gclue_agent_complete_authorize_app (data->agent,
                                            data->invocation,
                                            data->authorized,
                                            data->accuracy_level);
        notification_data_free (data);
}

static gboolean
gclue_service_agent_handle_authorize_app (GClueAgent            *agent,
                                          GDBusMethodInvocation *invocation,
                                          const char            *desktop_id,
                                          GClueAccuracyLevel     accuracy_level)
{
        NotifyNotification *notification;
        NotificationData *data;
        GError *error = NULL;
        char *desktop_file;
        GDesktopAppInfo *app_info;
        char *msg;
        const char *reason;

        desktop_file = g_strjoin (".", desktop_id, "desktop", NULL);
        app_info = g_desktop_app_info_new (desktop_file);
        if (app_info == NULL) {
                g_debug ("Failed to find %s", desktop_file);
                gclue_agent_complete_authorize_app (agent,
                                                    invocation,
                                                    FALSE,
                                                    accuracy_level);

                return TRUE;
        }
        g_free (desktop_file);

        msg = g_strdup_printf (_("Allow '%s' to access your location information?"),
                               g_app_info_get_display_name (G_APP_INFO (app_info)));
        reason = g_desktop_app_info_get_string (app_info, "X-Geoclue-Reason");
        if (reason != NULL) {
                char *tmp = msg;
                msg = g_strdup_printf ("%s\n\n%s", msg, reason);
                g_free (tmp);
        }
        notification = notify_notification_new (_("Geolocation"), msg, "dialog-question");
        g_free (msg);

        data = g_slice_new0 (NotificationData);
        data->invocation = invocation;
        data->notification = notification;
        data->app_info = G_APP_INFO (app_info);
        data->accuracy_level = accuracy_level;

        notify_notification_add_action (notification,
                                        ACTION_YES,
                                        _("Yes"),
                                        on_notify_action,
                                        data,
                                        NULL);
        notify_notification_add_action (notification,
                                        ACTION_NO,
                                        _("No"),
                                        on_notify_action,
                                        data,
                                        NULL);
        g_signal_connect (notification,
                          "closed",
                          G_CALLBACK (on_notify_closed),
                          data);

        if (!notify_notification_show (notification, &error)) {
                g_critical ("Failed to show notification: %s\n", error->message);
                g_dbus_method_invocation_take_error (invocation, error);
                notification_data_free (data);

                return TRUE;
        }

        return TRUE;
}

static void
gclue_service_agent_agent_iface_init (GClueAgentIface *iface)
{
        iface->handle_authorize_app = gclue_service_agent_handle_authorize_app;
}

GClueServiceAgent *
gclue_service_agent_new (GDBusConnection *connection)
{
        return g_object_new (GCLUE_TYPE_SERVICE_AGENT,
                             "connection",
                             connection,
                             NULL);
}
