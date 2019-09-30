/* -*- tab-width: 8 -*-
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <bluetooth-applet.h>
#include <bluetooth-client.h>
#include <bluetooth-client-private.h>
#include <bluetooth-utils.h>
#include <bluetooth-killswitch.h>
#include <bluetooth-agent.h>

static gpointer
bluetooth_simple_device_copy (gpointer boxed)
{
	BluetoothSimpleDevice* origin = (BluetoothSimpleDevice*) boxed;

	BluetoothSimpleDevice* result = g_new (BluetoothSimpleDevice, 1);
	result->bdaddr = g_strdup (origin->bdaddr);
	result->device_path = g_strdup (origin->device_path);
	result->alias = g_strdup (origin->alias);
	result->connected = origin->connected;
	result->can_connect = origin->can_connect;
	result->capabilities = origin->capabilities;
	result->type = origin->type;

	return (gpointer)result;
}

static void
bluetooth_simple_device_free (gpointer boxed)
{
	BluetoothSimpleDevice* obj = (BluetoothSimpleDevice*) boxed;

	g_free (obj->device_path);
	g_free (obj->bdaddr);
	g_free (obj->alias);
	g_free (obj);
}

G_DEFINE_BOXED_TYPE(BluetoothSimpleDevice, bluetooth_simple_device, bluetooth_simple_device_copy, bluetooth_simple_device_free)

struct _BluetoothApplet
{
	GObject parent_instance;

	BluetoothKillswitch* killswitch_manager;
	BluetoothClient* client;
	GtkTreeModel* device_model;
	gulong signal_row_added;
	gulong signal_row_changed;
	gulong signal_row_deleted;
	char* default_adapter;
	BluetoothAgent* agent;
	GHashTable* pending_requests;

	gint num_adapters_powered;
	gint num_adapters_present;
};

struct _BluetoothAppletClass {
	GObjectClass parent_class;
};

G_DEFINE_TYPE(BluetoothApplet, bluetooth_applet, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_KILLSWITCH_STATE,
	PROP_DISCOVERABLE,
	PROP_FULL_MENU,
	PROP_LAST
};
static GParamSpec *properties[PROP_LAST];

enum {
	SIGNAL_DEVICES_CHANGED,

	SIGNAL_PINCODE_REQUEST,
	SIGNAL_CONFIRM_REQUEST,
	SIGNAL_AUTHORIZE_REQUEST,
	SIGNAL_CANCEL_REQUEST,

	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

/**
 * bluetooth_applet_send_to_address:
 * @applet: a #BluetoothApplet
 * @address: the target
 * @alias: the string to display for the device
 *
 * Send a file to a bluetooth device
 */
void bluetooth_applet_send_to_address (BluetoothApplet *applet,
				       const char *address,
				       const char *alias)
{
	g_return_if_fail (BLUETOOTH_IS_APPLET (applet));

	bluetooth_send_to_address (address, alias);
}

/**
 * bluetooth_applet_agent_reply_pincode:
 * @self: a #BluetoothApplet
 * @request_key: an opaque token given in the pincode-request signal
 * @pincode: (allow-none): the PIN code entered by the user, as a string, or NULL if the dialog was dismissed
 */
void
bluetooth_applet_agent_reply_pincode (BluetoothApplet *self,
				      const char      *request_key,
				      const char      *pincode)
{
	GDBusMethodInvocation* invocation;

	g_return_if_fail (BLUETOOTH_IS_APPLET (self));
	g_return_if_fail (request_key != NULL);

	invocation = g_hash_table_lookup (self->pending_requests, request_key);

	if (pincode != NULL) {
		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(s)", pincode));
	} else {
		GError *error;
		error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT,
				     "Pairing request rejected");
		g_dbus_method_invocation_return_gerror (invocation, error);
		g_error_free (error);
	}

	g_hash_table_remove (self->pending_requests, request_key);
}

/**
 * bluetooth_applet_agent_reply_passkey:
 * @self: a #BluetoothApplet
 * @request_key: an opaque token given in the pincode-request signal
 * @passkey: the numeric PIN code entered by the user, or -1 if the dialog was dismissed
 */
