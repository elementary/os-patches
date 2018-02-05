/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

/*
 *  Copyright (C) 2004-2008 Red Hat, Inc.
 *  Copyright (C) 2013 Intel Corporation.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Bastien Nocera <hadess@hadess.net>
 *  Authors: Emilio Pozuelo Monfort <emilio.pozuelo@collabora.co.uk>
 *
 */

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <bluetooth-client.h>
#include <libnotify/notify.h>
#include <canberra-gtk.h>

#include "bluetooth-settings-obexpush.h"

#define MANAGER_SERVICE	"org.bluez.obex"
#define MANAGER_IFACE	"org.bluez.obex.AgentManager1"
#define MANAGER_PATH	"/org/bluez/obex"

#define AGENT_PATH	"/org/gnome/share/agent"
#define AGENT_IFACE	"org.bluez.obex.Agent1"

#define TRANSFER_IFACE	"org.bluez.obex.Transfer1"
#define SESSION_IFACE	"org.bluez.obex.Session1"

static GDBusNodeInfo *introspection_data = NULL;

static const gchar introspection_xml[] =
"<node name='"AGENT_PATH"'>"
"  <interface name='"AGENT_IFACE"'>"
"    <method name='Release'>"
"    </method>"
"    <method name='Cancel'>"
"    </method>"
"    <method name='AuthorizePush'>"
"      <arg name='transfer' type='o' />"
"      <arg name='path' type='s' direction='out' />"
"    </method>"
"  </interface>"
"</node>";

G_DEFINE_TYPE(ObexAgent, obex_agent, G_TYPE_OBJECT)

static ObexAgent *agent;
static BluetoothClient *client;

static void
on_close_notification (NotifyNotification *notification)
{
	g_object_unref (notification);
}

static void
notification_launch_action_on_file_cb (NotifyNotification *notification,
				       const char *action,
				       const char *file_uri)
{
	g_assert (action != NULL);

	/* We launch the file viewer for the file */
	if (g_str_equal (action, "display") != FALSE) {
		GdkDisplay *display;
		GAppLaunchContext *ctx;
		GTimeVal val;

		g_get_current_time (&val);

		display = gdk_display_get_default ();
		ctx = G_APP_LAUNCH_CONTEXT (gdk_display_get_app_launch_context (display));
		gdk_app_launch_context_set_timestamp (GDK_APP_LAUNCH_CONTEXT (ctx), val.tv_sec);

		if (g_app_info_launch_default_for_uri (file_uri, ctx, NULL) == FALSE) {
			g_warning ("Failed to launch the file viewer\n");
		}

		g_object_unref (ctx);
	}

	/* we open the Downloads folder */
	if (g_str_equal (action, "reveal") != FALSE) {
		GDBusConnection *connection = agent->connection;
		GVariantBuilder builder;

		g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
		g_variant_builder_add (&builder, "s", file_uri);

		g_dbus_connection_call (connection,
					"org.freedesktop.FileManager1",
					"/org/freedesktop/FileManager1",
					"org.freedesktop.FileManager1",
					"ShowItems",
					g_variant_new ("(ass)", &builder, ""),
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					NULL,
					NULL);

		g_variant_builder_clear (&builder);
	}

	notify_notification_close (notification, NULL);
}

static void
show_notification (const char *filename)
{
	char *file_uri, *notification_text, *display, *mime_type;
	NotifyNotification *notification;
	ca_context *ctx;
	GAppInfo *app;

	file_uri = g_filename_to_uri (filename, NULL, NULL);
	if (file_uri == NULL) {
		g_warning ("Could not make a filename from '%s'", filename);
		return;
	}

	display = g_filename_display_basename (filename);
	/* Translators: %s is the name of the filename received */
	notification_text = g_strdup_printf(_("You received “%s” via Bluetooth"), display);
	g_free (display);
	notification = notify_notification_new (_("You received a file"),
						notification_text,
						"bluetooth");

	notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
	notify_notification_set_hint_string (notification, "desktop-entry", "gnome-bluetooth-panel");

	mime_type = g_content_type_guess (filename, NULL, 0, NULL);
	app = g_app_info_get_default_for_type (mime_type, FALSE);
	if (app != NULL) {
		g_object_unref (app);
		notify_notification_add_action (notification, "display", _("Open File"),
						(NotifyActionCallback) notification_launch_action_on_file_cb,
						g_strdup (file_uri), (GFreeFunc) g_free);
	}
	notify_notification_add_action (notification, "reveal", _("Reveal File"),
					(NotifyActionCallback) notification_launch_action_on_file_cb,
					g_strdup (file_uri), (GFreeFunc) g_free);

	g_free (file_uri);
	
	g_signal_connect (G_OBJECT (notification), "closed", G_CALLBACK (on_close_notification), notification);

	if (!notify_notification_show (notification, NULL)) {
		g_warning ("failed to send notification\n");
	}
	g_free (notification_text);

	/* Now we do the audio notification */
	ctx = ca_gtk_context_get ();
	ca_context_play (ctx, 0,
			 CA_PROP_EVENT_ID, "complete-download",
			 CA_PROP_EVENT_DESCRIPTION, _("File reception complete"),
			 NULL);
}

