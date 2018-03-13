/* vim: set et ts=8 sw=8: */
/*
 * Copyright (C) 2014 Red Hat, Inc.
 * Copyright (C) 2015 Ankit (Verma)
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
 * Authors: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *          Ankit (Verma) <ankitstarski@gmail.com>
 */

#include <stdlib.h>
#include <glib.h>
#include "gclue-nmea-source.h"
#include "gclue-location.h"
#include "config.h"
#include "gclue-enum-types.h"

#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-glib/glib-watch.h>

typedef struct AvahiServiceInfo AvahiServiceInfo;

struct _GClueNMEASourcePrivate {
        GSocketConnection *connection;

        GSocketClient *client;

        GCancellable *cancellable;

        AvahiClient *avahi_client;

        AvahiServiceInfo *active_service;

        /* List of all services but only the most accurate one is used. */
        GList *all_services;
};

G_DEFINE_TYPE (GClueNMEASource, gclue_nmea_source, GCLUE_TYPE_LOCATION_SOURCE)

static gboolean
gclue_nmea_source_start (GClueLocationSource *source);
static gboolean
gclue_nmea_source_stop (GClueLocationSource *source);

static void
connect_to_service (GClueNMEASource *source);
static void
disconnect_from_service (GClueNMEASource *source);

struct AvahiServiceInfo {
    char *identifier;
    char *host_name;
    guint16 port;
    GClueAccuracyLevel accuracy;
    guint64 timestamp;
};

static void
avahi_service_free (gpointer data)
{
        AvahiServiceInfo *service = (AvahiServiceInfo *) data;

        g_free (service->identifier);
        g_free (service->host_name);
        g_slice_free(AvahiServiceInfo, service);
}

static AvahiServiceInfo *
avahi_service_new (const char        *identifier,
                   const char        *host_name,
                   guint16            port,
                   GClueAccuracyLevel accuracy)
{
        GTimeVal tv;

        AvahiServiceInfo *service = g_slice_new0 (AvahiServiceInfo);

        service->identifier = g_strdup (identifier);
        service->host_name = g_strdup (host_name);
        service->port = port;
        service->accuracy = accuracy;
        g_get_current_time (&tv);
        service->timestamp = tv.tv_sec;

        return service;
}

static gint
compare_avahi_service_by_identifier (gconstpointer a,
                                     gconstpointer b)
{
        AvahiServiceInfo *first, *second;

        first = (AvahiServiceInfo *) a;
        second = (AvahiServiceInfo *) b;

        return g_strcmp0 (first->identifier, second->identifier);
}

static gint
compare_avahi_service_by_accuracy_n_time (gconstpointer a,
                                          gconstpointer b)
{
        AvahiServiceInfo *first, *second;
        gint diff;

        first = (AvahiServiceInfo *) a;
        second = (AvahiServiceInfo *) b;

        diff = second->accuracy - first->accuracy;

        if (diff == 0)
                return first->timestamp - second->timestamp;

        return diff;
}

static gboolean
reconnection_required (GClueNMEASource *source)
{
        GClueNMEASourcePrivate *priv = source->priv;

        /* Basically, reconnection is required if either
         *
         * 1. service in use went down.
         * 2. a more accurate service than one currently in use, is now
         *    available.
         */
        return (priv->active_service != NULL &&
                (priv->all_services == NULL ||
                 priv->active_service != priv->all_services->data));
}

static void
reconnect_service (GClueNMEASource *source)
{
        if (!reconnection_required (source))
                return;

        disconnect_from_service (source);
        connect_to_service (source);
}