void
bluetooth_applet_agent_reply_passkey (BluetoothApplet *self,
				      const char      *request_key,
				      int              passkey)
{
	GDBusMethodInvocation* invocation;

	g_return_if_fail (BLUETOOTH_IS_APPLET (self));
	g_return_if_fail (request_key != NULL);

	invocation = g_hash_table_lookup (self->pending_requests, request_key);

	if (passkey != -1) {
		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(u)", passkey));
	} else {
		GError *error;
		error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT,
				     "Pairing request rejected");
		g_dbus_method_invocation_return_gerror (invocation, error);
		g_error_free (error);
	}

	g_hash_table_remove (self->pending_requests, request_key);
}

/**
 * bluetooth_applet_agent_reply_confirm:
 * @self: a #BluetoothApplet
 * @request_key: an opaque token given in the confirm-request signal
 * @confirm: %TRUE if operation was confirmed, %FALSE otherwise
 */
void
bluetooth_applet_agent_reply_confirm (BluetoothApplet *self,
				      const char      *request_key,
				      gboolean         confirm)
{
	GDBusMethodInvocation* invocation;

	g_return_if_fail (BLUETOOTH_IS_APPLET (self));
	g_return_if_fail (request_key != NULL);

	invocation = g_hash_table_lookup (self->pending_requests, request_key);

	if (confirm) {
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		GError *error;
		error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT,
				     "Confirmation request rejected");
		g_dbus_method_invocation_return_gerror (invocation, error);
		g_error_free (error);
	}

	g_hash_table_remove (self->pending_requests, request_key);
}

/**
 * bluetooth_applet_agent_reply_auth:
 * @self: a #BluetoothApplet
 * @request_key: an opaque token given in the pincode-request signal
 * @auth: %TRUE if operation was authorized, %FALSE otherwise
 * @trusted: %TRUE if the operation should be authorized automatically in the future
 */
void
bluetooth_applet_agent_reply_auth (BluetoothApplet *self,
				   const char      *request_key,
				   gboolean         auth,
				   gboolean         trusted)
{
	GDBusMethodInvocation* invocation;

	g_return_if_fail (BLUETOOTH_IS_APPLET (self));
	g_return_if_fail (request_key != NULL);

	invocation = g_hash_table_lookup (self->pending_requests, request_key);

	if (auth) {
		if (trusted)
			bluetooth_client_set_trusted (self->client, request_key, TRUE);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		GError *error;
		error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT,
				     "Confirmation request rejected");
		g_dbus_method_invocation_return_gerror (invocation, error);
		g_error_free (error);
	}

	g_hash_table_remove (self->pending_requests, request_key);
}