static void
reject_transfer (GDBusMethodInvocation *invocation)
{
	const char *filename;

	filename = g_object_get_data (G_OBJECT (invocation), "temp-filename");
	g_remove (filename);

	g_dbus_method_invocation_return_dbus_error (invocation,
		"org.bluez.obex.Error.Rejected", "Not Authorized");
}

static void
ask_user_transfer_accepted (NotifyNotification *notification,
			    char *action,
			    GDBusMethodInvocation *invocation)
{
	gchar *file = g_object_get_data (G_OBJECT (invocation), "temp-filename");

	g_debug ("Notification: transfer accepted! accepting transfer");

	g_dbus_method_invocation_return_value (invocation,
		g_variant_new ("(s)", file));
}

static void
ask_user_transfer_rejected (NotifyNotification *notification,
			    char *action,
			    GDBusMethodInvocation *invocation)
{
	g_debug ("Notification: transfer rejected! rejecting transfer");
	reject_transfer (invocation);
}

static void
ask_user_on_close (NotifyNotification *notification,
		   GDBusMethodInvocation *invocation)
{
	g_debug ("Notification closed! rejecting transfer");
	reject_transfer (invocation);
}

static void
ask_user (GDBusMethodInvocation *invocation,
	  const char            *filename,
	  const char            *name)
{
	NotifyNotification *notification;
	char *summary, *body;

	summary = g_strdup_printf(_("Bluetooth file transfer from %s"), name);
	body = g_filename_display_basename (filename);

	notification = notify_notification_new (summary, body, "bluetooth");

	notify_notification_set_urgency (notification, NOTIFY_URGENCY_CRITICAL);
	notify_notification_set_timeout (notification, NOTIFY_EXPIRES_NEVER);
	notify_notification_set_hint_string (notification, "desktop-entry",
					     "gnome-bluetooth-panel");

	notify_notification_add_action (notification, "cancel", _("Decline"),
					(NotifyActionCallback) ask_user_transfer_rejected,
					invocation, NULL);
	notify_notification_add_action (notification, "receive", _("Accept"),
					(NotifyActionCallback) ask_user_transfer_accepted,
					invocation, NULL);

	/* We want to reject the transfer if the user closes the notification
	 * without accepting or rejecting it, so we connect to it. However
	 * if the user clicks on one of the actions, the callback for the
	 * action will be invoked, and then the notification will be closed
	 * and the callback for :closed will be called. So we disconnect
	 * from :closed if the invocation object goes away (which will happen
	 * after the handler of either action accepts or rejects the transfer).
	 */
	g_signal_connect_object (G_OBJECT (notification), "closed",
		G_CALLBACK (ask_user_on_close), invocation, 0);

	if (!notify_notification_show (notification, NULL))
		g_warning ("failed to send notification\n");

	g_free (summary);
	g_free (body);
}

static gboolean
get_paired_for_address (const char  *adapter,
			const char  *device,
			char       **name)
{
	GtkTreeModel *model;
	GtkTreeIter parent;
	gboolean next;
	gboolean ret = FALSE;
	char *addr;

	model = bluetooth_client_get_model (client);

	for (next = gtk_tree_model_get_iter_first (model, &parent);
	     next;
	     next = gtk_tree_model_iter_next (model, &parent)) {
		gtk_tree_model_get (model, &parent,
			BLUETOOTH_COLUMN_ADDRESS, &addr,
			-1);

		if (g_strcmp0 (addr, adapter) == 0) {
			GtkTreeIter child;
			char *dev_addr;

			for (next = gtk_tree_model_iter_children (model, &child, &parent);
			     next;
			     next = gtk_tree_model_iter_next (model, &child)) {
				gboolean paired;
				char *alias;

				gtk_tree_model_get (model, &child,
					BLUETOOTH_COLUMN_ADDRESS, &dev_addr,
					BLUETOOTH_COLUMN_PAIRED, &paired,
					BLUETOOTH_COLUMN_ALIAS, &alias,
					-1);
				if (g_strcmp0 (dev_addr, device) == 0) {
					ret = paired;
					*name = alias;
					next = FALSE;
				} else {
					g_free (alias);
				}
				g_free (dev_addr);
			}
		}
		g_free (addr);
	}

	g_object_unref (model);
	return ret;
}

