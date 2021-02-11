/* vim: set et ts=8 sw=8: */
/* main.c
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
#include <config.h>

#include <glib.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "gclue-service-manager.h"
#include "gclue-config.h"

#define BUS_NAME "org.freedesktop.GeoClue2"

/* Commandline options */
static gboolean version = FALSE;
static gint inactivity_timeout = 60;
static gboolean submit_data = FALSE;
static char *submit_nick = NULL;

static GOptionEntry entries[] =
{
        { "version",
          0,
          0,
          G_OPTION_ARG_NONE,
          &version,
          N_("Display version number"),
          NULL },
        { "timeout",
          't',
          0,
          G_OPTION_ARG_INT,
          &inactivity_timeout,
          N_("Exit after T seconds of inactivity, 0 for never. Default: 60"),
          "T" },
        { "submit-data",
          's',
          0,
          G_OPTION_ARG_NONE,
          &submit_data,
          N_("Enable submission of network data"),
          NULL },
        { "submit-nick",
          'n',
          0,
          G_OPTION_ARG_STRING,
          &submit_nick,
          N_("Nickname to submit network data under (2-32 characters)"),
          "NICK" },
        { NULL }
};

GMainLoop *main_loop;
GClueServiceManager *manager = NULL;
guint inactivity_timeout_id = 0;

static gboolean
on_inactivity_timeout (gpointer user_data)
{
        g_message ("Service not used for %d seconds. Shutting down..",
                   inactivity_timeout);
        g_main_loop_quit (main_loop);

        return FALSE;
}

static void
on_active_notify (GObject    *gobject,
                  GParamSpec *pspec,
                  gpointer    user_data)
{
        GClueServiceManager *manager = GCLUE_SERVICE_MANAGER (gobject);
        gboolean active;

        active = gclue_service_manager_get_active (manager);

        if (inactivity_timeout <= 0)
                return;

        g_debug ("Service %s in use", active? "now" : "not");

        if (!active)
                inactivity_timeout_id =
                        g_timeout_add_seconds (inactivity_timeout,
                                               on_inactivity_timeout,
                                               NULL);
        else if (inactivity_timeout_id != 0) {
                g_source_remove (inactivity_timeout_id);
                inactivity_timeout_id = 0;
        }
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
        GError *error = NULL;

        manager = gclue_service_manager_new (connection, &error);
        if (manager == NULL) {
                g_critical ("Failed to register server: %s", error->message);
                g_error_free (error);

                exit (-2);
        }

        g_signal_connect (manager,
                          "notify::active",
                          G_CALLBACK (on_active_notify),
                          NULL);

        if (inactivity_timeout > 0)
                inactivity_timeout_id =
                        g_timeout_add_seconds (inactivity_timeout,
                                               on_inactivity_timeout,
                                               NULL);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
        g_critical ("Failed to acquire name '%s' on system bus or lost it.", name);

        exit (-3);
}

int
main (int argc, char **argv)
{
        guint owner_id;
        GError *error = NULL;
        GOptionContext *context;
        GClueConfig *config;

        setlocale (LC_ALL, "");

        textdomain (GETTEXT_PACKAGE);
        bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        g_set_application_name (_("GeoClue"));

        context = g_option_context_new ("- Geoclue D-Bus service");
        g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_critical ("option parsing failed: %s\n", error->message);
                exit (-1);
        }
        g_option_context_free (context);

        if (version) {
                g_print ("%s\n", PACKAGE_VERSION);
                exit (0);
        }

        config = gclue_config_get_singleton ();
        if (submit_data)
                gclue_config_set_wifi_submit_data (config, submit_data);
        if (submit_nick != NULL)
                gclue_config_set_wifi_submit_nick (config, submit_nick);

        owner_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
                                   BUS_NAME,
                                   G_BUS_NAME_OWNER_FLAGS_NONE,
                                   on_bus_acquired,
                                   NULL,
                                   on_name_lost,
                                   NULL,
                                   NULL);

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);

        if (manager != NULL)
                g_object_unref (manager);
        g_bus_unown_name (owner_id);
        g_main_loop_unref (main_loop);

        return 0;
}