static char *
device_get_name (GDBusProxy *proxy, char **long_name)
{
	GVariant *value;
	GVariant *result;
	GVariant *dict;
	char *alias, *address;

	g_return_val_if_fail (long_name != NULL, NULL);

	result = g_dbus_proxy_call_sync (proxy, "GetProperties",  NULL,
					 G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
	if (result == NULL)
		return NULL;

	/* Retrieve the dictionary */
	dict  = g_variant_get_child_value (result, 0);

	value = g_variant_lookup_value (dict, "Address", G_VARIANT_TYPE_STRING);
	if (value == NULL) {
		g_variant_unref (result);
		return NULL;
	}
	address = g_strdup (g_variant_get_string (value, NULL));

	value = g_variant_lookup_value (dict, "Name", G_VARIANT_TYPE_STRING);
	alias = value ? g_strdup (g_variant_get_string (value, NULL)) : g_strdup (address);

	g_variant_unref (result);

	if (value)
		*long_name = g_strdup_printf ("'%s' (%s)", alias, address);
	else
		*long_name = g_strdup_printf ("'%s'", address);

	g_free (address);
	return alias;
}

static gboolean
pincode_request (GDBusMethodInvocation *invocation,
		 GDBusProxy            *device,
		 gpointer               user_data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (user_data);
	char *name;
	char *long_name = NULL;
	const char *path;

	name = device_get_name (device, &long_name);
	path = g_dbus_proxy_get_object_path (device);
	g_hash_table_insert (self->pending_requests, g_strdup (path), invocation);

	g_signal_emit (self, signals[SIGNAL_PINCODE_REQUEST], 0, path, name, long_name, FALSE);

	g_free (name);
	g_free (long_name);

	return TRUE;
}

static gboolean
passkey_request (GDBusMethodInvocation *invocation,
		 GDBusProxy            *device,
		 gpointer               user_data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (user_data);
	char *name;
	char *long_name = NULL;
	const char *path;

	name = device_get_name (device, &long_name);
	path = g_dbus_proxy_get_object_path (device);
	g_hash_table_insert (self->pending_requests, g_strdup (path), invocation);

	g_signal_emit (self, signals[SIGNAL_PINCODE_REQUEST], 0, path, name, long_name, TRUE);

	g_free (name);
	g_free (long_name);

	return TRUE;
}

static gboolean
confirm_request (GDBusMethodInvocation *invocation,
		 GDBusProxy *device,
		 guint pin,
		 gpointer user_data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (user_data);
	char *name;
	char *long_name = NULL;
	const char *path;

	name = device_get_name (device, &long_name);
	path = g_dbus_proxy_get_object_path (device);
	g_hash_table_insert (self->pending_requests, g_strdup (path), invocation);

	g_signal_emit (self, signals[SIGNAL_CONFIRM_REQUEST], 0, path, name, long_name, pin);

	g_free (name);
	g_free (long_name);

	return TRUE;
}

static gboolean
authorize_request (GDBusMethodInvocation *invocation,
		   GDBusProxy *device,
		   const char *uuid,
		   gpointer user_data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (user_data);
	char *name;
	char *long_name = NULL;
	const char *path;

	name = device_get_name (device, &long_name);
	path = g_dbus_proxy_get_object_path (device);
	g_hash_table_insert (self->pending_requests, g_strdup (path), invocation);

	g_signal_emit (self, signals[SIGNAL_AUTHORIZE_REQUEST], 0, path, name, long_name, uuid);

	g_free (name);
	g_free (long_name);

	return TRUE;
}

static void
cancel_request_single (gpointer key, gpointer value, gpointer user_data)
{
	GDBusMethodInvocation* request_invocation = value;
	GError* error;

	if (value) {
		error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT, "Agent callback cancelled");
		g_dbus_method_invocation_return_gerror (request_invocation, error);
		g_error_free (error);
	}
}

static gboolean
cancel_request(GDBusMethodInvocation *invocation,
               gpointer user_data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (user_data);

	g_hash_table_foreach (self->pending_requests, cancel_request_single, NULL);
	g_hash_table_remove_all (self->pending_requests);

	g_signal_emit (self, signals[SIGNAL_CANCEL_REQUEST], 0);

	return TRUE;
}

static void
device_added_or_changed (GtkTreeModel    *model,
			 GtkTreePath     *path,
			 GtkTreeIter     *child_iter,
			 BluetoothApplet *self)
{
	GtkTreeIter iter;

	/* The line that changed isn't in the filtered view */
	if (gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (self->device_model),
							      &iter,
							      child_iter) == FALSE)
		return;
	g_signal_emit (self, signals[SIGNAL_DEVICES_CHANGED], 0);
}

static void
device_removed (GtkTreeModel    *model,
	        GtkTreePath     *path,
	        BluetoothApplet *self)
{
	/* We cannot check whether the row still exists, because
	 * it's already gone */
	g_signal_emit (self, signals[SIGNAL_DEVICES_CHANGED], 0);
}

static GtkTreeModel *
get_child_model (GtkTreeModel *model)
{
	GtkTreeModel *child_model;

	if (model == NULL)
		return NULL;
	g_object_get (model, "child-model", &child_model, NULL);
	return child_model;
}

