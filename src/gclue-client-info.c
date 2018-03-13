/* vim: set et ts=8 sw=8: */
/* gclue-client-info.c
 *
 * Copyright (C) 2013 Red Hat, Inc.
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

#include "gclue-client-info.h"

#define MAX_CMDLINE_LEN 4096

static void
gclue_client_info_async_initable_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GClueClientInfo,
                         gclue_client_info,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                                gclue_client_info_async_initable_init));

struct _GClueClientInfoPrivate
{
        char *bus_name;
        GDBusConnection *connection;
        GDBusProxy *dbus_proxy;
        guint watch_id;

        guint32 user_id;
        char *xdg_id;
};

enum
{
        PROP_0,
        PROP_PEER,
        PROP_CONNECTION,
        LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

enum {
        PEER_VANISHED,
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

static void
gclue_client_info_finalize (GObject *object)
{
        GClueClientInfoPrivate *priv = GCLUE_CLIENT_INFO (object)->priv;

        if (priv->watch_id != 0) {
                g_bus_unwatch_name (priv->watch_id);
                priv->watch_id = 0;
        }

        g_clear_pointer (&priv->bus_name, g_free);
        g_clear_pointer (&priv->xdg_id, g_free);
        g_clear_object (&priv->connection);

        /* Chain up to the parent class */
        G_OBJECT_CLASS (gclue_client_info_parent_class)->finalize (object);
}

static void
gclue_client_info_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        GClueClientInfo *info = GCLUE_CLIENT_INFO (object);

        switch (prop_id) {
        case PROP_PEER:
                g_value_set_string (value, info->priv->bus_name);
                break;

        case PROP_CONNECTION:
                g_value_set_object (value, info->priv->connection);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_client_info_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        GClueClientInfo *info = GCLUE_CLIENT_INFO (object);

        switch (prop_id) {
        case PROP_PEER:
                info->priv->bus_name = g_value_dup_string (value);
                break;

        case PROP_CONNECTION:
                info->priv->connection = g_value_dup_object (value);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_client_info_class_init (GClueClientInfoClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gclue_client_info_finalize;
        object_class->get_property = gclue_client_info_get_property;
        object_class->set_property = gclue_client_info_set_property;

        g_type_class_add_private (object_class, sizeof (GClueClientInfoPrivate));

        gParamSpecs[PROP_PEER] = g_param_spec_string ("bus-name",
                                                      "BusName",
                                                      "Bus name of client",
                                                      NULL,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class,
                                         PROP_PEER,
                                         gParamSpecs[PROP_PEER]);

        gParamSpecs[PROP_CONNECTION] = g_param_spec_object ("connection",
                                                            "Connection",
                                                            "DBus Connection",
                                                            G_TYPE_DBUS_CONNECTION,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class,
                                         PROP_CONNECTION,
                                         gParamSpecs[PROP_CONNECTION]);

        signals[PEER_VANISHED] =
                g_signal_new ("peer-vanished",
                              GCLUE_TYPE_CLIENT_INFO,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GClueClientInfoClass,
                                               peer_vanished),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE,
                              0,
                              G_TYPE_NONE);
}

static void
on_name_vanished (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
        g_signal_emit (GCLUE_CLIENT_INFO (user_data),
                       signals[PEER_VANISHED],
                       0);
}

/* Based on got_credentials_cb() from xdg-app source code */
static char *
get_xdg_id (guint32 pid)
{
        char *xdg_id = NULL;
        g_autofree char *path = NULL;
        g_autofree char *content = NULL;
        gchar **lines;
        int i;

        path = g_strdup_printf ("/proc/%u/cgroup", pid);

        if (!g_file_get_contents (path, &content, NULL, NULL))
                return NULL;
        lines =  g_strsplit (content, "\n", -1);

        for (i = 0; lines[i] != NULL; i++) {
                const char *unit = lines[i] + strlen ("1:name=systemd:");
                g_autofree char *scope = NULL;
                const char *name;
                char *dash;

                if (!g_str_has_prefix (lines[i], "1:name=systemd:"))
                        continue;

                scope = g_path_get_basename (unit);
                if ((!g_str_has_prefix (scope, "xdg-app-") &&
                     !g_str_has_prefix (scope, "flatpak-")) ||
                    !g_str_has_suffix (scope, ".scope"))
                        break;

                /* strlen("flatpak-") == strlen("xdg-app-")
                 * so all is good here */
                name = scope + strlen("xdg-app-");
                dash = strchr (name, '-');

                if (dash == NULL)
                        break;

                *dash = 0;
                xdg_id = g_strdup (name);
        }

        g_strfreev (lines);

        return xdg_id;
}

static void
on_get_pid_ready (GObject      *source_object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
        GTask *task = G_TASK (user_data);
        gpointer *info = g_task_get_source_object (task);
        GClueClientInfoPrivate *priv = GCLUE_CLIENT_INFO (info)->priv;
        guint32 pid;
        GError *error = NULL;
        GVariant *results = NULL;

        results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                            res,
                                            &error);
        if (results == NULL) {
                g_task_return_error (task, error);
                g_object_unref (task);

                return;
        }

        g_assert (g_variant_n_children (results) > 0);
        g_variant_get_child (results, 0, "u", &pid);
        g_variant_unref (results);

        priv->xdg_id = get_xdg_id (pid);

        priv->watch_id = g_bus_watch_name_on_connection (priv->connection,
                                                         priv->bus_name,
                                                         G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                         NULL,
                                                         on_name_vanished,
                                                         info,
                                                         NULL);

        g_task_return_boolean (task, TRUE);

        g_object_unref (task);
}

