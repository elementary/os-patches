/* vim: set et ts=8 sw=8: */
/* gclue-service-client.c
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

#include "gclue-service-client.h"
#include "gclue-service-location.h"
#include "gclue-locator.h"
#include "gclue-enum-types.h"
#include "gclue-config.h"

#define DEFAULT_ACCURACY_LEVEL GCLUE_ACCURACY_LEVEL_CITY
#define DEFAULT_AGENT_STARTUP_WAIT_SECS 5

static void
gclue_service_client_client_iface_init (GClueDBusClientIface *iface);
static void
gclue_service_client_initable_iface_init (GInitableIface *iface);

typedef struct _StartData StartData;

struct _GClueServiceClientPrivate
{
        GClueClientInfo *client_info;
        char *path;
        GDBusConnection *connection;
        GClueAgent *agent_proxy;
        StartData *pending_auth_start_data;
        guint pending_auth_timeout_id;

        GClueServiceLocation *location;
        GClueServiceLocation *prev_location;
        guint distance_threshold;
        guint time_threshold;

        GClueLocator *locator;

        /* Number of times location has been updated */
        guint locations_updated;

        gboolean agent_stopped; /* Agent stopped client, not the app */
};

G_DEFINE_TYPE_WITH_CODE (GClueServiceClient,
                         gclue_service_client,
                         GCLUE_DBUS_TYPE_CLIENT_SKELETON,
                         G_IMPLEMENT_INTERFACE (GCLUE_DBUS_TYPE_CLIENT,
                                                gclue_service_client_client_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                gclue_service_client_initable_iface_init)
                         G_ADD_PRIVATE (GClueServiceClient));

