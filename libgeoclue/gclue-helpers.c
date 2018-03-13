/* vim: set et ts=8 sw=8: */
/*
 * Geoclue convenience library.
 *
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#include "gclue-helpers.h"

#define BUS_NAME "org.freedesktop.GeoClue2"
#define MANAGER_PATH "/org/freedesktop/GeoClue2/Manager"

/**
 * SECTION: gclue-helpers
 * @title: Geoclue helper API
 * @short_description: Geoclue helper API
 *
 * TODO
 */

typedef struct {
        char              *desktop_id;
        GClueAccuracyLevel accuracy_level;

        gulong notify_id;
} ClientCreateData;

static ClientCreateData *
client_create_data_new (const char        *desktop_id,
                        GClueAccuracyLevel accuracy_level)
{
        ClientCreateData *data = g_slice_new0 (ClientCreateData);

        data->desktop_id = g_strdup (desktop_id);
        data->accuracy_level = accuracy_level;

        return data;
}

static void
client_create_data_free (ClientCreateData *data)
{
        g_free (data->desktop_id);
        g_slice_free (ClientCreateData, data);
}

static void
on_client_proxy_ready (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
        GTask *task = G_TASK (user_data);
        ClientCreateData *data;
        GClueClient *client;
        GError *error = NULL;

        client = gclue_client_proxy_new_for_bus_finish (res, &error);
        if (error != NULL) {
                g_task_return_error (task, error);
                g_object_unref (task);

                return;
        }

        data = g_task_get_task_data (task);
        gclue_client_set_desktop_id (client, data->desktop_id);
        gclue_client_set_requested_accuracy_level (client, data->accuracy_level);

        g_task_return_pointer (task, client, g_object_unref);
        g_object_unref (task);
}

static void
on_get_client_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
        GTask *task = G_TASK (user_data);
        GClueManager *manager = GCLUE_MANAGER (source_object);
        char *client_path;
        GError *error = NULL;

        if (!gclue_manager_call_get_client_finish (manager,
                                                   &client_path,
                                                   res,
                                                   &error)) {
                g_task_return_error (task, error);
                g_object_unref (task);
                g_object_unref (manager);

                return;
        }

        gclue_client_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        BUS_NAME,
                                        client_path,
                                        g_task_get_cancellable (task),
                                        on_client_proxy_ready,
                                        task);
        g_free (client_path);
        g_object_unref (manager);
}

static void
on_manager_proxy_ready (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
        GTask *task = G_TASK (user_data);
        GClueManager *manager;
        GError *error = NULL;

        manager = gclue_manager_proxy_new_finish (res, &error);
        if (error != NULL) {
                g_task_return_error (task, error);
                g_object_unref (task);

                return;
        }

        gclue_manager_call_get_client (manager,
                                       g_task_get_cancellable (task),
                                       on_get_client_ready,
                                       task);
}

/**
 * gclue_client_proxy_create:
 * @desktop_id: The desktop file id (the basename of the desktop file).
 * @accuracy_level: The requested accuracy level as #GClueAccuracyLevel.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the results are ready.
 * @user_data: User data to pass to @callback.
 *
 * A utility function to create a #GClueClientProxy without having to deal with
 * a #GClueManager.
 *
 * See #gclue_client_proxy_create_sync() for the synchronous, blocking version
 * of this function.
 */
void
gclue_client_proxy_create (const char         *desktop_id,
                           GClueAccuracyLevel  accuracy_level,
                           GCancellable       *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer            user_data)
{
        GTask *task;
        ClientCreateData *data;

        task = g_task_new (NULL, cancellable, callback, user_data);

        data = client_create_data_new (desktop_id, accuracy_level);
        g_task_set_task_data (task,
                              data,
                              (GDestroyNotify) client_create_data_free);

        gclue_manager_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         BUS_NAME,
                                         MANAGER_PATH,
                                         cancellable,
                                         on_manager_proxy_ready,
                                         task);
}

/**
 * gclue_client_proxy_create_finish:
 * @result: The #GAsyncResult obtained from the #GAsyncReadyCallback passed to
 *          gclue_client_proxy_create().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with gclue_client_proxy_create().
 *
 * Returns: (transfer full) (type GClueClientProxy): The constructed proxy
 * object or %NULL if @error is set.
 */
GClueClient *
gclue_client_proxy_create_finish (GAsyncResult *result,
                                  GError      **error)
{
        g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);

        return g_task_propagate_pointer (G_TASK (result), error);
}

typedef struct {
        GClueClient *client;
        GError     **error;

        GMainLoop *main_loop;
} ClientCreateSyncData;

static void
on_client_proxy_created (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
        ClientCreateSyncData *data = (ClientCreateSyncData *) user_data;

        data->client = gclue_client_proxy_create_finish (res, data->error);

        g_main_loop_quit (data->main_loop);
}

/**
 * gclue_client_proxy_create_sync:
 * @desktop_id: The desktop file id (the basename of the desktop file).
 * @accuracy_level: The requested accuracy level as #GClueAccuracyLevel.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * The synchronous and blocking version of #gclue_client_proxy_create().
 *
 * Returns: (transfer full) (type GClueClientProxy): The constructed proxy
 * object or %NULL if @error is set.
 */
GClueClient *
gclue_client_proxy_create_sync (const char        *desktop_id,
                                GClueAccuracyLevel accuracy_level,
                                GCancellable      *cancellable,
                                GError           **error)
{
        GClueClient *client;
        ClientCreateSyncData *data = g_slice_new0 (ClientCreateSyncData);

        data->error = error;
        data->main_loop = g_main_loop_new (NULL, FALSE);
        gclue_client_proxy_create (desktop_id,
                                   accuracy_level,
                                   cancellable,
                                   on_client_proxy_created,
                                   data);

        g_main_loop_run (data->main_loop);
        g_main_loop_unref (data->main_loop);

        client = data->client;
        g_slice_free (ClientCreateSyncData, data);

        return client;
}
