/* vim: set et ts=8 sw=8: */
/* where-am-i.c
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

#include <config.h>

#include <stdlib.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <geoclue.h>

/* Commandline options */
static gint timeout = 30; /* seconds */
static GClueAccuracyLevel accuracy_level = GCLUE_ACCURACY_LEVEL_EXACT;

static GOptionEntry entries[] =
{
        { "timeout",
          't',
          0,
          G_OPTION_ARG_INT,
          &timeout,
          N_("Exit after T seconds. Default: 30"),
          "T" },
        { "accuracy-level",
          'a',
          0,
          G_OPTION_ARG_INT,
          &accuracy_level,
          N_("Request accuracy level A. "
             "Country = 1, "
             "City = 4, "
             "Neighborhood = 5, "
             "Street = 6, "
             "Exact = 8."),
          "A" },
        { NULL }
};

GClueSimple *simple = NULL;
GClueClient *client = NULL;
GMainLoop *main_loop;

static gboolean
on_location_timeout (gpointer user_data)
{
        g_clear_object (&client);
        g_clear_object (&simple);
        g_main_loop_quit (main_loop);

        return FALSE;
}

static void
print_location (GClueSimple *simple)
{
        GClueLocation *location;
        gdouble altitude, speed, heading;
        GVariant *timestamp;
        GTimeVal tv = { 0 };
        const char *desc;

        location = gclue_simple_get_location (simple);
        g_print ("\nNew location:\n");
        g_print ("Latitude:    %f°\nLongitude:   %f°\nAccuracy:    %f meters\n",
                 gclue_location_get_latitude (location),
                 gclue_location_get_longitude (location),
                 gclue_location_get_accuracy (location));

        altitude = gclue_location_get_altitude (location);
        if (altitude != -G_MAXDOUBLE)
                g_print ("Altitude:    %f meters\n", altitude);
        speed = gclue_location_get_speed (location);
        if (speed >= 0)
                g_print ("Speed:       %f meters/second\n", speed);
        heading = gclue_location_get_heading (location);
        if (heading >= 0)
                g_print ("Heading:     %f°\n", heading);

        desc = gclue_location_get_description (location);
        if (strlen (desc) > 0)
                g_print ("Description: %s\n", desc);

        timestamp = gclue_location_get_timestamp (location);
        if (timestamp) {
                GDateTime *date_time;
                gchar *str;

                g_variant_get (timestamp, "(tt)", &tv.tv_sec, &tv.tv_usec);

                date_time = g_date_time_new_from_timeval_local (&tv);
                str = g_date_time_format
                      (date_time,
                       "%c (%s seconds since the Epoch)");
                g_date_time_unref (date_time);

                g_print ("Timestamp:   %s\n", str);
                g_free (str);
        }
}

static void
on_client_active_notify (GClueClient *client,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
        if (gclue_client_get_active (client))
                return;

        g_print ("Geolocation disabled. Quiting..\n");
        on_location_timeout (NULL);
}

static void
on_simple_ready (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
        GError *error = NULL;

        simple = gclue_simple_new_finish (res, &error);
        if (error != NULL) {
            g_critical ("Failed to connect to GeoClue2 service: %s", error->message);

            exit (-1);
        }
        client = gclue_simple_get_client (simple);
        g_object_ref (client);
        g_print ("Client object: %s\n",
                 g_dbus_proxy_get_object_path (G_DBUS_PROXY (client)));

        print_location (simple);

        g_signal_connect (simple,
                          "notify::location",
                          G_CALLBACK (print_location),
                          NULL);
        g_signal_connect (client,
                          "notify::active",
                          G_CALLBACK (on_client_active_notify),
                          NULL);
}

gint
main (gint argc, gchar *argv[])
{
        GOptionContext *context;
        GError *error = NULL;

        setlocale (LC_ALL, "");
        textdomain (GETTEXT_PACKAGE);
        bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

        context = g_option_context_new ("- Where am I?");
        g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_critical ("option parsing failed: %s\n", error->message);
                exit (-1);
        }
        g_option_context_free (context);

        g_timeout_add_seconds (timeout, on_location_timeout, NULL);

        gclue_simple_new ("geoclue-where-am-i",
                          accuracy_level,
                          NULL,
                          on_simple_ready,
                          NULL);

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);

        return EXIT_SUCCESS;
}
