/*
An indicator to show information that is in messaging applications
that the user is using.

Copyright 2012 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>
    Lars Uebernickel <lars.uebernickel@canonical.com>

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

#include <config.h>
#include <locale.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib-unix.h>

#include "dbus-data.h"
#include "gsettingsstrv.h"
#include "indicator-messages-service.h"
#include "indicator-messages-application.h"
#include "im-phone-menu.h"
#include "im-desktop-menu.h"
#include "im-application-list.h"

#define NUM_STATUSES 5

static ImApplicationList *applications;

static IndicatorMessagesService *messages_service;
static GHashTable *menus;
static GSettings *settings;

enum {
	DBUS_ERROR_BAD_DESKTOP_FILE,
};

G_DEFINE_QUARK(indicator_messages_dbus_error, dbus_error);

static gboolean
register_application (IndicatorMessagesService *service,
		      GDBusMethodInvocation *invocation,
		      const gchar *desktop_id,
		      const gchar *menu_path,
		      gpointer user_data)
{
	GDBusConnection *bus;
	const gchar *sender;

	if (!im_application_list_add (applications, desktop_id)) {
		g_dbus_method_invocation_return_error(invocation, dbus_error_quark(), DBUS_ERROR_BAD_DESKTOP_FILE, "Unable to find or parse desktop file for application '%s'", desktop_id);
		return TRUE;
	}

	bus = g_dbus_interface_skeleton_get_connection (G_DBUS_INTERFACE_SKELETON (service));
	sender = g_dbus_method_invocation_get_sender (invocation);

	im_application_list_set_remote (applications, desktop_id, bus, sender, menu_path);
	g_settings_strv_append_unique (settings, "applications", desktop_id);

	indicator_messages_service_complete_register_application (service, invocation);

	return TRUE;
}

static gboolean
unregister_application (IndicatorMessagesService *service,
			GDBusMethodInvocation *invocation,
			const gchar *desktop_id,
			gpointer user_data)
{
	im_application_list_remove (applications, desktop_id);
	g_settings_strv_remove (settings, "applications", desktop_id);

	indicator_messages_service_complete_unregister_application (service, invocation);

	return TRUE;
}

static gboolean
set_status (IndicatorMessagesService *service,
	    GDBusMethodInvocation *invocation,
	    const gchar *desktop_id,
	    const gchar *status_str,
	    gpointer user_data)
{
	GDesktopAppInfo *appinfo;
	const gchar *id;

	g_return_val_if_fail (g_str_equal (status_str, "available") ||
			      g_str_equal (status_str, "away")||
			      g_str_equal (status_str, "busy") ||
			      g_str_equal (status_str, "invisible") ||
			      g_str_equal (status_str, "offline"),
			      FALSE);

	appinfo = g_desktop_app_info_new (desktop_id);
	if (!appinfo) {
		g_warning ("could not set status for '%s', there's no desktop file with that id", desktop_id);
		return TRUE;
	}

	id = g_app_info_get_id (G_APP_INFO (appinfo));

	im_application_list_set_status(applications, id, status_str);

	indicator_messages_service_complete_set_status (service, invocation);

	g_object_unref (appinfo);

	return TRUE;
}

static gboolean
app_stopped (IndicatorMessagesService *service,
	     GDBusMethodInvocation *invocation,
	     const gchar *desktop_id,
	     gpointer user_data)
{
	GDesktopAppInfo *appinfo;
	const gchar *id;

	appinfo = g_desktop_app_info_new (desktop_id);
	if (!appinfo)
		return TRUE;

	id = g_app_info_get_id (G_APP_INFO (appinfo));
	im_application_list_set_remote (applications, id, NULL, NULL, NULL);
	indicator_messages_service_complete_application_stopped_running (service, invocation);

	g_object_unref (appinfo);

	return TRUE;
}

/* The status has been set by the user, let's tell the world! */
static void
status_set_by_user (ImApplicationList * list, const gchar * status, gpointer user_data)
{
	indicator_messages_service_emit_status_changed(messages_service, status);
	return;
}

