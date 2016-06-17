/*
The core file for the service that starts up all the objects we need
and houses our main loop.

Copyright 2009 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <gio/gio.h>
#include "application-service-appstore.h"
#include "application-service-watcher.h"
#include "dbus-shared.h"

/* The base main loop */
static GMainLoop * mainloop = NULL;
/* Where the application registry lives */
static ApplicationServiceAppstore * appstore = NULL;
/* Interface for applications */
static ApplicationServiceWatcher * watcher = NULL;

/* Make sure we can set up all our objects before we get the name */
static void
bus_acquired (GDBusConnection * con, const gchar * name, gpointer user_data)
{
	g_debug("Bus Acquired, building objects");

	/* Building our app store */
	appstore = application_service_appstore_new();

	/* Adding a watcher for the Apps coming up */
	watcher = application_service_watcher_new(appstore);
}

/* Nice to know, but we're not doing anything special */
static void
name_acquired (GDBusConnection * con, const gchar * name, gpointer user_data)
{
	g_debug("Name Acquired");
}

/* Shouldn't happen under normal usage */
static void
name_lost (GDBusConnection * con, const gchar * name, gpointer user_data)
{
	g_warning("Name Lost");
	g_main_loop_quit(mainloop);
}

/* Builds up the core objects and puts us spinning into
   a main loop. */
int
main (int argc, char ** argv)
{
	guint nameownership = g_bus_own_name(G_BUS_TYPE_SESSION,
		INDICATOR_APPLICATION_DBUS_ADDR,
		G_BUS_NAME_OWNER_FLAGS_NONE,
		bus_acquired,
		name_acquired,
		name_lost,
		NULL, NULL);

	/* Building and executing our main loop */
	mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(mainloop);

	g_debug("Finishing Main Loop");

	g_bus_unown_name(nameownership);

	/* Unref'ing all the objects */
	g_main_loop_unref(mainloop);
	g_object_unref(G_OBJECT(watcher));
	g_object_unref(G_OBJECT(appstore));

	return 0;
}