static void
refresh_accuracy_level (GClueNMEASource *source)
{
        GClueAccuracyLevel new, existing;

        existing = gclue_location_source_get_available_accuracy_level
                        (GCLUE_LOCATION_SOURCE (source));

        if (source->priv->all_services != NULL) {
                AvahiServiceInfo *service;

                service = (AvahiServiceInfo *) source->priv->all_services->data;
                new = service->accuracy;
        } else {
                new = GCLUE_ACCURACY_LEVEL_NONE;
        }

        if (new != existing) {
                g_debug ("Available accuracy level from %s: %u",
                         G_OBJECT_TYPE_NAME (source), new);
                g_object_set (G_OBJECT (source),
                              "available-accuracy-level", new,
                              NULL);
        }
}

static void
add_new_service (GClueNMEASource *source,
                 const char *name,
                 const char *host_name,
                 uint16_t port,
                 AvahiStringList *txt)
{
        GClueAccuracyLevel accuracy = GCLUE_ACCURACY_LEVEL_NONE;
        AvahiServiceInfo *service;
        AvahiStringList *node;
        guint n_services;
        char *key, *value;
        GEnumClass *enum_class;
        GEnumValue *enum_value;

        node = avahi_string_list_find (txt, "accuracy");

        if (node == NULL) {
                g_warning ("No `accuracy` key inside TXT record");
                accuracy = GCLUE_ACCURACY_LEVEL_EXACT;

                goto CREATE_SERVICE;
        }

        avahi_string_list_get_pair (node, &key, &value, NULL);

        if (value == NULL) {
                g_warning ("There is no value for `accuracy` inside TXT "
                           "record");
                accuracy = GCLUE_ACCURACY_LEVEL_EXACT;

                goto CREATE_SERVICE;
        }

        enum_class = g_type_class_ref (GCLUE_TYPE_ACCURACY_LEVEL);
        enum_value = g_enum_get_value_by_nick (enum_class, value);
        g_type_class_unref (enum_class);

        if (enum_value == NULL) {
                g_warning ("Invalid `accuracy` value `%s` inside TXT records.",
                           value);
                accuracy = GCLUE_ACCURACY_LEVEL_EXACT;

                goto CREATE_SERVICE;
        }

        accuracy = enum_value->value;

CREATE_SERVICE:
        service = avahi_service_new (name, host_name, port, accuracy);

        source->priv->all_services = g_list_insert_sorted
                (source->priv->all_services,
                 service,
                 compare_avahi_service_by_accuracy_n_time);

        refresh_accuracy_level (source);
        reconnect_service (source);

        n_services = g_list_length (source->priv->all_services);

        g_debug ("No. of _nmea-0183._tcp services %u", n_services);
}

static void
remove_service (GClueNMEASource *source,
                AvahiServiceInfo *service)
{
        guint n_services = 0;

        avahi_service_free (service);
        source->priv->all_services = g_list_remove
                (source->priv->all_services, service);

        n_services = g_list_length (source->priv->all_services);

        g_debug ("No. of _nmea-0183._tcp services %u",
                 n_services);

        refresh_accuracy_level (source);
        reconnect_service (source);
}

static void
remove_service_by_name (GClueNMEASource *source,
                        const char      *name)
{
        AvahiServiceInfo *service;
        GList *item;

        /* only `name` is required here */
        service = avahi_service_new (name,
                                     NULL,
                                     0,
                                     GCLUE_ACCURACY_LEVEL_NONE);

        item = g_list_find_custom (source->priv->all_services,
                                   service,
                                   compare_avahi_service_by_identifier);
        avahi_service_free (service);

        if (item == NULL)
                return;

        remove_service (source, item->data);
}

