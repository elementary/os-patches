/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <gio/gio.h>

#include "bluetooth-client-glue.h"
#include "bluetooth-agent.h"

#define BLUEZ_SERVICE	"org.bluez"

#define BLUEZ_MANAGER_PATH	"/"
#define BLUEZ_MANAGER_INTERFACE	"org.bluez.Manager"
#define BLUEZ_DEVICE_INTERFACE	"org.bluez.Device"

static const gchar introspection_xml[] =
"<node name='/'>"
"  <interface name='org.bluez.Agent'>"
"    <method name='Release'/>"
"    <method name='RequestPinCode'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='s' name='pincode' direction='out'/>"
"    </method>"
"    <method name='RequestPasskey'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='u' name='passkey' direction='out'/>"
"    </method>"
"    <method name='DisplayPasskey'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='u' name='passkey' direction='in'/>"
"      <arg type='y' name='entered' direction='in'/>"
"    </method>"
"    <method name='RequestConfirmation'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='u' name='passkey' direction='in'/>"
"    </method>"
"    <method name='Authorize'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='s' name='uuid' direction='in'/>"
"    </method>"
"    <method name='ConfirmMode'>"
"      <arg type='s' name='mode'/>"
"    </method>"
"    <method name='Cancel'/>"
"  </interface>"
"</node>";

#define BLUETOOTH_AGENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
				BLUETOOTH_TYPE_AGENT, BluetoothAgentPrivate))

typedef struct _BluetoothAgentPrivate BluetoothAgentPrivate;

struct _BluetoothAgentPrivate {
	GDBusConnection *conn;
	gchar *busname;
	gchar *path;
	GDBusProxy *adapter;
	GDBusNodeInfo *introspection_data;
	guint reg_id;
	guint watch_id;

	BluetoothAgentPasskeyFunc pincode_func;
	gpointer pincode_data;

	BluetoothAgentDisplayFunc display_func;
	gpointer display_data;

	BluetoothAgentPasskeyFunc passkey_func;
	gpointer passkey_data;

	BluetoothAgentConfirmFunc confirm_func;
	gpointer confirm_data;

	BluetoothAgentAuthorizeFunc authorize_func;
	gpointer authorize_data;

	BluetoothAgentCancelFunc cancel_func;
	gpointer cancel_data;
};

G_DEFINE_TYPE(BluetoothAgent, bluetooth_agent, G_TYPE_OBJECT)

static GDBusProxy *
get_device_from_adapter (BluetoothAgent *agent,
			 const char *path)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	if (priv->adapter == NULL)
		return NULL;

	return g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					      G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
					      NULL,
					      g_dbus_proxy_get_name (priv->adapter),
					      path,
					      BLUEZ_DEVICE_INTERFACE,
					      NULL,
					      NULL);
}

static gboolean bluetooth_agent_request_pin_code(BluetoothAgent *agent,
			const char *path, GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;
	gboolean result = FALSE;

	if (priv->pincode_func) {
		device = get_device_from_adapter (agent, path);

		result = priv->pincode_func(invocation, device,
							priv->pincode_data);

		if (device != NULL)
			g_object_unref(device);
	}

	return result;
}

static gboolean bluetooth_agent_request_passkey(BluetoothAgent *agent,
			const char *path, GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;
	gboolean result = FALSE;

	if (priv->passkey_func) {
		device = get_device_from_adapter (agent, path);

		result = priv->passkey_func(invocation, device,
							priv->passkey_data);

		if (device != NULL)
			g_object_unref(device);
	}

	return result;
}

static gboolean bluetooth_agent_display_passkey(BluetoothAgent *agent,
			const char *path, guint passkey, guint8 entered,
						GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;
	gboolean result = FALSE;

	if (priv->display_func) {
		device = get_device_from_adapter (agent, path);

		result = priv->display_func(invocation, device, passkey, entered,
							priv->display_data);

		if (device != NULL)
			g_object_unref(device);
	}

	return result;
}

static gboolean bluetooth_agent_request_confirmation(BluetoothAgent *agent,
					const char *path, guint passkey,
						GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;
	gboolean result = FALSE;

	if (priv->confirm_func) {
		device = get_device_from_adapter (agent, path);

		result = priv->confirm_func(invocation, device, passkey,
							priv->confirm_data);

		if (device != NULL)
			g_object_unref(device);
	}

	return result;
}

static gboolean bluetooth_agent_authorize(BluetoothAgent *agent,
					const char *path, const char *uuid,
						GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;
	gboolean result = FALSE;

	if (priv->authorize_func) {
		device = get_device_from_adapter (agent, path);

		result = priv->authorize_func(invocation, device, uuid,
							priv->authorize_data);

		if (device != NULL)
			g_object_unref(device);
	}

	return result;
}

static gboolean bluetooth_agent_cancel(BluetoothAgent *agent,
						GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	gboolean result = FALSE;

	if (priv->cancel_func)
		result = priv->cancel_func(invocation, priv->cancel_data);

	return result;
}

