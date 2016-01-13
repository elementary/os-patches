/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Rodrigo Moya <rodrigo@gnome-db.org>
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
 * Copyright (C) 2004 - 2013 Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <dbus/dbus.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <glib.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <nm-setting-wired.h>
#include <nm-setting-gsm.h>
#include <nm-setting-cdma.h>
#include "nm-connection-list.h"
#include "nm-connection-editor.h"

gboolean nm_ce_keep_above;

static GMainLoop *loop = NULL;

#define ARG_TYPE      "type"
#define ARG_CREATE    "create"
#define ARG_SHOW      "show"
#define ARG_UUID      "uuid"

#define DBUS_TYPE_G_MAP_OF_VARIANT    (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

#define NM_CE_DBUS_SERVICE_NAME       "org.gnome.nm_connection_editor"

/*************************************************/

#define NM_TYPE_CE_SERVICE            (nm_ce_service_get_type ())
#define NM_CE_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NM_TYPE_CE_SERVICE, NMCEService))
#define NM_CE_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NM_TYPE_CE_SERVICE, NMCEServiceClass))
#define NM_IS_CE_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NM_TYPE_CE_SERVICE))
#define NM_IS_CE_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NM_TYPE_CE_SERVICE))
#define NM_CE_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NM_TYPE_CE_SERVICE, NMCEServiceClass))

typedef struct {
	GObject parent;
	NMConnectionList *list;
} NMCEService;

typedef struct {
	GObjectClass parent;
} NMCEServiceClass;

GType nm_ce_service_get_type (void);

G_DEFINE_TYPE (NMCEService, nm_ce_service, G_TYPE_OBJECT)

static gboolean impl_start (NMCEService *self, GHashTable *args, GError **error);

#include "nm-connection-editor-service-glue.h"

static NMCEService *
nm_ce_service_new (DBusGConnection *bus, DBusGProxy *proxy, NMConnectionList *list)
{
	GObject *object;
	DBusConnection *connection;
	GError *err = NULL;
	guint32 result;

	g_return_val_if_fail (bus != NULL, NULL);
	g_return_val_if_fail (proxy != NULL, NULL);

	object = g_object_new (NM_TYPE_CE_SERVICE, NULL);
	if (!object)
		return NULL;

	NM_CE_SERVICE (object)->list = list;

	dbus_connection_set_change_sigpipe (TRUE);
	connection = dbus_g_connection_get_connection (bus);
	dbus_connection_set_exit_on_disconnect (connection, FALSE);

	/* Register our single-instance service.  Don't care if it fails. */
	if (!dbus_g_proxy_call (proxy, "RequestName", &err,
	                        G_TYPE_STRING, NM_CE_DBUS_SERVICE_NAME,
	                        G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
	                        G_TYPE_INVALID,
	                        G_TYPE_UINT, &result,
	                        G_TYPE_INVALID)) {
		g_warning ("Could not acquire the connection editor service.\n"
		           "  Message: '%s'", err->message);
		g_error_free (err);
		return (NMCEService *) object;
	}

	if (result == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		/* success; grab the bus name */
		dbus_g_connection_register_g_object (bus, "/", object);
	}

	return (NMCEService *) object;
}

static void
nm_ce_service_init (NMCEService *self)
{
}

static void
nm_ce_service_class_init (NMCEServiceClass *service_class)
{
	dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (service_class),
	                                 &dbus_glib_nm_connection_editor_service_object_info);
}

/*************************************************/

static gboolean
idle_create_connection (gpointer user_data)
{
	NMConnectionList *list = user_data;
	GType ctype = (GType) GPOINTER_TO_SIZE (g_object_get_data (G_OBJECT (list), "nm-connection-editor-ctype"));
	char *detail = g_object_get_data (G_OBJECT (list), "nm-connection-editor-detail");

	nm_connection_list_create (list, ctype, detail);
	return FALSE;
}