static void
on_check_bonded_or_ask_session_acquired (GObject *object,
					 GAsyncResult *res,
					 gpointer user_data)
{
	GDBusMethodInvocation *invocation = user_data;
	GDBusProxy *session;
	GError *error = NULL;
	GVariant *v;
	char *device, *adapter, *name;
	gboolean paired;

	session = g_dbus_proxy_new_for_bus_finish (res, &error);

	if (!session) {
		g_debug ("Failed to create a proxy for the session: %s", error->message);
		g_clear_error (&error);
		goto out;
	}

	device = NULL;
	adapter = NULL;

	/* obexd puts the remote device in Destination and our adapter
	 * in Source */
	v = g_dbus_proxy_get_cached_property (session, "Destination");
	if (v) {
		device = g_variant_dup_string (v, NULL);
		g_variant_unref (v);
	}

	v = g_dbus_proxy_get_cached_property (session, "Source");
	if (v) {
		adapter = g_variant_dup_string (v, NULL);
		g_variant_unref (v);
	}

	g_object_unref (session);

	if (!device || !adapter) {
		g_debug ("Could not get remote device for the transfer");
		g_free (device);
		g_free (adapter);
		goto out;
	}

	name = NULL;
	paired = get_paired_for_address (adapter, device, &name);
	g_free (device);
	g_free (adapter);

	if (paired) {
		g_debug ("Remote device '%s' is paired, auto-accepting the transfer", name);
		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(s)", g_object_get_data (G_OBJECT (invocation), "temp-filename")));
		g_free (name);
		return;
	} else {
		ask_user (invocation,
			  g_object_get_data (G_OBJECT (invocation), "filename"),
			  name ? name : device);
		g_free (name);
		return;
	}

out:
	g_debug ("Rejecting transfer");
	reject_transfer (invocation);
}

static void
check_if_bonded_or_ask (GDBusProxy *transfer,
			GDBusMethodInvocation *invocation)
{
	GVariant *v;
	const gchar *session = NULL;

	v = g_dbus_proxy_get_cached_property (transfer, "Session");

	if (v) {
		session = g_variant_get_string (v, NULL);
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  MANAGER_SERVICE,
					  session,
					  SESSION_IFACE,
					  NULL,
					  on_check_bonded_or_ask_session_acquired,
					  invocation);
		g_variant_unref (v);
	} else {
		g_debug ("Could not get session path for the transfer, "
			 "rejecting the transfer");
		reject_transfer (invocation);
	}
}


static void
obex_agent_release (GError **error)
{
}

static void
obex_agent_cancel (GError **error)
{
}

/* From the old embed/mozilla/MozDownload.cpp */
static const char*
file_is_compressed (const char *filename)
{
  int i;
  static const char * const compression[] = {".gz", ".bz2", ".Z", ".lz", ".xz", NULL};

  for (i = 0; compression[i] != NULL; i++) {
    if (g_str_has_suffix (filename, compression[i]))
      return compression[i];
  }

  return NULL;
}

static const char*
parse_extension (const char *filename)
{
  const char *compression;
  const char *last_separator;

  compression = file_is_compressed (filename);

  /* if the file is compressed we might have a double extension */
  if (compression != NULL) {
    int i;
    static const char * const extensions[] = {"tar", "ps", "xcf", "dvi", "txt", "text", NULL};

    for (i = 0; extensions[i] != NULL; i++) {
      char *suffix;
      suffix = g_strdup_printf (".%s%s", extensions[i], compression);

      if (g_str_has_suffix (filename, suffix)) {
        char *p;

        p = g_strrstr (filename, suffix);
        g_free (suffix);

        return p;
      }

      g_free (suffix);
    }
  }

  /* no compression, just look for the last dot in the filename */
  last_separator = strrchr (filename, G_DIR_SEPARATOR);
  return strrchr ((last_separator) ? last_separator : filename, '.');
}