static void
name_appeared_cb (GDBusConnection *connection,
		  const gchar     *name,
		  const gchar     *name_owner,
		  BluetoothAgent  *agent)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	g_free (priv->busname);
	priv->busname = g_strdup (name_owner);
}

static void
name_vanished_cb (GDBusConnection *connection,
		  const gchar     *name,
		  BluetoothAgent  *agent)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	g_free (priv->busname);
	priv->busname = NULL;
}

static void bluetooth_agent_init(BluetoothAgent *agent)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
	g_assert (priv->introspection_data);
	priv->conn = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
	priv->watch_id = g_bus_watch_name_on_connection (priv->conn,
							 BLUEZ_SERVICE,
							 G_BUS_NAME_WATCHER_FLAGS_NONE,
							 (GBusNameAppearedCallback) name_appeared_cb,
							 (GBusNameVanishedCallback) name_vanished_cb,
							 agent,
							 NULL);
}

static void bluetooth_agent_finalize(GObject *agent)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	bluetooth_agent_unregister (BLUETOOTH_AGENT (agent));

	g_bus_unwatch_name (priv->watch_id);
	g_free (priv->busname);
	g_dbus_node_info_unref (priv->introspection_data);
	g_object_unref (priv->conn);

	G_OBJECT_CLASS(bluetooth_agent_parent_class)->finalize(agent);
}

static void bluetooth_agent_class_init(BluetoothAgentClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	g_type_class_add_private(klass, sizeof(BluetoothAgentPrivate));

	object_class->finalize = bluetooth_agent_finalize;
}

BluetoothAgent *
bluetooth_agent_new (void)
{
	return BLUETOOTH_AGENT (g_object_new (BLUETOOTH_TYPE_AGENT, NULL));
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
	BluetoothAgent *agent = (BluetoothAgent *) user_data;
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	if (g_str_equal (sender, priv->busname) == FALSE) {
		g_assert_not_reached ();
		/* FIXME, should this just be a D-Bus Error instead? */
	}

	if (g_strcmp0 (method_name, "Release") == 0) {
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "RequestPinCode") == 0) {
		char *path;
		g_variant_get (parameters, "(o)", &path);
		bluetooth_agent_request_pin_code (agent, path, invocation);
		g_free (path);
	} else if (g_strcmp0 (method_name, "RequestPasskey") == 0) {
		char *path;
		g_variant_get (parameters, "(o)", &path);
		bluetooth_agent_request_passkey (agent, path, invocation);
		g_free (path);
	} else if (g_strcmp0 (method_name, "DisplayPasskey") == 0) {
		char *path;
		guint32 passkey;
		guint8 entered;

		g_variant_get (parameters, "(ouy)", &path, &passkey, &entered);
		bluetooth_agent_display_passkey (agent, path, passkey, entered, invocation);
		g_free (path);
	} else if (g_strcmp0 (method_name, "RequestConfirmation") == 0) {
		char *path;
		guint32 passkey;

		g_variant_get (parameters, "(ou)", &path, &passkey);
		bluetooth_agent_request_confirmation (agent, path, passkey, invocation);
		g_free (path);
	} else if (g_strcmp0 (method_name, "Authorize") == 0) {
		char *path, *uuid;
		g_variant_get (parameters, "(os)", &path, &uuid);
		bluetooth_agent_authorize (agent, path, uuid, invocation);
		g_free (path);
		g_free (uuid);
	} else if (g_strcmp0 (method_name, "Cancel") == 0) {
		bluetooth_agent_cancel (agent, invocation);
	} else if (g_strcmp0 (method_name, "ConfirmMode") == 0) {
		g_dbus_method_invocation_return_value (invocation, NULL);
	}
}

static const GDBusInterfaceVTable interface_vtable =
{
	handle_method_call,
	NULL, /* GetProperty */
	NULL, /* SetProperty */
};

gboolean bluetooth_agent_setup(BluetoothAgent *agent, const char *path)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GError *error = NULL;

	if (priv->path != NULL) {
		g_warning ("Agent already setup on '%s'", priv->path);
		return FALSE;
	}

	priv->path = g_strdup(path);

	priv->reg_id = g_dbus_connection_register_object (priv->conn,
						      priv->path,
						      priv->introspection_data->interfaces[0],
						      &interface_vtable,
						      agent,
						      NULL,
						      &error);
	if (priv->reg_id == 0) {
		g_warning ("Failed to register object: %s", error->message);
		g_error_free (error);
	}

	return TRUE;
}

#define BLUEZ_SERVICE			"org.bluez"
#define BLUEZ_MANAGER_INTERFACE		"org.bluez.Manager"

static GDBusProxy *
get_default_adapter (void)
{
	Manager *manager;
	char *adapter_path;
	Adapter *adapter;

	manager = manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
						  BLUEZ_SERVICE,
						  BLUEZ_MANAGER_PATH,
						  NULL,
						  NULL);
	if (manager == NULL)
		return NULL;
	if (manager_call_default_adapter_sync (manager, &adapter_path, NULL, NULL) == FALSE) {
		g_object_unref (manager);
		return NULL;
	}
	adapter = adapter_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
						  BLUEZ_SERVICE,
						  adapter_path,
						  NULL,
						  NULL);
	g_object_unref (manager);
	g_free (adapter_path);

	return G_DBUS_PROXY (adapter);
}