static void
default_adapter_changed (GObject    *client,
			 GParamSpec *spec,
			 gpointer    data)
{
	BluetoothApplet* self = BLUETOOTH_APPLET (data);
	GtkTreeModel *child_model;

	if (self->default_adapter) {
		g_free (self->default_adapter);
		self->default_adapter = NULL;
	}
	g_object_get (G_OBJECT (self->client), "default-adapter", &self->default_adapter, NULL);

	/* The old model */
	child_model = get_child_model (self->device_model);
	if (child_model) {
		g_signal_handler_disconnect (child_model, self->signal_row_added);
		g_signal_handler_disconnect (child_model, self->signal_row_changed);
		g_signal_handler_disconnect (child_model, self->signal_row_deleted);
		g_object_unref (child_model);
	}
	if (self->device_model)
		g_object_unref (self->device_model);

	/* The new model */
	self->device_model = bluetooth_client_get_device_model (self->client);

	child_model = get_child_model (self->device_model);
	if (child_model) {
		self->signal_row_added = g_signal_connect (child_model, "row-inserted",
							   G_CALLBACK(device_added_or_changed), self);
		self->signal_row_deleted = g_signal_connect (child_model, "row-deleted",
							     G_CALLBACK(device_removed), self);
		self->signal_row_changed = g_signal_connect (child_model, "row-changed",
							     G_CALLBACK (device_added_or_changed), self);
	}

	if (self->agent)
		g_object_unref (self->agent);

	if (self->default_adapter) {
		self->agent = bluetooth_agent_new ();
		g_object_add_weak_pointer (G_OBJECT (self->agent), (void**) &(self->agent));

		bluetooth_agent_set_pincode_func (self->agent, pincode_request, self);
		bluetooth_agent_set_passkey_func (self->agent, passkey_request, self);
		bluetooth_agent_set_authorize_func (self->agent, authorize_request, self);
		bluetooth_agent_set_confirm_func (self->agent, confirm_request, self);
		bluetooth_agent_set_cancel_func (self->agent, cancel_request, self);

		bluetooth_agent_register (self->agent);
	}

	g_signal_emit (self, signals[SIGNAL_DEVICES_CHANGED], 0);
}

static void
default_adapter_powered_changed (GObject    *client,
				 GParamSpec *spec,
				 gpointer    data)
{
        BluetoothApplet *self = BLUETOOTH_APPLET (data);

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FULL_MENU]);
}

static void
default_adapter_discoverable_changed (GObject    *client,
				 GParamSpec *spec,
				 gpointer    data)
{
        BluetoothApplet *self = BLUETOOTH_APPLET (data);

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DISCOVERABLE]);
}

static gboolean
set_powered_foreach (GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer      data)
{
	GDBusProxy *proxy = NULL;

	gtk_tree_model_get (model, iter,
			    BLUETOOTH_COLUMN_PROXY, &proxy, -1);

	if (proxy == NULL)
		return FALSE;

	g_dbus_proxy_call (proxy,
			   "SetProperty",
			   g_variant_new ("sv", "Powered", g_variant_new_boolean (TRUE)),
			   G_DBUS_CALL_FLAGS_NO_AUTO_START,
			   -1,
			   NULL,
			   NULL,
			   NULL);

	g_object_unref (proxy);

	return FALSE;
}

static void
set_adapter_powered (BluetoothApplet* self)
{
	GtkTreeModel *adapters;

	adapters = bluetooth_client_get_adapter_model (self->client);
	gtk_tree_model_foreach (adapters, set_powered_foreach, NULL);
	g_object_unref (adapters);
}

static gboolean
device_has_uuid (const char **uuids, const char *uuid)
{
	guint i;

	if (uuids == NULL)
		return FALSE;

	for (i = 0; uuids[i] != NULL; i++) {
		if (g_str_equal (uuid, uuids[i]) != FALSE)
			return TRUE;
	}
	return FALSE;
}

static void
killswitch_state_change (BluetoothKillswitch *kill_switch, BluetoothKillswitchState state, gpointer user_data)
{
  BluetoothApplet *self = BLUETOOTH_APPLET (user_data);

  g_object_notify (G_OBJECT (self), "killswitch-state");
}

typedef struct {
	BluetoothApplet* self;
	BluetoothAppletConnectFunc func;
	gpointer user_data;
} ConnectionClosure;

static void

connection_callback (GObject      *source_object,
		     GAsyncResult *res,
		     gpointer      user_data)
{
	ConnectionClosure *closure = (ConnectionClosure*) user_data;
	gboolean success;

	success = bluetooth_client_connect_service_finish (BLUETOOTH_CLIENT (source_object), res, NULL);

	(*(closure->func)) (closure->self, success, closure->user_data);

	g_free (closure);
}

/**
 * bluetooth_applet_connect_device:
 * @applet: a #BluetoothApplet
 * @device: the device to connect
 * @func: (scope async): a completion callback
 * @data: user data
 */