static void
resolve_callback (AvahiServiceResolver  *service_resolver,
                  AvahiIfIndex           interface G_GNUC_UNUSED,
                  AvahiProtocol          protocol G_GNUC_UNUSED,
                  AvahiResolverEvent     event,
                  const char            *name,
                  const char            *type,
                  const char            *domain,
                  const char            *host_name,
                  const AvahiAddress    *address,
                  uint16_t               port,
                  AvahiStringList       *txt,
                  AvahiLookupResultFlags flags,
                  void                  *user_data)
{
        const char *errorstr;

        /* FIXME: check with Avahi devs whether this is really needed. */
        g_return_if_fail (service_resolver != NULL);

        switch (event) {
        case AVAHI_RESOLVER_FAILURE: {
                AvahiClient *avahi_client = avahi_service_resolver_get_client
                        (service_resolver);

                errorstr = avahi_strerror (avahi_client_errno (avahi_client));

                g_warning ("(Resolver) Failed to resolve service '%s' "
                           "of type '%s' in domain '%s': %s",
                           name,
                           type,
                           domain,
                           errorstr);

                break;
        }

        case AVAHI_RESOLVER_FOUND:
                g_debug ("Service %s:%u resolved",
                         host_name,
                         port);

                add_new_service (GCLUE_NMEA_SOURCE (user_data),
                                 name,
                                 host_name,
                                 port,
                                 txt);

                break;
        }

    avahi_service_resolver_free (service_resolver);
}

static void
client_callback (AvahiClient     *avahi_client,
                 AvahiClientState state,
                 void            *user_data)
{
        GClueNMEASourcePrivate *priv = GCLUE_NMEA_SOURCE (user_data)->priv;

        g_return_if_fail (avahi_client != NULL);

        priv->avahi_client = avahi_client;

        if (state == AVAHI_CLIENT_FAILURE) {
                const char *errorstr = avahi_strerror
                        (avahi_client_errno (avahi_client));
                g_warning ("Avahi client failure: %s",
                           errorstr);
        }
}

static void
browse_callback (AvahiServiceBrowser   *service_browser,
                 AvahiIfIndex           interface,
                 AvahiProtocol          protocol,
                 AvahiBrowserEvent      event,
                 const char            *name,
                 const char            *type,
                 const char            *domain,
                 AvahiLookupResultFlags flags G_GNUC_UNUSED,
                 void                  *user_data)
{
        GClueNMEASourcePrivate *priv = GCLUE_NMEA_SOURCE (user_data)->priv;
        const char *errorstr;

        /* FIXME: check with Avahi devs whether this is really needed. */
        g_return_if_fail (service_browser != NULL);

        switch (event) {
        case AVAHI_BROWSER_FAILURE:
                errorstr = avahi_strerror (avahi_client_errno
                        (avahi_service_browser_get_client (service_browser)));

                g_warning ("Avahi service browser Error %s", errorstr);

                return;

        case AVAHI_BROWSER_NEW: {
                AvahiServiceResolver *service_resolver;

                g_debug ("Service '%s' of type '%s' found in domain '%s'",
                         name, type, domain);

                service_resolver = avahi_service_resolver_new
                        (priv->avahi_client,
                         interface, protocol,
                         name, type,
                         domain,
                         AVAHI_PROTO_UNSPEC,
                         0,
                         resolve_callback,
                         user_data);

                if (service_resolver == NULL) {
                        errorstr = avahi_strerror
                                (avahi_client_errno (priv->avahi_client));

                        g_warning ("Failed to resolve service '%s': %s",
                                   name,
                                   errorstr);
                }

                break;
        }

        case AVAHI_BROWSER_REMOVE:
                g_debug ("Service '%s' of type '%s' in domain '%s' removed "
                         "from the list of available NMEA services",
                         name,
                         type,
                         domain);

                remove_service_by_name (GCLUE_NMEA_SOURCE (user_data), name);

                break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
                g_debug ("Avahi Service Browser's %s event occurred",
                         event == AVAHI_BROWSER_CACHE_EXHAUSTED ?
                         "CACHE_EXHAUSTED" :
                         "ALL_FOR_NOW");

                break;
        }
}

