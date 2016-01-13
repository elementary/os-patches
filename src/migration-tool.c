/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager applet migration tool -- migrate old GConf settings
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2005-2012 Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libintl.h>
#include <stdlib.h>

#include <nm-remote-connection.h>
#include <nm-remote-settings.h>

#include "gconf-helpers.h"

gboolean success = TRUE;

static void
add_cb (NMRemoteSettings *settings,
        NMRemoteConnection *connection,
        GError *error,
        gpointer user_data)
{
	NMConnection *c = user_data;

	if (error) {
		g_printerr ("Failed to move connection '%s' to NetworkManager system settings: %s",
					nm_connection_get_id (c),
					error->message);
		success = FALSE;
	}
	g_object_unref (c);
}

static void
import_cb (NMConnection *connection, gpointer user_data)
{
	NMRemoteSettings *settings = user_data;

	if (!nm_remote_settings_add_connection (settings, connection, add_cb, g_object_ref (connection))) {
		g_warning ("Failed to move connection '%s' to NetworkManager system settings.",
		           nm_connection_get_id (connection));
		g_object_unref (connection);
		success = FALSE;
	}
}

int
main (int argc, char **argv)
{
	DBusGConnection *bus;
	NMRemoteSettings *settings;
	GError *error = NULL;

	bindtextdomain (GETTEXT_PACKAGE, NMALOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();

	if (argc != 1) {
		g_printerr ("Usage: %s\n", argv[0]);
		exit (1);
	}

	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!bus) {
		g_printerr ("Could not get system bus: %s\n", error->message);
		g_error_free (error);
		exit (1);
	}

	settings = nm_remote_settings_new (bus);
	nm_gconf_move_connections_to_system (import_cb, settings);

	g_object_unref (settings);
	dbus_g_connection_unref (bus);

	return success ? 0 : 1;
}