gboolean
bluetooth_applet_connect_device (BluetoothApplet* applet,
                                 const char* device,
                                 BluetoothAppletConnectFunc func,
                                 gpointer data)
{
	ConnectionClosure *closure;

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (applet), FALSE);
	g_return_val_if_fail (device != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	closure = g_new (ConnectionClosure, 1);
	closure->self = applet;
	closure->func = func;
	closure->user_data = data;

	bluetooth_client_connect_service (applet->client, device, TRUE, NULL, connection_callback, closure);

	return TRUE;
}

/**
 * bluetooth_applet_disconnect_device:
 * @applet: a #BluetoothApplet
 * @device: the device to disconnect
 * @func: (scope async): a completion callback
 * @data: user data
 */
gboolean
bluetooth_applet_disconnect_device (BluetoothApplet* applet,
                                 const char* device,
                                 BluetoothAppletConnectFunc func,
                                 gpointer data)
{
	ConnectionClosure *closure;

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (applet), FALSE);
	g_return_val_if_fail (device != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	closure = g_new (ConnectionClosure, 1);
	closure->self = applet;
	closure->func = func;
	closure->user_data = data;

	bluetooth_client_connect_service (applet->client, device, FALSE, NULL, connection_callback, closure);

	return TRUE;
}

/**
 * bluetooth_applet_get_discoverable:
 * @self: a #BluetoothApplet
 *
 * Returns: %TRUE if the default adapter is discoverable, %FALSE otherwise
 */
gboolean
bluetooth_applet_get_discoverable (BluetoothApplet* self)
{
	gboolean ret;
	g_return_val_if_fail (BLUETOOTH_IS_APPLET (self), FALSE);

	g_object_get (G_OBJECT (self->client), "default-adapter-discoverable", &ret, NULL);
	return ret;
}

/**
 * bluetooth_applet_set_discoverable:
 * @self: a #BluetoothApplet
 * @visible:
 */
void
bluetooth_applet_set_discoverable (BluetoothApplet* self, gboolean visible)
{
	g_return_if_fail (BLUETOOTH_IS_APPLET (self));

	g_object_set (G_OBJECT (self->client), "default-adapter-discoverable", visible, NULL);
}

/**
 * bluetooth_applet_get_killswitch_state:
 * @self: a #BluetoothApplet
 *
 * Returns: the state of the killswitch, if one is present, or BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER otherwise
 */
BluetoothKillswitchState
bluetooth_applet_get_killswitch_state (BluetoothApplet* self)
{

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (self), BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER);

	if (bluetooth_killswitch_has_killswitches (self->killswitch_manager))
		return bluetooth_killswitch_get_state (self->killswitch_manager);
	else
		return BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER;
}

/**
 * bluetooth_applet_set_killswitch_state:
 * @self: a #BluetoothApplet
 * @state: the new state
 *
 * Returns: %TRUE if the operation could be performed, %FALSE otherwise
 */
gboolean
bluetooth_applet_set_killswitch_state (BluetoothApplet* self, BluetoothKillswitchState state)
{

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (self), FALSE);

	if (bluetooth_killswitch_has_killswitches (self->killswitch_manager)) {
		bluetooth_killswitch_set_state (self->killswitch_manager, state);
		return TRUE;
	}
	return FALSE;
}

/**
 * bluetooth_applet_get_show_full_menu:
 * @self: a #BluetoothApplet
 *
 * Returns: %TRUE if the full menu is to be shown, %FALSE otherwise
 * (full menu includes device submenus and global actions)
 */
gboolean
bluetooth_applet_get_show_full_menu (BluetoothApplet* self)
{
	gboolean has_adapter, has_powered_adapter;
	g_return_val_if_fail (BLUETOOTH_IS_APPLET (self), FALSE);

	has_adapter = self->default_adapter != NULL;
	g_object_get (G_OBJECT (self->client), "default-adapter-powered", &has_powered_adapter, NULL);

	if (!has_adapter)
		return FALSE;

	return has_powered_adapter &&
		bluetooth_applet_get_killswitch_state(self) == BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED;
}