static void
on_read_gga_sentence (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
        GClueNMEASource *source = GCLUE_NMEA_SOURCE (user_data);
        GDataInputStream *data_input_stream = G_DATA_INPUT_STREAM (object);
        GError *error = NULL;
        GClueLocation *location;
        gsize data_size = 0 ;
        char *message;

        message = g_data_input_stream_read_line_finish (data_input_stream,
                                                        result,
                                                        &data_size,
                                                        &error);

        if (message == NULL) {
                if (error != NULL) {
                        if (error->code == G_IO_ERROR_CLOSED)
                                g_debug ("Socket closed.");
                        else if (error->code != G_IO_ERROR_CANCELLED)
                                g_warning ("Error when receiving message: %s",
                                           error->message);
                        g_error_free (error);
                } else {
                        g_debug ("Nothing to read");
                }
                g_object_unref (data_input_stream);

                if (source->priv->active_service != NULL)
                        /* In case service did not advertise it exiting
                         * or we failed to receive it's notification.
                         */
                        remove_service (source, source->priv->active_service);

                return;
        }
        g_debug ("Network source sent: \"%s\"", message);

        if (!g_str_has_prefix (message, "$GPGGA")) {
                /* FIXME: Handle other useful NMEA sentences too */
                g_debug ("Ignoring non-GGA sentence from NMEA source");

                goto READ_NEXT_LINE;
        }

        location = gclue_location_create_from_gga (message, &error);

        if (error != NULL) {
                g_warning ("Error: %s", error->message);
                g_clear_error (&error);
        } else {
                gclue_location_source_set_location
                        (GCLUE_LOCATION_SOURCE (source), location);
        }

READ_NEXT_LINE:
        g_data_input_stream_read_line_async (data_input_stream,
                                             G_PRIORITY_DEFAULT,
                                             source->priv->cancellable,
                                             on_read_gga_sentence,
                                             source);
}

static void
on_connection_to_location_server (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
        GClueNMEASource *source = GCLUE_NMEA_SOURCE (user_data);
        GSocketClient *client = G_SOCKET_CLIENT (object);
        GError *error = NULL;
        GDataInputStream *data_input_stream;
        GInputStream *input_stream;

        source->priv->connection = g_socket_client_connect_to_host_finish
                (client,
                 result,
                 &error);

        if (error != NULL) {
                if (error->code != G_IO_ERROR_CANCELLED)
                        g_warning ("Failed to connect to NMEA service: %s", error->message);
                g_clear_error (&error);

                return;
        }

        input_stream = g_io_stream_get_input_stream
                (G_IO_STREAM (source->priv->connection));
        data_input_stream = g_data_input_stream_new (input_stream);

        g_data_input_stream_read_line_async (data_input_stream,
                                             G_PRIORITY_DEFAULT,
                                             source->priv->cancellable,
                                             on_read_gga_sentence,
                                             source);
}

static void
connect_to_service (GClueNMEASource *source)
{
        GClueNMEASourcePrivate *priv = source->priv;

        if (priv->all_services == NULL)
                return;

        priv->client = g_socket_client_new ();
        g_cancellable_reset (priv->cancellable);

        /* The service with the highest accuracy will be stored in the beginning
         * of the list.
         */
        priv->active_service = (AvahiServiceInfo *) priv->all_services->data;

        g_socket_client_connect_to_host_async
                (priv->client,
                 priv->active_service->host_name,
                 priv->active_service->port,
                 priv->cancellable,
                 on_connection_to_location_server,
                 source);
}

static void
disconnect_from_service (GClueNMEASource *source)
{
        GClueNMEASourcePrivate *priv = source->priv;

        g_cancellable_cancel (priv->cancellable);

        if (priv->connection != NULL) {
                GError *error = NULL;

                g_io_stream_close (G_IO_STREAM (priv->connection),
                                   NULL,
                                   &error);
                if (error != NULL)
                        g_warning ("Error in closing socket connection: %s", error->message);
        }

        g_clear_object (&priv->connection);
        g_clear_object (&priv->client);
        priv->active_service = NULL;
}