char *
lookup_download_dir (void)
{
	const char *special_dir;
	char *dir;

	special_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD);
	if (special_dir != NULL && strcmp (special_dir, g_get_home_dir ()) != 0) {
		g_mkdir_with_parents (special_dir, 0755);
		return g_strdup (special_dir);
	}

	dir = g_build_filename (g_get_home_dir (), "Downloads", NULL);
	g_mkdir_with_parents (dir, 0755);
	return dir;
}

static char *
move_temp_filename (GObject *object)
{
	const char *orig_filename;
	char *dest_filename, *dest_dir;
	GFile *src, *dest;
	GError *error = NULL;
	gboolean res;

	orig_filename = g_object_get_data (object, "temp-filename");
	src = g_file_new_for_path (orig_filename);

	dest_dir = lookup_download_dir ();
	dest_filename = g_build_filename (dest_dir, g_object_get_data (object, "filename"), NULL);
	g_free (dest_dir);
	dest = g_file_new_for_path (dest_filename);

	res = g_file_move (src, dest,
			   G_FILE_COPY_NONE, NULL,
			   NULL, NULL, &error);

	/* This is sync, but the files will be on the same partition already
	 * (~/.cache/obexd to ~/Downloads) */
	if (!res && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
		guint i = 1;
		const char *dot_pos;
		gssize position;
		char *serial = NULL;
		GString *tmp_filename;

		dot_pos = parse_extension (dest_filename);
		if (dot_pos)
			position = dot_pos - dest_filename;
		else
			position = strlen (dest_filename);

		tmp_filename = g_string_new (NULL);
		g_string_assign (tmp_filename, dest_filename);

		while (!res && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
			g_debug ("Couldn't move file to %s", tmp_filename->str);

			g_clear_error (&error);
			g_object_unref (dest);

			serial = g_strdup_printf ("(%d)", i++);

			g_string_assign (tmp_filename, dest_filename);
			g_string_insert (tmp_filename, position, serial);

			g_free (serial);

			dest = g_file_new_for_path (tmp_filename->str);
			res = g_file_move (src, dest,
					   G_FILE_COPY_NONE, NULL,
					   NULL, NULL, &error);
		}

		g_free (dest_filename);
		dest_filename = g_strdup (tmp_filename->str);
		g_string_free (tmp_filename, TRUE);
	}

	if (!res) {
		g_warning ("Failed to move %s to %s: '%s'",
			   orig_filename, dest_filename, error->message);
		g_error_free (error);
	} else {
		g_debug ("Moved %s (orig name %s) to %s",
			 orig_filename, (char *) g_object_get_data (object, "filename"), dest_filename);
	}

	g_object_unref (src);
	g_object_unref (dest);

	return dest_filename;
}

static void
transfer_property_changed (GDBusProxy *transfer,
			   GVariant   *changed_properties,
			   GStrv       invalidated_properties,
			   gpointer    user_data)
{
	GVariantIter iter;
	const gchar *key;
	GVariant *value;
	const char *filename;

	g_debug ("Calling transfer_property_changed()");

	filename = g_object_get_data (G_OBJECT (transfer), "filename");

	g_variant_iter_init (&iter, changed_properties);
	while (g_variant_iter_next (&iter, "{&sv}", &key, &value)) {
		char *str = g_variant_print (value, TRUE);

		if (g_str_equal (key, "Status")) {
			const gchar *status;

			status = g_variant_get_string (value, NULL);

			g_debug ("Got status %s = %s for filename %s", status, str, filename);

			if (g_str_equal (status, "complete")) {
				char *path;

				path = move_temp_filename (G_OBJECT (transfer));
				g_debug ("transfer completed, showing a notification");
				show_notification (path);
				g_free (path);
			}

			/* Done with this transfer */
			if (g_str_equal (status, "complete") ||
			    g_str_equal (status, "error")) {
				g_object_unref (transfer);
			}
		} else {
			g_debug ("Unhandled property changed %s = %s for filename %s", key, str, filename);
		}
		g_free (str);
		g_variant_unref (value);
	}
}