static BluetoothSimpleDevice *
bluetooth_applet_create_device_from_iter (GtkTreeModel *model,
					  GtkTreeIter  *iter,
					  gboolean      check_proxy)
{
	BluetoothSimpleDevice *dev;
	GHashTable *services;
	GDBusProxy *proxy;
	char **uuids;

	dev = g_new0 (BluetoothSimpleDevice, 1);

	gtk_tree_model_get (model, iter,
			    BLUETOOTH_COLUMN_ADDRESS, &dev->bdaddr,
			    BLUETOOTH_COLUMN_PROXY, &proxy,
			    BLUETOOTH_COLUMN_SERVICES, &services,
			    BLUETOOTH_COLUMN_ALIAS, &dev->alias,
			    BLUETOOTH_COLUMN_UUIDS, &uuids,
			    BLUETOOTH_COLUMN_TYPE, &dev->type,
			    -1);

	if (dev->bdaddr == NULL || dev->alias == NULL ||
	    (check_proxy != FALSE && proxy == NULL)) {
		if (proxy != NULL)
			g_object_unref (proxy);
		g_strfreev (uuids);
		if (services != NULL)
			g_hash_table_unref (services);
		bluetooth_simple_device_free (dev);

		return NULL;
	}

	if (proxy != NULL) {
		dev->device_path = g_strdup (g_dbus_proxy_get_object_path (proxy));
		g_object_unref (proxy);
	}

	/* If one service is connected, then we're connected */
	dev->connected = FALSE;
	dev->can_connect = FALSE;
	if (services != NULL) {
		GList *list, *l;

		dev->can_connect = TRUE;
		list = g_hash_table_get_values (services);
		for (l = list; l != NULL; l = l->next) {
			BluetoothStatus val = GPOINTER_TO_INT (l->data);
			if (val == BLUETOOTH_STATUS_CONNECTED ||
			    val == BLUETOOTH_STATUS_PLAYING) {
				dev->connected = TRUE;
				break;
			}
		}
		g_list_free (list);
	}

	dev->capabilities = 0;
	dev->capabilities |= device_has_uuid ((const char **) uuids, "OBEXObjectPush") ? BLUETOOTH_CAPABILITIES_OBEX_PUSH : 0;
	dev->capabilities |= device_has_uuid ((const char **) uuids, "OBEXFileTransfer") ? BLUETOOTH_CAPABILITIES_OBEX_FILE_TRANSFER : 0;

	if (services != NULL)
		g_hash_table_unref (services);
	g_strfreev (uuids);

	return dev;
}

/**
 * bluetooth_applet_get_devices:
 * @self: a #BluetoothApplet
 *
 * Returns: (element-type GnomeBluetoothApplet.SimpleDevice) (transfer full): Returns the devices which should be shown to the user
 */
GList*
bluetooth_applet_get_devices (BluetoothApplet* self)
{
	GList* result = NULL;
	GtkTreeIter iter;
	gboolean cont;

	g_return_val_if_fail (BLUETOOTH_IS_APPLET (self), NULL);

	/* No adapter */
	if (self->default_adapter == NULL)
		return NULL;

	cont = gtk_tree_model_get_iter_first (self->device_model, &iter);
	while (cont) {
		BluetoothSimpleDevice *dev;

		dev = bluetooth_applet_create_device_from_iter (self->device_model, &iter, TRUE);

		if (dev != NULL)
			result = g_list_prepend (result, dev);

		cont = gtk_tree_model_iter_next (self->device_model, &iter);
	}

	result = g_list_reverse (result);

	return result;
}

static void
bluetooth_applet_get_property (GObject    *self,
			       guint       property_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_FULL_MENU:
		g_value_set_boolean (value, bluetooth_applet_get_show_full_menu (BLUETOOTH_APPLET (self)));
		return;
	case PROP_KILLSWITCH_STATE:
		g_value_set_int (value, bluetooth_applet_get_killswitch_state (BLUETOOTH_APPLET (self)));
		return;
	case PROP_DISCOVERABLE:
		g_value_set_boolean (value, bluetooth_applet_get_discoverable (BLUETOOTH_APPLET (self)));
		return;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
	}
}

static void
bluetooth_applet_set_property (GObject      *gobj,
			       guint         property_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	BluetoothApplet *self = BLUETOOTH_APPLET (gobj);

	switch (property_id) {
	case PROP_KILLSWITCH_STATE:
		bluetooth_applet_set_killswitch_state (self, g_value_get_int (value));
		return;
	case PROP_DISCOVERABLE:
		bluetooth_applet_set_discoverable (self, g_value_get_boolean (value));
		return;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
	}
}

