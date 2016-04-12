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
 * Copyright 2004 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <glib-unix.h>

#include "gsystem-local-alloc.h"
#include "nm-connection-list.h"
#include "nm-connection-editor.h"
#include "nm-dbus-compat.h"

gboolean nm_ce_keep_above;

static GMainLoop *loop = NULL;

#define ARG_TYPE      "type"
#define ARG_CREATE    "create"
#define ARG_SHOW      "show"
#define ARG_UUID      "uuid"

#define NM_CE_DBUS_SERVICE   "org.gnome.nm_connection_editor"
#define NM_CE_DBUS_INTERFACE "org.gnome.nm_connection_editor"

static GDBusNodeInfo *introspection_data = NULL;

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
	ctype = nm_setting_lookup_type (type);
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

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
	NMConnectionList *list = NM_CONNECTION_LIST (user_data);
	char *type = NULL, *uuid = NULL;
	gboolean create = FALSE, show = FALSE;

	if (g_strcmp0 (method_name, "Start") == 0) {
		if (g_variant_is_of_type (parameters, (const GVariantType *) "(a{sv})")) {
			gs_unref_variant GVariant *dict = NULL;

			g_variant_get (parameters, "(@a{sv})", &dict);
			g_variant_lookup (dict, ARG_TYPE, "s", &type);
			g_variant_lookup (dict, ARG_UUID, "s", &uuid);
			g_variant_lookup (dict, ARG_CREATE, "b", &create);
			g_variant_lookup (dict, ARG_SHOW, "b", &show);
			if (handle_arguments (list, type, create, show, uuid, FALSE))
				nm_connection_list_present (list);

			g_dbus_method_invocation_return_value (invocation, NULL);
		} else {
			g_dbus_method_invocation_return_error (invocation,
			                                       G_DBUS_ERROR,
			                                       G_DBUS_ERROR_INVALID_ARGS,
			                                       "Invalid argument type (not a dict)");
		}
	}
}

static const GDBusInterfaceVTable interface_vtable = {
	handle_method_call, NULL, NULL
};

static guint
start_service (GDBusConnection *bus,
               NMConnectionList *list,
               guint *out_registration_id)
{
	static const gchar introspection_xml[] =
		"<node>"
		"  <interface name='org.gnome.nm_connection_editor'>"
		"    <method name='Start'>"
		"      <arg type='a{sv}' name='args' direction='in'/>"
		"    </method>"
		"  </interface>"
		"</node>";

	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
	g_assert (introspection_data != NULL);

	*out_registration_id = g_dbus_connection_register_object (bus,
	                                                          "/",
	                                                          introspection_data->interfaces[0],
	                                                          &interface_vtable,
	                                                          list,  /* user_data */
	                                                          NULL,  /* user_data_free_func */
	                                                          NULL); /* GError** */

	return g_bus_own_name_on_connection (bus,
	                                     NM_CE_DBUS_SERVICE,
	                                     G_BUS_NAME_OWNER_FLAGS_NONE,
	                                     NULL,
	                                     NULL,
	                                     NULL,
	                                     NULL);
}