static void
on_bus_acquired (GDBusConnection *bus,
		 const gchar     *name,
		 gpointer         user_data)
{
	GError *error = NULL;
	GHashTableIter it;
	const gchar *profile;
	ImMenu *menu;

	/* Register some errors */
	g_dbus_error_register_error (dbus_error_quark(), DBUS_ERROR_BAD_DESKTOP_FILE, "BadDesktopFile");

	g_dbus_connection_export_action_group (bus, INDICATOR_MESSAGES_DBUS_OBJECT,
					       im_application_list_get_action_group (applications),
					       &error);
	if (error) {
		g_warning ("unable to export action group on dbus: %s", error->message);
		g_error_free (error);
		return;
	}

	g_hash_table_iter_init (&it, menus);
	while (g_hash_table_iter_next (&it, (gpointer *) &profile, (gpointer *) &menu)) {
		gchar *object_path;

		object_path = g_strconcat (INDICATOR_MESSAGES_DBUS_OBJECT, "/", profile, NULL);
		if (!im_menu_export (menu, bus, object_path, &error)) {
			g_warning ("unable to export menu for profile '%s': %s", profile, error->message);
			g_clear_error (&error);
		}

		g_free (object_path);
	}

	g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (messages_service),
					  bus, INDICATOR_MESSAGES_DBUS_SERVICE_OBJECT,
					  &error);
	if (error) {
		g_warning ("unable to export messages service on dbus: %s", error->message);
		g_error_free (error);
		return;
	}
}

static void
on_name_lost (GDBusConnection *bus,
	      const gchar     *name,
	      gpointer         user_data)
{
	GMainLoop *mainloop = user_data;

	g_main_loop_quit (mainloop);
}

static gboolean
sig_term_handler (gpointer user_data)
{
	GMainLoop *mainloop = user_data;

	g_main_loop_quit (mainloop);

	return FALSE;
}

int
main (int argc, char ** argv)
{
	GMainLoop * mainloop = NULL;
	GBusNameOwnerFlags flags;

	/* Glib init */
#if G_ENCODE_VERSION(GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION) <= GLIB_VERSION_2_34
	g_type_init();
#endif

	mainloop = g_main_loop_new (NULL, FALSE);

	/* Setting up i18n and gettext.  Apparently, we need
	   all of these. */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	textdomain (GETTEXT_PACKAGE);

	/* Bring up the service DBus interface */
	messages_service = indicator_messages_service_skeleton_new ();

	flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
	if (argc >= 2 && g_str_equal (argv[1], "--replace"))
		flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

	g_bus_own_name (G_BUS_TYPE_SESSION, "com.canonical.indicator.messages", flags,
			on_bus_acquired, NULL, on_name_lost, mainloop, NULL);

	g_signal_connect (messages_service, "handle-register-application",
			  G_CALLBACK (register_application), NULL);
	g_signal_connect (messages_service, "handle-unregister-application",
			  G_CALLBACK (unregister_application), NULL);
	g_signal_connect (messages_service, "handle-set-status",
			  G_CALLBACK (set_status), NULL);
	g_signal_connect (messages_service, "handle-application-stopped-running",
			  G_CALLBACK (app_stopped), NULL);

	applications = im_application_list_new ();
	g_signal_connect (applications, "status-set",
			  G_CALLBACK (status_set_by_user), NULL);

	settings = g_settings_new ("com.canonical.indicator.messages");
	{
		gchar **app_ids;
		gchar **id;

		app_ids = g_settings_get_strv (settings, "applications");
		for (id = app_ids; *id; id++)
			im_application_list_add (applications, *id);

		g_strfreev (app_ids);
	}

	menus = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
	g_hash_table_insert (menus, "phone", im_phone_menu_new (applications));
	g_hash_table_insert (menus, "desktop", im_desktop_menu_new (applications));

	g_unix_signal_add(SIGTERM, sig_term_handler, mainloop);

	g_main_loop_run(mainloop);

	/* Clean up */
	g_hash_table_unref (menus);
	g_object_unref (messages_service);
	g_object_unref (settings);
	g_object_unref (applications);
	return 0;
}