enum
{
        PROP_0,
        PROP_CLIENT_INFO,
        PROP_PATH,
        PROP_CONNECTION,
        PROP_AGENT_PROXY,
        LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

static char *
next_location_path (GClueServiceClient *client)
{
        GClueServiceClientPrivate *priv = client->priv;
        char *path, *index_str;

        index_str = g_strdup_printf ("%u", (priv->locations_updated)++),
        path = g_strjoin ("/", priv->path, "Location", index_str, NULL);
        g_free (index_str);

        return path;
}

/* We don't use the gdbus-codegen provided gclue_client_emit_location_updated()
 * as that sends the signal to all listeners on the bus
 */
static gboolean
emit_location_updated (GClueServiceClient *client,
                       const char         *old,
                       const char         *new,
                       GError            **error)
{
        GClueServiceClientPrivate *priv = client->priv;
        GVariant *variant;
        const char *peer;

        variant = g_variant_new ("(oo)", old, new);
        peer = gclue_client_info_get_bus_name (priv->client_info);

        return g_dbus_connection_emit_signal (priv->connection,
                                              peer,
                                              priv->path,
                                              "org.freedesktop.GeoClue2.Client",
                                              "LocationUpdated",
                                              variant,
                                              error);
}

static gboolean
distance_below_threshold (GClueServiceClient *client,
                          GClueLocation      *location)
{
        GClueServiceClientPrivate *priv = client->priv;
        GClueLocation *cur_location;
        gdouble distance;
        gdouble threshold_km;

        if (priv->distance_threshold == 0)
                return FALSE;

        g_object_get (priv->location,
                      "location", &cur_location,
                      NULL);
        distance = gclue_location_get_distance_from (cur_location, location);
        g_object_unref (cur_location);

        threshold_km = priv->distance_threshold / 1000.0;
        if (distance < threshold_km) {
                g_debug ("Distance from previous location is %f km and "
                         "below threshold of %f km.",
                         distance, threshold_km); 
                return TRUE;
        }

        return FALSE;
}

static gboolean
time_below_threshold (GClueServiceClient *client,
                      GClueLocation      *location)
{
        GClueServiceClientPrivate *priv = client->priv;
        GClueLocation *cur_location;
        gint64 cur_ts, ts;
        guint64 diff_ts;

        if (priv->time_threshold == 0)
                return FALSE;

        g_object_get (priv->location,
                      "location", &cur_location,
                      NULL);

        cur_ts = gclue_location_get_timestamp (cur_location);
        ts = gclue_location_get_timestamp (location);
        diff_ts = ABS (ts - cur_ts);

        g_object_unref (cur_location);

        if (diff_ts < priv->time_threshold) {
                g_debug ("Time difference between previous and new location"
                         " is %" G_GUINT64_FORMAT " seconds and"
                         " below threshold of %" G_GUINT32_FORMAT " seconds.",
                         diff_ts, priv->time_threshold);
                return TRUE;
        }

        return FALSE;
}

static gboolean
below_threshold (GClueServiceClient *client,
                 GClueLocation      *location)
{
        return (distance_below_threshold (client, location) ||
                time_below_threshold (client, location));
}

static gboolean
on_prev_location_timeout (gpointer user_data)
{
        g_object_unref (user_data);

        return FALSE;
}

static void
on_locator_location_changed (GObject    *gobject,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
        GClueServiceClient *client = GCLUE_SERVICE_CLIENT (user_data);
        GClueServiceClientPrivate *priv = client->priv;
        GClueLocationSource *locator = GCLUE_LOCATION_SOURCE (gobject);
        GClueLocation *location_info;
        char *path = NULL;
        const char *prev_path;
        GError *error = NULL;

        location_info = gclue_location_source_get_location (locator);
        if (location_info == NULL)
                return; /* No location found yet */

        if (priv->location != NULL && below_threshold (client, location_info)) {
                g_debug ("Updating location, below threshold");
                g_object_set (priv->location,
                              "location", location_info,
                              NULL);
                return;
        }

        if (priv->prev_location != NULL)
                // Lets try to ensure that apps are not still accessing the
                // last location before unrefing (and therefore destroying) it.
                g_timeout_add_seconds (5, on_prev_location_timeout, priv->prev_location);
        priv->prev_location = priv->location;

        path = next_location_path (client);
        priv->location = gclue_service_location_new (priv->client_info,
                                                     path,
                                                     priv->connection,
                                                     location_info,
                                                     &error);
        if (priv->location == NULL)
                goto error_out;

        if (priv->prev_location != NULL)
                prev_path = gclue_service_location_get_path (priv->prev_location);
        else
                prev_path = "/";

        gclue_dbus_client_set_location (GCLUE_DBUS_CLIENT (client), path);

        if (!emit_location_updated (client, prev_path, path, &error))
                goto error_out;
        goto out;

error_out:
        g_warning ("Failed to update location info: %s", error->message);
        g_error_free (error);
out:
        g_free (path);
}

static void
start_client (GClueServiceClient *client, GClueAccuracyLevel accuracy_level)
{
        GClueServiceClientPrivate *priv = client->priv;

        gclue_dbus_client_set_active (GCLUE_DBUS_CLIENT (client), TRUE);
        priv->locator = gclue_locator_new (accuracy_level);
        gclue_locator_set_time_threshold (priv->locator, 0);
        g_signal_connect (priv->locator,
                          "notify::location",
                          G_CALLBACK (on_locator_location_changed),
                          client);

        gclue_location_source_start (GCLUE_LOCATION_SOURCE (priv->locator));
}

static void
stop_client (GClueServiceClient *client)
{
        g_clear_object (&client->priv->locator);
        gclue_dbus_client_set_active (GCLUE_DBUS_CLIENT (client), FALSE);
}

static GClueAccuracyLevel
ensure_valid_accuracy_level (GClueAccuracyLevel accuracy_level,
                             GClueAccuracyLevel max_accuracy)
{
        GClueAccuracyLevel accuracy;
        GEnumClass *enum_class;
        GEnumValue *enum_value;

        accuracy = CLAMP (accuracy_level,
                          GCLUE_ACCURACY_LEVEL_COUNTRY,
                          max_accuracy);

        enum_class = g_type_class_ref (GCLUE_TYPE_ACCURACY_LEVEL);
        enum_value = g_enum_get_value (enum_class, accuracy);

        if (enum_value == NULL) {
                GClueAccuracyLevel i;

                g_debug ("Invalid accuracy level %u requested", accuracy_level);
                for (i = accuracy; i >= GCLUE_ACCURACY_LEVEL_COUNTRY; i--) {
                        enum_value = g_enum_get_value (enum_class, i);

                        if (enum_value != NULL) {
                                accuracy = i;

                                break;
                        }
                }
        }

        return accuracy;
}

static void
on_agent_props_changed (GDBusProxy *agent_proxy,
                        GVariant   *changed_properties,
                        GStrv       invalidated_properties,
                        gpointer    user_data)
{
        GClueServiceClient *client = GCLUE_SERVICE_CLIENT (user_data);
        GClueDBusClient *gdbus_client;
        GVariantIter *iter;
        GVariant *value;
        gchar *key;
        
        if (g_variant_n_children (changed_properties) <= 0)
                return;

        g_variant_get (changed_properties, "a{sv}", &iter);
        while (g_variant_iter_loop (iter, "{&sv}", &key, &value)) {
                GClueAccuracyLevel max_accuracy;
                const char *id;
                gboolean system_app;

                if (strcmp (key, "MaxAccuracyLevel") != 0)
                        continue;

                gdbus_client = GCLUE_DBUS_CLIENT (client);
                id = gclue_dbus_client_get_desktop_id (gdbus_client);
                max_accuracy = g_variant_get_uint32 (value);
                system_app = (gclue_client_info_get_xdg_id
                              (client->priv->client_info) == NULL);
                /* FIXME: We should be handling all values of max accuracy
                 *        level here, not just 0 and non-0.
                 */
                if (max_accuracy != 0 && client->priv->agent_stopped) {
                        GClueAccuracyLevel accuracy;

                        client->priv->agent_stopped = FALSE;
                        accuracy = gclue_dbus_client_get_requested_accuracy_level
                                (gdbus_client);
                        accuracy = ensure_valid_accuracy_level (accuracy, max_accuracy);

                        start_client (client, accuracy);
                        g_debug ("Re-started '%s'.", id);
                } else if (max_accuracy == 0 &&
                           gclue_dbus_client_get_active (gdbus_client) &&
                           !system_app) {
                        stop_client (client);
                        client->priv->agent_stopped = TRUE;
                        g_debug ("Stopped '%s'.", id);
                }

                break;
        }
        g_variant_iter_free (iter);
}

struct _StartData
{
        GClueServiceClient *client;
        GDBusMethodInvocation *invocation;
        char *desktop_id;
        GClueAccuracyLevel accuracy_level;
};

static void
start_data_free (StartData *data)
{
        g_object_unref (data->client);
        g_object_unref (data->invocation);
        g_free(data->desktop_id);
        g_slice_free (StartData, data);
}

static void
complete_start (StartData *data)
{
        GClueDBusClient *gdbus_client = GCLUE_DBUS_CLIENT (data->client);
        start_client (data->client, data->accuracy_level);

        gclue_dbus_client_complete_start (gdbus_client,
                                          data->invocation);
        g_debug ("'%s' started.",
                 gclue_dbus_client_get_desktop_id (gdbus_client));
        start_data_free (data);
}

static void
on_authorize_app_ready (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
        StartData *data = (StartData *) user_data;
        GClueServiceClientPrivate *priv = data->client->priv;
        GError *error = NULL;
        gboolean authorized = FALSE;

        if (!gclue_agent_call_authorize_app_finish (GCLUE_AGENT (source_object),
                                                    &authorized,
                                                    &data->accuracy_level,
                                                    res,
                                                    &error))
                goto error_out;

        if (!authorized) {
                guint32 uid;

                uid = gclue_client_info_get_user_id (priv->client_info);

                g_set_error (&error,
                             G_DBUS_ERROR,
                             G_DBUS_ERROR_ACCESS_DENIED,
                             "Agent rejected '%s' for user '%u'. Please ensure "
                             "that '%s' has installed a valid %s.desktop file.",
                             data->desktop_id,
                             uid,
                             data->desktop_id,
                             data->desktop_id);
                goto error_out;
        }

        complete_start (data);

        return;

error_out:
        g_dbus_method_invocation_take_error (data->invocation, error);
        start_data_free (data);
}

static void
handle_post_agent_check_auth (StartData *data)
{
        GClueServiceClientPrivate *priv = GCLUE_SERVICE_CLIENT (data->client)->priv;
        GClueAccuracyLevel max_accuracy;
        GClueConfig *config;
        GClueAppPerm app_perm;
        guint32 uid;
        gboolean system_app;

        uid = gclue_client_info_get_user_id (priv->client_info);
        max_accuracy = gclue_agent_get_max_accuracy_level (priv->agent_proxy);

        if (max_accuracy == 0) {
                // Agent disabled geolocation for the user
                g_dbus_method_invocation_return_error (data->invocation,
                                                       G_DBUS_ERROR,
                                                       G_DBUS_ERROR_ACCESS_DENIED,
                                                       "Geolocation disabled for"
                                                       " UID %u",
                                                       uid);
                start_data_free (data);
                return;
        }
        g_debug ("requested accuracy level: %u. "
                 "Max accuracy level allowed by agent: %u",
                 data->accuracy_level, max_accuracy);
        data->accuracy_level = ensure_valid_accuracy_level (data->accuracy_level, max_accuracy);

        config = gclue_config_get_singleton ();
        app_perm = gclue_config_get_app_perm (config,
                                              data->desktop_id,
                                              priv->client_info);
        system_app = (gclue_client_info_get_xdg_id (priv->client_info) == NULL);

        if (app_perm == GCLUE_APP_PERM_ALLOWED || system_app) {
                /* Since we have no reliable way to identify system apps, no
                 * need for auth for them. */
                complete_start (data);
                return;
        }

        gclue_agent_call_authorize_app (priv->agent_proxy,
                                        data->desktop_id,
                                        data->accuracy_level,
                                        NULL,
                                        on_authorize_app_ready,
                                        data);
}

static gboolean
handle_pending_auth (gpointer user_data)
{
        GClueServiceClientPrivate *priv = GCLUE_SERVICE_CLIENT (user_data)->priv;
        StartData *data = priv->pending_auth_start_data;
        guint32 uid;

        g_return_val_if_fail (data != NULL, G_SOURCE_REMOVE);

        uid = gclue_client_info_get_user_id (priv->client_info);
        if (priv->agent_proxy == NULL) {
                GClueConfig *config = gclue_config_get_singleton ();

                if (gclue_config_get_num_allowed_agents (config) == 0) {
                        /* If there are no white-listed agents, there is no
                         * point in requiring an agent */
                        complete_start (data);
                } else {
                        g_dbus_method_invocation_return_error
                                (data->invocation,
                                 G_DBUS_ERROR,
                                 G_DBUS_ERROR_ACCESS_DENIED,
                                 "'%s' disallowed, no agent "
                                 "for UID %u",
                                 data->desktop_id,
                                 uid);
                        start_data_free (data);
                }
        } else {
                handle_post_agent_check_auth (data);
        }

        priv->pending_auth_timeout_id = 0;
        priv->pending_auth_start_data = NULL;

        return G_SOURCE_REMOVE;
}

static void
set_pending_auth_timeout_enable (GClueDBusClient *client)
{
        GClueServiceClientPrivate *priv = GCLUE_SERVICE_CLIENT (client)->priv;

        if (priv->pending_auth_timeout_id > 0)
                return;
        priv->pending_auth_timeout_id = g_timeout_add_seconds
                (DEFAULT_AGENT_STARTUP_WAIT_SECS, handle_pending_auth, client);
}

static void
set_pending_auth_timeout_disable (GClueDBusClient *client)
{
        GClueServiceClientPrivate *priv = GCLUE_SERVICE_CLIENT (client)->priv;

        if (priv->pending_auth_timeout_id == 0)
                return;

        g_source_remove (priv->pending_auth_timeout_id);
        priv->pending_auth_timeout_id = 0;
}

static gboolean
gclue_service_client_handle_start (GClueDBusClient       *client,
                                   GDBusMethodInvocation *invocation)
{
        GClueServiceClientPrivate *priv = GCLUE_SERVICE_CLIENT (client)->priv;
        GClueConfig *config;
        StartData *data;
        const char *desktop_id;
        GClueAppPerm app_perm;
        guint32 uid;

        if (priv->locator != NULL) {
                /* Already started */
                gclue_dbus_client_complete_start (client, invocation);

                return TRUE;
        }

        desktop_id = gclue_client_info_get_xdg_id (priv->client_info);
        if (desktop_id == NULL) {
                /* Non-xdg app */
                desktop_id = gclue_dbus_client_get_desktop_id (client);
        }

        if (desktop_id == NULL) {
                g_dbus_method_invocation_return_error_literal (invocation,
                                                               G_DBUS_ERROR,
                                                               G_DBUS_ERROR_ACCESS_DENIED,
                                                               "'DesktopId' property must be set");
                return TRUE;
        }

        config = gclue_config_get_singleton ();
        uid = gclue_client_info_get_user_id (priv->client_info);
        app_perm = gclue_config_get_app_perm (config,
                                              desktop_id,
                                              priv->client_info);
        if (app_perm == GCLUE_APP_PERM_DISALLOWED) {
                g_dbus_method_invocation_return_error (invocation,
                                                       G_DBUS_ERROR,
                                                       G_DBUS_ERROR_ACCESS_DENIED,
                                                       "'%s' disallowed by "
                                                       "configuration for UID %u",
                                                       desktop_id,
                                                       uid);
                return TRUE;
        }

        data = g_slice_new (StartData);
        data->client = g_object_ref (GCLUE_SERVICE_CLIENT (client));
        data->invocation =  g_object_ref (invocation);
        data->desktop_id =  g_strdup (desktop_id);

        data->accuracy_level = gclue_dbus_client_get_requested_accuracy_level (client);
        data->accuracy_level = ensure_valid_accuracy_level
                (data->accuracy_level, GCLUE_ACCURACY_LEVEL_EXACT);

        /* No agent == No authorization */
        if (priv->agent_proxy == NULL) {
                /* Already a pending Start()? Denied! */
                if (priv->pending_auth_start_data) {
                        g_dbus_method_invocation_return_error_literal
                                (invocation,
                                 G_DBUS_ERROR,
                                 G_DBUS_ERROR_ACCESS_DENIED,
                                 "An authorization request is already pending");
                } else {
                        priv->pending_auth_start_data = data;
                        set_pending_auth_timeout_enable (client);
                }
                return TRUE;
        }

        handle_post_agent_check_auth (data);

        return TRUE;
}

static gboolean
gclue_service_client_handle_stop (GClueDBusClient       *client,
                                  GDBusMethodInvocation *invocation)
{
        stop_client (GCLUE_SERVICE_CLIENT (client));
        gclue_dbus_client_complete_stop (client, invocation);
        g_debug ("'%s' stopped.", gclue_dbus_client_get_desktop_id (client));

        return TRUE;
}

static void
gclue_service_client_finalize (GObject *object)
{
        GClueServiceClientPrivate *priv = GCLUE_SERVICE_CLIENT (object)->priv;

        g_clear_pointer (&priv->path, g_free);
        g_clear_object (&priv->connection);
        set_pending_auth_timeout_disable (GCLUE_DBUS_CLIENT (object));
        g_clear_pointer (&priv->pending_auth_start_data, start_data_free);
        if (priv->agent_proxy != NULL)
                g_signal_handlers_disconnect_by_func
                                (priv->agent_proxy,
                                 G_CALLBACK (on_agent_props_changed),
                                 object);
        g_clear_object (&priv->agent_proxy);
        g_clear_object (&priv->locator);
        g_clear_object (&priv->location);
        g_clear_object (&priv->prev_location);
        g_clear_object (&priv->client_info);

        /* Chain up to the parent class */
        G_OBJECT_CLASS (gclue_service_client_parent_class)->finalize (object);
}

static void
gclue_service_client_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        GClueServiceClient *client = GCLUE_SERVICE_CLIENT (object);