static gboolean
handle_arguments (NMConnectionList *list,
                  const char *type,
                  gboolean create,
                  gboolean show,
                  const char *edit_uuid,
                  gboolean quit_after)
{
	gboolean show_list = TRUE;
	GType ctype;
	char *type_tmp = NULL;
	const char *p, *detail = NULL;

	if (type) {
		p = strchr (type, ':');
		if (p) {
			type = type_tmp = g_strndup (type, p - type);
			detail = p + 1;
		}
	} else
		type = NM_SETTING_WIRED_SETTING_NAME;

	/* Grab type to create or show */
	ctype = nm_connection_lookup_setting_type (type);
	if (ctype == 0) {
		g_warning ("Unknown connection type '%s'", type);
		g_free (type_tmp);
		return TRUE;
	}

	if (show) {
		/* Just show the given connection type page */
		nm_connection_list_set_type (list, ctype);
	} else if (create) {
		if (!type) {
			g_warning ("'create' requested but no connection type given.");
			g_free (type_tmp);
			return TRUE;
		}

		/* If type is "vpn" and the user cancels the "vpn type" dialog, we need
		 * to quit. But we haven't even started yet. So postpone this to an idle.
		 */
		g_idle_add (idle_create_connection, list);
		g_object_set_data (G_OBJECT (list), "nm-connection-editor-ctype",
		                   GSIZE_TO_POINTER (ctype));
		g_object_set_data_full (G_OBJECT (list), "nm-connection-editor-detail",
		                        g_strdup (detail), g_free);

		show_list = FALSE;
	} else if (edit_uuid) {
		/* Show the edit dialog for the given UUID */
		nm_connection_list_edit (list, edit_uuid);
		show_list = FALSE;
	}

	/* If only editing a single connection, exit when done with that connection */
	if (show_list == FALSE && quit_after == TRUE)
		g_signal_connect_swapped (list, "editing-done", G_CALLBACK (g_main_loop_quit), loop);

	g_free (type_tmp);
	return show_list;
}

static gboolean
impl_start (NMCEService *self, GHashTable *table, GError **error)
{
	GValue *value;
	const char *type = NULL;
	const char *uuid = NULL;
	gboolean create = FALSE;
	gboolean show = FALSE;
	gboolean show_list;

	value = g_hash_table_lookup (table, ARG_TYPE);
	if (value && G_VALUE_HOLDS_STRING (value)) {
		type = g_value_get_string (value);
		g_assert (type);
	}

	value = g_hash_table_lookup (table, ARG_UUID);
	if (value && G_VALUE_HOLDS_STRING (value)) {
		uuid = g_value_get_string (value);
		g_assert (uuid);
	}

	value = g_hash_table_lookup (table, ARG_CREATE);
	if (value && G_VALUE_HOLDS_BOOLEAN (value))
		create = g_value_get_boolean (value);

	value = g_hash_table_lookup (table, ARG_SHOW);
	if (value && G_VALUE_HOLDS_BOOLEAN (value))
		show = g_value_get_boolean (value);

	show_list = handle_arguments (self->list, type, create, show, uuid, FALSE);
	if (show_list)
		nm_connection_list_present (self->list);

	return TRUE;
}

static gboolean
try_existing_instance (DBusGConnection *bus,
                       DBusGProxy *proxy,
                       const char *type,
                       gboolean create,
                       gboolean show,
                       const char *uuid)
{
	gboolean has_owner = FALSE;
	DBusGProxy *instance;
	GHashTable *args;
	GValue type_value = { 0, };
	GValue create_value = { 0, };
	GValue show_value = { 0, };
	GValue uuid_value = { 0, };
	gboolean success = FALSE;
	GError *error = NULL;

	if (!dbus_g_proxy_call (proxy, "NameHasOwner", NULL,
	                        G_TYPE_STRING, NM_CE_DBUS_SERVICE_NAME, G_TYPE_INVALID,
	                        G_TYPE_BOOLEAN, &has_owner, G_TYPE_INVALID))
		return FALSE;

	if (!has_owner)
		return FALSE;

	/* Send arguments to existing process */
	instance = dbus_g_proxy_new_for_name (bus,
	                                      NM_CE_DBUS_SERVICE_NAME,
	                                      "/",
	                                      NM_CE_DBUS_SERVICE_NAME);
	if (!instance)
		return FALSE;

	args = g_hash_table_new (g_str_hash, g_str_equal);
	if (type) {
		g_value_init (&type_value, G_TYPE_STRING);
		g_value_set_static_string (&type_value, type);
		g_hash_table_insert (args, ARG_TYPE, &type_value);
	}
	if (create) {
		g_value_init (&create_value, G_TYPE_BOOLEAN);
		g_value_set_boolean (&create_value, TRUE);
		g_hash_table_insert (args, ARG_CREATE, &create_value);
	}
	if (show) {
		g_value_init (&show_value, G_TYPE_BOOLEAN);
		g_value_set_boolean (&show_value, TRUE);
		g_hash_table_insert (args, ARG_SHOW, &show_value);
	}
	if (uuid) {
		g_value_init (&uuid_value, G_TYPE_STRING);
		g_value_set_static_string (&uuid_value, uuid);
		g_hash_table_insert (args, ARG_UUID, &uuid_value);
	}

	if (dbus_g_proxy_call (instance, "Start", &error,
	                       DBUS_TYPE_G_MAP_OF_VARIANT, args, G_TYPE_INVALID,
	                       G_TYPE_INVALID))
		success = TRUE;
	else
		g_warning ("%s: error calling start: %s", __func__, error->message);

	g_hash_table_destroy (args);
	g_object_unref (instance);
	return success;
}