static gboolean
try_existing_instance (GDBusConnection *bus,
                       const char *type,
                       gboolean create,
                       gboolean show,
                       const char *uuid)
{
	gs_free char *owner = NULL;
	gs_free_error GError *error = NULL;
	gs_unref_variant GVariant *reply = NULL;
	GVariantBuilder builder;

	g_assert (bus);

	reply = g_dbus_connection_call_sync (bus,
	                                     DBUS_SERVICE_DBUS,
	                                     DBUS_PATH_DBUS,
	                                     DBUS_INTERFACE_DBUS,
	                                     "GetNameOwner",
	                                     g_variant_new ("(s)", NM_CE_DBUS_SERVICE),
	                                     G_VARIANT_TYPE ("(s)"),
	                                     G_DBUS_CALL_FLAGS_NONE,
	                                     -1,           /* timeout */
	                                     NULL,
	                                     &error);
	if (!reply) {
		if (!g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER))
			g_warning ("Failed to get editor name owner: %s", error->message);
		return FALSE;
	}

	g_variant_get (reply, "(s)", &owner);
	if (!owner)
		return FALSE;

	g_variant_unref (reply);

	g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
	if (type)
		g_variant_builder_add (&builder, "{sv}", ARG_TYPE, g_variant_new_string (type));
	if (create)
		g_variant_builder_add (&builder, "{sv}", ARG_CREATE, g_variant_new_boolean (TRUE));
	if (show)
		g_variant_builder_add (&builder, "{sv}", ARG_SHOW, g_variant_new_boolean (TRUE));
	if (uuid)
		g_variant_builder_add (&builder, "{sv}", ARG_UUID, g_variant_new_string (uuid));

	reply = g_dbus_connection_call_sync (bus,
	                                     NM_CE_DBUS_SERVICE,
	                                     "/",
	                                     NM_CE_DBUS_INTERFACE,
	                                     "Start",
	                                     g_variant_new ("(@a{sv})", g_variant_builder_end (&builder)),
	                                     NULL,
	                                     G_DBUS_CALL_FLAGS_NONE,
	                                     -1,           /* timeout */
	                                     NULL,
	                                     &error);
	if (!reply) {
		g_warning ("Failed to send arguments to existing editor instance: %s", error->message);
		return FALSE;
	}

	return TRUE;
}

static gboolean
signal_handler (gpointer user_data)
{
	int signo = GPOINTER_TO_INT (user_data);

	g_message ("Caught signal %d, shutting down...", signo);
	g_main_loop_quit (loop);

	return G_SOURCE_REMOVE;
}

int
main (int argc, char *argv[])
{
	GOptionContext *opt_ctx = NULL;
	GError *error = NULL;
	NMConnectionList *list = NULL;
	guint owner_id = 0, registration_id = 0;
	GDBusConnection *bus = NULL;
	gs_free char *type = NULL, *uuid = NULL;
	gboolean create = FALSE, show = FALSE;
	int ret = 1;

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
	if (!g_option_context_parse (opt_ctx, &argc, &argv, &error)) {
		g_warning ("Failed to parse options: %s", error->message);
		goto out;
	}

	/* Just one page for both CDMA & GSM, handle that here */
	if (g_strcmp0 (type, NM_SETTING_CDMA_SETTING_NAME) == 0) {
		g_free (type);
		type = g_strdup (NM_SETTING_GSM_SETTING_NAME);
	}

	bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
	if (bus) {
		/* Check for an existing instance on the bus, and if there
		 * is one, send the arguments to it and exit instead of opening
		 * a second instance of the connection editor.
		 */
		if (try_existing_instance (bus, type, create, show, uuid)) {
			/* success */
			ret = 0;
			goto out;
		}
	}

	loop = g_main_loop_new (NULL, FALSE);

	list = nm_connection_list_new ();
	if (!list) {
		g_warning ("Failed to initialize the UI, exiting...");
		goto out;
	}
	g_signal_connect_swapped (list, "done", G_CALLBACK (g_main_loop_quit), loop);

	owner_id = start_service (bus, list, &registration_id);

	/* Figure out what page or editor window we'll show initially */
	if (handle_arguments (list, type, create, show, uuid, (create || show || uuid)))
		nm_connection_list_present (list);

	g_unix_signal_add (SIGTERM, signal_handler, GINT_TO_POINTER (SIGTERM));
	g_unix_signal_add (SIGINT, signal_handler, GINT_TO_POINTER (SIGINT));

	g_main_loop_run (loop);
	ret = 0;

out:
	if (owner_id)
		g_bus_unown_name (owner_id);
	if (registration_id)
		g_dbus_connection_unregister_object (bus, registration_id);
	if (introspection_data)
		g_dbus_node_info_unref (introspection_data);
	g_clear_error (&error);
	if (opt_ctx)
		g_option_context_free (opt_ctx);
	g_clear_object (&list);
	g_clear_object (&bus);
	return ret;
}