static void
obex_agent_authorize_push (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	GDBusProxy *transfer = g_dbus_proxy_new_for_bus_finish (res, NULL);
	GDBusMethodInvocation *invocation = user_data;
	GVariant *variant = g_dbus_proxy_get_cached_property (transfer, "Name");
	const gchar *filename = g_variant_get_string (variant, NULL);
	char *template;
	int fd;

	g_debug ("AuthorizePush received");

	template = g_build_filename (g_get_user_cache_dir (), "obexd", "XXXXXX", NULL);
	fd = g_mkstemp (template);
	close (fd);

	g_object_set_data_full (G_OBJECT (transfer), "filename", g_strdup (filename), g_free);
	g_object_set_data_full (G_OBJECT (transfer), "temp-filename", g_strdup (template), g_free);

	g_object_set_data_full (G_OBJECT (invocation), "filename", g_strdup (filename), g_free);
	g_object_set_data_full (G_OBJECT (invocation), "temp-filename", g_strdup (template), g_free);

	g_signal_connect (transfer, "g-properties-changed",
			  G_CALLBACK (transfer_property_changed), NULL);

	/* check_if_bonded_or_ask() will accept or reject the transfer */
	check_if_bonded_or_ask (transfer, invocation);
	g_variant_unref (variant);
	g_free (template);
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
	if (g_str_equal (method_name, "Cancel")) {
		obex_agent_cancel (NULL);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_str_equal (method_name, "Release")) {
		obex_agent_release (NULL);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_str_equal (method_name, "AuthorizePush")) {
		const gchar *transfer;

		g_variant_get (parameters, "(&o)", &transfer);

		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
					  G_DBUS_PROXY_FLAGS_NONE,
					  NULL,
					  MANAGER_SERVICE,
					  transfer,
					  TRANSFER_IFACE,
					  NULL,
					  obex_agent_authorize_push,
					  invocation);
	} else {
		g_warning ("Unknown method name or unknown parameters: %s",
			   method_name);
	}
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL
};

static void
obexd_appeared_cb (GDBusConnection *connection,
		   const gchar *name,
		   const gchar *name_owner,
		   gpointer user_data)
{
	ObexAgent *self = user_data;

	g_debug ("obexd appeared, registering agent");
	g_dbus_connection_call (self->connection,
				MANAGER_SERVICE,
				MANAGER_PATH,
				MANAGER_IFACE,
				"RegisterAgent",
				g_variant_new ("(o)", AGENT_PATH),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				NULL,
				NULL);
}

static void
on_bus_acquired (GDBusConnection *connection,
		 const gchar     *name,
		 gpointer         user_data)
{
	ObexAgent *self = user_data;

	/* parse introspection data */
	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml,
							   NULL);

	self->connection = connection;
	self->object_reg_id = g_dbus_connection_register_object (connection,
								 AGENT_PATH,
								 introspection_data->interfaces[0],
								 &interface_vtable,
								 NULL,  /* user_data */
								 NULL,  /* user_data_free_func */
								 NULL); /* GError** */

	g_dbus_node_info_unref (introspection_data);

	g_assert (self->object_reg_id > 0);

	self->obexd_watch_id = g_bus_watch_name_on_connection (self->connection,
							       MANAGER_SERVICE,
							       G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
							       obexd_appeared_cb,
							       NULL,
							       self,
							       NULL);
}

static void
obex_agent_init (ObexAgent *self)
{
	self->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
					 AGENT_IFACE,
					 G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
					 on_bus_acquired,
					 NULL,
					 NULL,
					 self,
					 NULL);

	client = bluetooth_client_new ();
}

static void
obex_agent_dispose (GObject *obj)
{
	ObexAgent *self = OBEX_AGENT (obj);

	g_dbus_connection_unregister_object (self->connection, self->object_reg_id);
	self->object_reg_id = 0;

	g_bus_unown_name (self->owner_id);
	self->owner_id = 0;

	g_bus_unwatch_name (self->obexd_watch_id);
	self->obexd_watch_id = 0;

	g_clear_object (&client);

	G_OBJECT_CLASS (obex_agent_parent_class)->dispose (obj);
}

static void
obex_agent_class_init (ObexAgentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = obex_agent_dispose;
}

static ObexAgent *
obex_agent_new (void)
{
	return (ObexAgent *) g_object_new (OBEX_AGENT_TYPE, NULL);
}

void
obex_agent_down (void)
{
	if (agent != NULL) {
		g_dbus_connection_call (agent->connection,
					MANAGER_SERVICE,
					MANAGER_PATH,
					MANAGER_IFACE,
					"UnregisterAgent",
					g_variant_new ("(o)", AGENT_PATH),
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					NULL,
					NULL);
	}

	g_clear_object (&agent);
	g_clear_object (&client);
}

void
obex_agent_up (void)
{
	if (agent == NULL)
		agent = obex_agent_new ();

	if (!notify_init ("gnome-bluetooth")) {
		g_warning("Unable to initialize the notification system");
	}
}