static void
signal_handler (int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
		g_main_loop_quit (loop);
}

static void
setup_signals (void)
{
	struct sigaction action;
	sigset_t mask;

	sigemptyset (&mask);
	action.sa_handler = signal_handler;
	action.sa_mask = mask;
	action.sa_flags = 0;
	sigaction (SIGTERM,  &action, NULL);
	sigaction (SIGINT,  &action, NULL);
}

int
main (int argc, char *argv[])
{
	GOptionContext *opt_ctx;
	GError *error = NULL;
	NMConnectionList *list;
	DBusGConnection *bus;
	char *type = NULL;
	gboolean create = FALSE;
	gboolean show = FALSE;
	gboolean success;
	char *uuid = NULL;
	NMCEService *service = NULL;
	DBusGProxy *proxy = NULL;
	gboolean show_list;

	GOptionEntry entries[] = {
		{ ARG_TYPE,   't', 0, G_OPTION_ARG_STRING, &type,   "Type of connection to show or create", NM_SETTING_WIRED_SETTING_NAME },
		{ ARG_CREATE, 'c', 0, G_OPTION_ARG_NONE,   &create, "Create a new connection", NULL },
		{ ARG_SHOW,   's', 0, G_OPTION_ARG_NONE,   &show,   "Show a given connection type page", NULL },
		{ "edit",     'e', 0, G_OPTION_ARG_STRING, &uuid,   "Edit an existing connection with a given UUID", "UUID" },

		/* This is not passed over D-Bus. */
		{ "keep-above", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &nm_ce_keep_above, NULL, NULL },
		{ NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, NMALOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	gtk_init (&argc, &argv);
	textdomain (GETTEXT_PACKAGE);

	opt_ctx = g_option_context_new (NULL);
	g_option_context_set_summary (opt_ctx, "Allows users to view and edit network connection settings");
	g_option_context_add_main_entries (opt_ctx, entries, NULL);
	success = g_option_context_parse (opt_ctx, &argc, &argv, &error);
	g_option_context_free (opt_ctx);

	if (!success) {
		g_warning ("%s\n", error->message);
		g_error_free (error);
		return 1;
	}

	/* Just one page for both CDMA & GSM, handle that here */
	if (type && g_strcmp0 (type, NM_SETTING_CDMA_SETTING_NAME) == 0)
		type = (char *) NM_SETTING_GSM_SETTING_NAME;

	/* Inits the dbus-glib type system too */
	bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
	if (bus) {
		proxy = dbus_g_proxy_new_for_name (bus,
		                                   "org.freedesktop.DBus",
		                                   "/org/freedesktop/DBus",
		                                   "org.freedesktop.DBus");
		g_assert (proxy);

		/* Check for an existing instance on the bus, and if there
		 * is one, send the arguments to it and exit instead of opening
		 * a second instance of the connection editor.
		 */
		if (try_existing_instance (bus, proxy, type, create, show, uuid))
			return 0;
	}

	loop = g_main_loop_new (NULL, FALSE);

	list = nm_connection_list_new ();
	if (!list) {
		g_warning ("Failed to initialize the UI, exiting...");
		return 1;
	}
	g_signal_connect_swapped (list, "done", G_CALLBACK (g_main_loop_quit), loop);

	/* Create our single-instance-app service if we can */
	if (proxy)
		service = nm_ce_service_new (bus, proxy, list);

	/* Show the dialog */
	g_signal_connect_swapped (list, "done", G_CALLBACK (g_main_loop_quit), loop);

	/* Figure out what page or editor window we'll show initially */
	show_list = handle_arguments (list, type, create, show, uuid, (create || show || uuid));
	if (show_list)
		nm_connection_list_present (list);

	setup_signals ();
	g_main_loop_run (loop);

	/* Cleanup */
	g_object_unref (list);
	if (service)
		g_object_unref (service);
	if (proxy)
		g_object_unref (proxy);
	if (bus)
		dbus_g_connection_unref (bus);
	return 0;
}