static void
on_get_user_id_ready (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
        GTask *task = G_TASK (user_data);
        gpointer *info = g_task_get_source_object (task);
        GClueClientInfoPrivate *priv = GCLUE_CLIENT_INFO (info)->priv;
        GError *error = NULL;
        GVariant *results = NULL;

        results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                            res,
                                            &error);
        if (results == NULL) {
                g_task_return_error (task, error);
                g_object_unref (task);

                return;
        }

        g_assert (g_variant_n_children (results) > 0);
        g_variant_get_child (results, 0, "u", &priv->user_id);
        g_variant_unref (results);

        g_dbus_proxy_call (priv->dbus_proxy,
                           "GetConnectionUnixProcessID",
                           g_variant_new ("(s)", priv->bus_name),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           g_task_get_cancellable (task),
                           on_get_pid_ready,
                           task);
}

static void
on_dbus_proxy_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
        GTask *task = G_TASK (user_data);
        gpointer *info = g_task_get_source_object (task);
        GClueClientInfoPrivate *priv = GCLUE_CLIENT_INFO (info)->priv;
        GError *error = NULL;

        priv->dbus_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (priv->dbus_proxy == NULL) {
                g_task_return_error (task, error);
                g_object_unref (task);
                return;
        }

        g_dbus_proxy_call (priv->dbus_proxy,
                           "GetConnectionUnixUser",
                           g_variant_new ("(s)", priv->bus_name),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           g_task_get_cancellable (task),
                           on_get_user_id_ready,
                           task);
}

static void
gclue_client_info_init_async (GAsyncInitable     *initable,
                              int                 io_priority,
                              GCancellable       *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer            user_data)
{
        GTask *task;

        task = g_task_new (initable, cancellable, callback, user_data);

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                  NULL,
                                  "org.freedesktop.DBus",
                                  "/org/freedesktop/DBus",
                                  "org.freedesktop.DBus",
                                  cancellable,
                                  on_dbus_proxy_ready,
                                  task);
}

static gboolean
gclue_client_info_init_finish (GAsyncInitable *initable,
                               GAsyncResult   *result,
                               GError        **error)
{
        return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gclue_client_info_async_initable_init (GAsyncInitableIface *iface)
{
        iface->init_async = gclue_client_info_init_async;
        iface->init_finish = gclue_client_info_init_finish;
}

static void
gclue_client_info_init (GClueClientInfo *info)
{
        info->priv = G_TYPE_INSTANCE_GET_PRIVATE (info,
                                                  GCLUE_TYPE_CLIENT_INFO,
                                                  GClueClientInfoPrivate);
}

void
gclue_client_info_new_async (const char         *bus_name,
                             GDBusConnection    *connection,
                             GCancellable       *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer            user_data)
{
        g_async_initable_new_async (GCLUE_TYPE_CLIENT_INFO,
                                    G_PRIORITY_DEFAULT,
                                    cancellable,
                                    callback,
                                    user_data,
                                    "bus-name", bus_name,
                                    "connection", connection,
                                    NULL);
}

GClueClientInfo *
gclue_client_info_new_finish (GAsyncResult *res,
                              GError      **error)
{
        GObject *object;
        GObject *source_object;

        source_object = g_async_result_get_source_object (res);
        object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                              res,
                                              error);
        g_object_unref (source_object);
        if (object != NULL)
                return GCLUE_CLIENT_INFO (object);
        else
                return NULL;
}

const gchar *
gclue_client_info_get_bus_name (GClueClientInfo *info)
{
        g_return_val_if_fail (GCLUE_IS_CLIENT_INFO(info), NULL);

        return info->priv->bus_name;
}

guint32
gclue_client_info_get_user_id (GClueClientInfo *info)
{
        g_return_val_if_fail (GCLUE_IS_CLIENT_INFO(info), 0);

        return info->priv->user_id;
}

gboolean
gclue_client_info_check_bus_name (GClueClientInfo *info,
                                  const char      *bus_name)
{
        g_return_val_if_fail (GCLUE_IS_CLIENT_INFO(info), FALSE);

        return (strcmp (bus_name, info->priv->bus_name) == 0);
}

const char *
gclue_client_info_get_xdg_id (GClueClientInfo *info)
{
        g_return_val_if_fail (GCLUE_IS_CLIENT_INFO(info), FALSE);

        return info->priv->xdg_id;
}