static void
bluetooth_applet_init (BluetoothApplet *self)
{
	self->client = bluetooth_client_new ();
	self->device_model = NULL;

	self->default_adapter = NULL;
	self->agent = NULL;

	self->killswitch_manager = bluetooth_killswitch_new ();
	g_signal_connect (self->killswitch_manager, "state-changed", G_CALLBACK(killswitch_state_change), self);

	self->pending_requests = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	g_dbus_error_register_error (AGENT_ERROR, AGENT_ERROR_REJECT, "org.bluez.Error.Rejected");

	/* Make sure all the unblocked adapters are powered,
	 * so as to avoid seeing unpowered, but unblocked
	 * devices */
	set_adapter_powered (self);
	default_adapter_changed (NULL, NULL, self);

	g_signal_connect (self->client, "notify::default-adapter", G_CALLBACK (default_adapter_changed), self);
	g_signal_connect (self->client, "notify::default-adapter-powered", G_CALLBACK (default_adapter_powered_changed), self);
	g_signal_connect (self->client, "notify::default-adapter-discoverable", G_CALLBACK (default_adapter_discoverable_changed), self);
}

static void
bluetooth_applet_dispose (GObject* self)
{

	BluetoothApplet* applet = BLUETOOTH_APPLET (self);

	if (applet->client) {
		g_object_unref (applet->client);
		applet->client = NULL;
	}

	if (applet->killswitch_manager) {
		g_object_unref (applet->killswitch_manager);
		applet->killswitch_manager = NULL;
	}

	if (applet->device_model) {
		g_object_unref (applet->device_model);
		applet->device_model = NULL;
	}

	if (applet->agent) {
		g_object_unref (applet->agent);
		applet->agent = NULL;
	}
}

static void
bluetooth_applet_class_init (BluetoothAppletClass *klass)
{
	GObjectClass* gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = bluetooth_applet_dispose;
	gobject_class->get_property = bluetooth_applet_get_property;
	gobject_class->set_property = bluetooth_applet_set_property;

	/* should be enum, but BluetoothKillswitchState is not registered */
	properties[PROP_KILLSWITCH_STATE] = g_param_spec_int ("killswitch-state",
							      "Killswitch state",
							      "State of Bluetooth hardware switches",
							      BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER, BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED, BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER,
							      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (gobject_class, PROP_KILLSWITCH_STATE, properties[PROP_KILLSWITCH_STATE]);

	properties[PROP_DISCOVERABLE] = g_param_spec_boolean ("discoverable",
							      "Adapter visibility",
							      "Whether the adapter is visible or not",
							      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (gobject_class, PROP_DISCOVERABLE, properties[PROP_DISCOVERABLE]);

	properties[PROP_FULL_MENU] = g_param_spec_boolean ("show-full-menu",
							   "Show the full applet menu",
							   "Show actions related to the adapter and other miscellaneous in the main menu",
							   TRUE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
	g_object_class_install_property (gobject_class, PROP_FULL_MENU, properties[PROP_FULL_MENU]);

	signals[SIGNAL_DEVICES_CHANGED] = g_signal_new ("devices-changed", G_TYPE_FROM_CLASS (gobject_class),
							G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE, 0, NULL, NULL, NULL,
							G_TYPE_NONE, 0);

	signals[SIGNAL_PINCODE_REQUEST] = g_signal_new ("pincode-request", G_TYPE_FROM_CLASS (gobject_class),
							G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
							G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

	signals[SIGNAL_CONFIRM_REQUEST] = g_signal_new ("confirm-request", G_TYPE_FROM_CLASS (gobject_class),
							G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
							G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);

	signals[SIGNAL_AUTHORIZE_REQUEST] = g_signal_new ("auth-request", G_TYPE_FROM_CLASS (gobject_class),
							  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
							  G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	signals[SIGNAL_CANCEL_REQUEST] = g_signal_new ("cancel-request", G_TYPE_FROM_CLASS (gobject_class),
						       G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
						       G_TYPE_NONE, 0);
}