static void
gclue_nmea_source_finalize (GObject *gnmea)
{
        GClueNMEASourcePrivate *priv = GCLUE_NMEA_SOURCE (gnmea)->priv;

        G_OBJECT_CLASS (gclue_nmea_source_parent_class)->finalize (gnmea);

        g_clear_object (&priv->connection);
        g_clear_object (&priv->client);
        g_clear_object (&priv->cancellable);
        if (priv->avahi_client)
                avahi_client_free (priv->avahi_client);
        g_list_free_full (priv->all_services,
                          avahi_service_free);
}

static void
gclue_nmea_source_class_init (GClueNMEASourceClass *klass)
{
        GClueLocationSourceClass *source_class = GCLUE_LOCATION_SOURCE_CLASS (klass);
        GObjectClass *gnmea_class = G_OBJECT_CLASS (klass);

        gnmea_class->finalize = gclue_nmea_source_finalize;

        source_class->start = gclue_nmea_source_start;
        source_class->stop = gclue_nmea_source_stop;

        g_type_class_add_private (klass, sizeof (GClueNMEASourcePrivate));
}

static void
gclue_nmea_source_init (GClueNMEASource *source)
{
        GClueNMEASourcePrivate *priv;
        AvahiServiceBrowser *service_browser;
        const AvahiPoll *poll_api;
        AvahiGLibPoll *glib_poll;
        int error;

        source->priv = G_TYPE_INSTANCE_GET_PRIVATE ((source),
                                                    GCLUE_TYPE_NMEA_SOURCE,
                                                    GClueNMEASourcePrivate);
        priv = source->priv;

        glib_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
        poll_api = avahi_glib_poll_get (glib_poll);

        priv->cancellable = g_cancellable_new ();

        avahi_client_new (poll_api,
                          0,
                          client_callback,
                          source,
                          &error);

        if (priv->avahi_client == NULL) {
                g_warning ("Failed to connect to avahi service: %s",
                           avahi_strerror (error));
                return;
        }

        service_browser = avahi_service_browser_new
                (priv->avahi_client,
                 AVAHI_IF_UNSPEC,
                 AVAHI_PROTO_UNSPEC,
                 "_nmea-0183._tcp",
                 NULL,
                 0,
                 browse_callback,
                 source);


        if (service_browser == NULL) {
                const char *errorstr;

                error = avahi_client_errno (priv->avahi_client);
                errorstr = avahi_strerror (error);
                g_warning ("Failed to browse avahi services: %s", errorstr);
        }
}

/**
 * gclue_nmea_source_get_singleton:
 *
 * Get the #GClueNMEASource singleton.
 *
 * Returns: (transfer full): a new ref to #GClueNMEASource. Use g_object_unref()
 * when done.
 **/
GClueNMEASource *
gclue_nmea_source_get_singleton (void)
{
        static GClueNMEASource *source = NULL;

        if (source == NULL) {
                source = g_object_new (GCLUE_TYPE_NMEA_SOURCE, NULL);
                g_object_add_weak_pointer (G_OBJECT (source),
                                           (gpointer) &source);
        } else
                g_object_ref (source);

        return source;
}

static gboolean
gclue_nmea_source_start (GClueLocationSource *source)
{
        GClueLocationSourceClass *base_class;

        g_return_val_if_fail (GCLUE_IS_NMEA_SOURCE (source), FALSE);

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_nmea_source_parent_class);
        if (!base_class->start (source))
                return FALSE;

        connect_to_service (GCLUE_NMEA_SOURCE (source));

        return TRUE;
}

static gboolean
gclue_nmea_source_stop (GClueLocationSource *source)
{
        GClueLocationSourceClass *base_class;

        g_return_val_if_fail (GCLUE_IS_NMEA_SOURCE (source), FALSE);

        base_class = GCLUE_LOCATION_SOURCE_CLASS (gclue_nmea_source_parent_class);
        if (!base_class->stop (source))
                return FALSE;

        disconnect_from_service (GCLUE_NMEA_SOURCE (source));

        return TRUE;
}