gboolean bluetooth_agent_register(BluetoothAgent *agent)
{
	BluetoothAgentPrivate *priv;
	GError *error = NULL;
	char *path;
	GVariant *r;

	g_return_val_if_fail (BLUETOOTH_IS_AGENT (agent), FALSE);

	priv = BLUETOOTH_AGENT_GET_PRIVATE (agent);

	priv->adapter = get_default_adapter ();

	if (priv->adapter == NULL)
		return FALSE;

	if (priv->path != NULL) {
		g_warning ("Agent already setup on '%s'", priv->path);
		return FALSE;
	}

	path = g_path_get_basename(g_dbus_proxy_get_object_path(priv->adapter));
	priv->path = g_strdup_printf("/org/bluez/agent/%s", path);
	g_free(path);

	priv->reg_id = g_dbus_connection_register_object (priv->conn,
						      priv->path,
						      priv->introspection_data->interfaces[0],
						      &interface_vtable,
						      agent,
						      NULL,
						      &error);
	if (priv->reg_id == 0) {
		g_warning ("Failed to register object: %s", error->message);
		g_error_free (error);
		error = NULL;
		return FALSE;
	}

	r = g_dbus_proxy_call_sync (priv->adapter, "RegisterAgent",
				    g_variant_new ("(os)", priv->path, "DisplayYesNo"),
				    G_DBUS_CALL_FLAGS_NONE,
				    -1, NULL, &error);
	if (r == NULL) {
		g_printerr ("Agent registration failed: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}
	g_variant_unref (r);

	return TRUE;
}

gboolean bluetooth_agent_unregister(BluetoothAgent *agent)
{
	BluetoothAgentPrivate *priv;
	GError *error = NULL;

	g_return_val_if_fail (BLUETOOTH_IS_AGENT (agent), FALSE);

	priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	if (priv->adapter == NULL)
		return FALSE;

	if (g_dbus_proxy_call_sync (priv->adapter, "UnregisterAgent",
				    g_variant_new ("(o)", priv->path),
				    G_DBUS_CALL_FLAGS_NONE,
				    -1, NULL, &error) == FALSE) {
		/* Ignore errors if the adapter is gone */
		if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD) == FALSE) {
			g_printerr ("Agent unregistration failed: %s '%s'\n",
				    error->message,
				    g_quark_to_string (error->domain));
		}
		g_error_free(error);
	}

	g_object_unref(priv->adapter);
	priv->adapter = NULL;

	g_free(priv->path);
	priv->path = NULL;

	g_free(priv->busname);
	priv->busname = NULL;

	if (priv->reg_id > 0) {
		g_dbus_connection_unregister_object (priv->conn, priv->reg_id);
		priv->reg_id = 0;
	}

	return TRUE;
}

void bluetooth_agent_set_pincode_func(BluetoothAgent *agent,
				BluetoothAgentPasskeyFunc func, gpointer data)
{
	BluetoothAgentPrivate *priv;

	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	priv->pincode_func = func;
	priv->pincode_data = data;
}

void bluetooth_agent_set_passkey_func(BluetoothAgent *agent,
				BluetoothAgentPasskeyFunc func, gpointer data)
{
	BluetoothAgentPrivate *priv;

	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	priv->passkey_func = func;
	priv->passkey_data = data;
}

void bluetooth_agent_set_display_func(BluetoothAgent *agent,
				BluetoothAgentDisplayFunc func, gpointer data)
{
	BluetoothAgentPrivate *priv;

	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	priv->display_func = func;
	priv->display_data = data;
}

void bluetooth_agent_set_confirm_func(BluetoothAgent *agent,
				BluetoothAgentConfirmFunc func, gpointer data)
{
	BluetoothAgentPrivate *priv;

	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	priv->confirm_func = func;
	priv->confirm_data = data;
}

void bluetooth_agent_set_authorize_func(BluetoothAgent *agent,
				BluetoothAgentAuthorizeFunc func, gpointer data)
{
	BluetoothAgentPrivate *priv;

	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	priv->authorize_func = func;
	priv->authorize_data = data;
}

void bluetooth_agent_set_cancel_func(BluetoothAgent *agent,
				BluetoothAgentCancelFunc func, gpointer data)
{
	BluetoothAgentPrivate *priv;

	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	priv->cancel_func = func;
	priv->cancel_data = data;
}

GQuark bluetooth_agent_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("agent");

	return quark;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

GType bluetooth_agent_error_get_type(void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			ENUM_ENTRY(AGENT_ERROR_REJECT, "Rejected"),
			{ 0, 0, 0 }
		};

		etype = g_enum_register_static("agent", values);
	}

	return etype;
}