        switch (prop_id) {
        case PROP_CLIENT_INFO:
                g_value_set_object (value, client->priv->client_info);
                break;

        case PROP_PATH:
                g_value_set_string (value, client->priv->path);
                break;

        case PROP_CONNECTION:
                g_value_set_object (value, client->priv->connection);
                break;

        case PROP_AGENT_PROXY:
                g_value_set_object (value, client->priv->agent_proxy);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_service_client_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
        GClueServiceClient *client = GCLUE_SERVICE_CLIENT (object);

        switch (prop_id) {
        case PROP_CLIENT_INFO:
                client->priv->client_info = g_value_dup_object (value);
                break;

        case PROP_PATH:
                client->priv->path = g_value_dup_string (value);
                break;

        case PROP_CONNECTION:
                client->priv->connection = g_value_dup_object (value);
                break;

        case PROP_AGENT_PROXY:
                client->priv->agent_proxy = g_value_dup_object (value);
                if (client->priv->agent_proxy != NULL)
                        g_signal_connect (client->priv->agent_proxy,
                                          "g-properties-changed",
                                          G_CALLBACK (on_agent_props_changed),
                                          object);
                if (client->priv->pending_auth_start_data != NULL)
                        handle_pending_auth (client);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_service_client_handle_method_call (GDBusConnection       *connection,
                                         const gchar           *sender,
                                         const gchar           *object_path,
                                         const gchar           *interface_name,
                                         const gchar           *method_name,
                                         GVariant              *parameters,
                                         GDBusMethodInvocation *invocation,
                                         gpointer               user_data)
{
        GClueServiceClientPrivate *priv = GCLUE_SERVICE_CLIENT (user_data)->priv;
        GDBusInterfaceSkeletonClass *skeleton_class;
        GDBusInterfaceVTable *skeleton_vtable;

        if (!gclue_client_info_check_bus_name (priv->client_info, sender)) {
                g_dbus_method_invocation_return_error_literal (invocation,
                                                               G_DBUS_ERROR,
                                                               G_DBUS_ERROR_ACCESS_DENIED,
                                                               "Access denied");
                return;
        }

        skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (gclue_service_client_parent_class);
        skeleton_vtable = skeleton_class->get_vtable (G_DBUS_INTERFACE_SKELETON (user_data));
        skeleton_vtable->method_call (connection,
                                      sender,
                                      object_path,
                                      interface_name,
                                      method_name,
                                      parameters,
                                      invocation,
                                      user_data);
}

static GVariant *
gclue_service_client_handle_get_property (GDBusConnection *connection,
                                          const gchar     *sender,
                                          const gchar     *object_path,
                                          const gchar     *interface_name,
                                          const gchar     *property_name,
                                          GError         **error,
                                          gpointer        user_data)
{
        GClueServiceClientPrivate *priv = GCLUE_SERVICE_CLIENT (user_data)->priv;
        GDBusInterfaceSkeletonClass *skeleton_class;
        GDBusInterfaceVTable *skeleton_vtable;

        if (!gclue_client_info_check_bus_name (priv->client_info, sender)) {
                g_set_error (error,
                             G_DBUS_ERROR,
                             G_DBUS_ERROR_ACCESS_DENIED,
                             "Access denied");
                return NULL;
        }

        skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (gclue_service_client_parent_class);
        skeleton_vtable = skeleton_class->get_vtable (G_DBUS_INTERFACE_SKELETON (user_data));
        return skeleton_vtable->get_property (connection,
                                              sender,
                                              object_path,
                                              interface_name,
                                              property_name,
                                              error,
                                              user_data);
}

static gboolean
gclue_service_client_handle_set_property (GDBusConnection *connection,
                                          const gchar     *sender,
                                          const gchar     *object_path,
                                          const gchar     *interface_name,
                                          const gchar     *property_name,
                                          GVariant        *variant,
                                          GError         **error,
                                          gpointer        user_data)
{
        GClueDBusClient *client = GCLUE_DBUS_CLIENT (user_data);
        GClueServiceClientPrivate *priv = GCLUE_SERVICE_CLIENT (client)->priv;
        GDBusInterfaceSkeletonClass *skeleton_class;
        GDBusInterfaceVTable *skeleton_vtable;
        gboolean ret;

        if (!gclue_client_info_check_bus_name (priv->client_info, sender)) {
                g_set_error (error,
                             G_DBUS_ERROR,
                             G_DBUS_ERROR_ACCESS_DENIED,
                             "Access denied");
                return FALSE;
        }

        skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (gclue_service_client_parent_class);
        skeleton_vtable = skeleton_class->get_vtable (G_DBUS_INTERFACE_SKELETON (user_data));
        ret = skeleton_vtable->set_property (connection,
                                             sender,
                                             object_path,
                                             interface_name,
                                             property_name,
                                             variant,
                                             error,
                                             user_data);
        if (ret && strcmp (property_name, "DistanceThreshold") == 0) {
                priv->distance_threshold = gclue_dbus_client_get_distance_threshold
                        (client);
                g_debug ("New distance threshold: %u", priv->distance_threshold);
        } else if (ret && strcmp (property_name, "TimeThreshold") == 0) {
                priv->time_threshold = gclue_dbus_client_get_time_threshold
                        (client);
                gclue_locator_set_time_threshold (priv->locator,
                                                  priv->time_threshold);
                g_debug ("%s: New time-threshold:  %u",
                         G_OBJECT_TYPE_NAME (client),
                         priv->time_threshold);
        }

        return ret;
}

static const GDBusInterfaceVTable gclue_service_client_vtable =
{
        gclue_service_client_handle_method_call,
        gclue_service_client_handle_get_property,
        gclue_service_client_handle_set_property,
        {NULL}
};

static GDBusInterfaceVTable *
gclue_service_client_get_vtable (GDBusInterfaceSkeleton *skeleton G_GNUC_UNUSED)
{
        return (GDBusInterfaceVTable *) &gclue_service_client_vtable;
}

static void
gclue_service_client_class_init (GClueServiceClientClass *klass)
{
        GObjectClass *object_class;
        GDBusInterfaceSkeletonClass *skeleton_class;

        object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gclue_service_client_finalize;
        object_class->get_property = gclue_service_client_get_property;
        object_class->set_property = gclue_service_client_set_property;

        skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (klass);
        skeleton_class->get_vtable = gclue_service_client_get_vtable;

        gParamSpecs[PROP_CLIENT_INFO] = g_param_spec_object ("client-info",
                                                             "ClientInfo",
                                                             "Information on client",
                                                             GCLUE_TYPE_CLIENT_INFO,
                                                             G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class,
                                         PROP_CLIENT_INFO,
                                         gParamSpecs[PROP_CLIENT_INFO]);

        gParamSpecs[PROP_PATH] = g_param_spec_string ("path",
                                                      "Path",
                                                      "Path",
                                                      NULL,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class,
                                         PROP_PATH,
                                         gParamSpecs[PROP_PATH]);

        gParamSpecs[PROP_CONNECTION] = g_param_spec_object ("connection",
                                                            "Connection",
                                                            "DBus Connection",
                                                            G_TYPE_DBUS_CONNECTION,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class,
                                         PROP_CONNECTION,
                                         gParamSpecs[PROP_CONNECTION]);

        gParamSpecs[PROP_AGENT_PROXY] = g_param_spec_object ("agent-proxy",
                                                             "AgentProxy",
                                                             "Proxy to app authorization agent",
                                                             GCLUE_TYPE_AGENT_PROXY,
                                                             G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT);
        g_object_class_install_property (object_class,
                                         PROP_AGENT_PROXY,
                                         gParamSpecs[PROP_AGENT_PROXY]);
}

static void
gclue_service_client_client_iface_init (GClueDBusClientIface *iface)
{
        iface->handle_start = gclue_service_client_handle_start;
        iface->handle_stop = gclue_service_client_handle_stop;
}

static gboolean
gclue_service_client_initable_init (GInitable    *initable,
                                    GCancellable *cancellable,
                                    GError      **error)
{
        if (!g_dbus_interface_skeleton_export
                                (G_DBUS_INTERFACE_SKELETON (initable),
                                 GCLUE_SERVICE_CLIENT (initable)->priv->connection,
                                 GCLUE_SERVICE_CLIENT (initable)->priv->path,
                                 error))
                return FALSE;

        return TRUE;
}

static void
gclue_service_client_initable_iface_init (GInitableIface *iface)
{
        iface->init = gclue_service_client_initable_init;
}

static void
gclue_service_client_init (GClueServiceClient *client)
{
        client->priv = G_TYPE_INSTANCE_GET_PRIVATE (client,
                                                    GCLUE_TYPE_SERVICE_CLIENT,
                                                    GClueServiceClientPrivate);
        gclue_dbus_client_set_requested_accuracy_level
                (GCLUE_DBUS_CLIENT (client), DEFAULT_ACCURACY_LEVEL);
}

GClueServiceClient *
gclue_service_client_new (GClueClientInfo *info,
                          const char      *path,
                          GDBusConnection *connection,
                          GClueAgent      *agent_proxy,
                          GError         **error)
{
        return g_initable_new (GCLUE_TYPE_SERVICE_CLIENT,
                               NULL,
                               error,
                               "client-info", info,
                               "path", path,
                               "connection", connection,
                               "agent-proxy", agent_proxy,
                               NULL);
}

const gchar *
gclue_service_client_get_path (GClueServiceClient *client)
{
        g_return_val_if_fail (GCLUE_IS_SERVICE_CLIENT(client), NULL);

        return client->priv->path;
}

GClueClientInfo *
gclue_service_client_get_client_info (GClueServiceClient *client)
{
        g_return_val_if_fail (GCLUE_IS_SERVICE_CLIENT(client), NULL);

        return client->priv->client_info;
}
