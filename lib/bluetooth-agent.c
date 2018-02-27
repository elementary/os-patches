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

#define BLUEZ_SERVICE			"org.bluez"
#define BLUEZ_AGENT_PATH		"/org/bluez/agent/gnome"
#define BLUEZ_MANAGER_PATH		"/"

static const gchar introspection_xml[] =
"<node name='/'>"
"  <interface name='org.bluez.Agent1'>"
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
"      <arg type='q' name='entered' direction='in'/>"
"    </method>"
"    <method name='DisplayPinCode'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='s' name='pincode' direction='in'/>"
"    </method>"
"    <method name='RequestConfirmation'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='u' name='passkey' direction='in'/>"
"    </method>"
"    <method name='RequestAuthorization'>"
"      <arg type='o' name='device' direction='in'/>"
"    </method>"
"    <method name='AuthorizeService'>"
"      <arg type='o' name='device' direction='in'/>"
"      <arg type='s' name='uuid' direction='in'/>"
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
	AgentManager1 *agent_manager;
	GDBusNodeInfo *introspection_data;
	guint reg_id;
	guint watch_id;

	BluetoothAgentPasskeyFunc pincode_func;
	gpointer pincode_data;

	BluetoothAgentDisplayFunc display_func;
	gpointer display_data;

	BluetoothAgentDisplayPinCodeFunc display_pincode_func;
	gpointer display_pincode_data;

	BluetoothAgentPasskeyFunc passkey_func;
	gpointer passkey_data;

	BluetoothAgentConfirmFunc confirm_func;
	gpointer confirm_data;

	BluetoothAgentAuthorizeFunc authorize_func;
	gpointer authorize_data;

	BluetoothAgentAuthorizeServiceFunc authorize_service_func;
	gpointer authorize_service_data;

	BluetoothAgentCancelFunc cancel_func;
	gpointer cancel_data;
};

enum {
  PROP_0,
  PROP_PATH,
  PROP_LAST
};

static GParamSpec *props[PROP_LAST];

static GDBusProxy *
get_device_from_path (BluetoothAgentPrivate *priv,
		      const char            *path)
{
	Device1 *device;

	device = device1_proxy_new_sync (priv->conn,
					 G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
					 BLUEZ_SERVICE,
					 path,
					 NULL,
					 NULL);

	return G_DBUS_PROXY(device);
}

G_DEFINE_TYPE(BluetoothAgent, bluetooth_agent, G_TYPE_OBJECT)

static gboolean bluetooth_agent_request_pincode(BluetoothAgent *agent,
			const char *path, GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;

	if (priv->pincode_func == NULL)
		return FALSE;

	device = get_device_from_path (priv, path);
	if (device == NULL)
		return FALSE;

	priv->pincode_func(invocation, device, priv->pincode_data);

	g_object_unref(device);

	return TRUE;
}

static gboolean bluetooth_agent_request_passkey(BluetoothAgent *agent,
			const char *path, GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;

	if (priv->passkey_func == NULL)
		return FALSE;

	device = get_device_from_path (priv, path);
	if (device == NULL)
		return FALSE;

	priv->passkey_func(invocation, device, priv->passkey_data);

	g_object_unref(device);

	return TRUE;
}

static gboolean bluetooth_agent_display_passkey(BluetoothAgent *agent,
			const char *path, guint passkey, guint16 entered,
						GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;

	if (priv->display_func == NULL)
		return FALSE;

	device = get_device_from_path (priv, path);
	if (device == NULL)
		return FALSE;

	priv->display_func(invocation, device, passkey, entered,
			   priv->display_data);

	g_object_unref(device);

	return TRUE;
}

static gboolean bluetooth_agent_display_pincode(BluetoothAgent *agent,
						const char *path, const char *pincode,
						GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;

	if (priv->display_pincode_func == NULL)
		return FALSE;

	device = get_device_from_path (priv, path);
	if (device == NULL)
		return FALSE;

	priv->display_pincode_func(invocation, device, pincode,
				   priv->display_data);

	g_object_unref(device);

	return TRUE;
}

static gboolean bluetooth_agent_request_confirmation(BluetoothAgent *agent,
					const char *path, guint passkey,
						GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;

	if (priv->confirm_func == NULL)
		return FALSE;

	device = get_device_from_path (priv, path);
	if (device == NULL)
		return FALSE;

	priv->confirm_func(invocation, device, passkey, priv->confirm_data);

	g_object_unref(device);

	return TRUE;
}

static gboolean bluetooth_agent_request_authorization(BluetoothAgent *agent,
					const char *path, GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;

	if (priv->authorize_func == NULL)
		return FALSE;

	device = get_device_from_path (priv, path);
	if (device == NULL)
		return FALSE;

	priv->authorize_func(invocation, device, priv->authorize_data);

	g_object_unref(device);

	return TRUE;
}

static gboolean bluetooth_agent_authorize_service(BluetoothAgent *agent,
					const char *path, const char *uuid,
						GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);
	GDBusProxy *device;

	if (priv->authorize_service_func == NULL)
		return FALSE;

	device = get_device_from_path (priv, path);
	if (device == NULL)
		return FALSE;

	priv->authorize_service_func(invocation, device, uuid,
					    priv->authorize_service_data);

	g_object_unref(device);

	return TRUE;
}

static gboolean bluetooth_agent_cancel(BluetoothAgent *agent,
						GDBusMethodInvocation *invocation)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	if (priv->cancel_func == NULL)
		return FALSE;

	return priv->cancel_func(invocation, priv->cancel_data);
}

static void
register_agent (BluetoothAgentPrivate *priv)
{
	GError *error = NULL;
	gboolean ret;

	ret = agent_manager1_call_register_agent_sync (priv->agent_manager,
						       priv->path,
						       "DisplayYesNo",
						       NULL, &error);
	if (ret == FALSE) {
		g_printerr ("Agent registration failed: %s\n", error->message);
		g_error_free (error);
		return;
	}

	ret = agent_manager1_call_request_default_agent_sync (priv->agent_manager,
							      priv->path,
							      NULL, &error);
	if (ret == FALSE) {
		g_printerr ("Agent registration as default failed: %s\n", error->message);
		g_error_free (error);
	}
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

	priv->agent_manager = agent_manager1_proxy_new_sync (priv->conn,
							     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
							     BLUEZ_SERVICE,
							     "/org/bluez",
							     NULL,
							     NULL);

	if (priv->reg_id > 0)
		register_agent (priv);
}

static void
name_vanished_cb (GDBusConnection *connection,
		  const gchar     *name,
		  BluetoothAgent  *agent)
{
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	g_free (priv->busname);
	priv->busname = NULL;
	g_clear_object (&priv->agent_manager);
}

static void
bluetooth_agent_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	BluetoothAgent *agent = BLUETOOTH_AGENT (object);
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE (agent);

	switch (prop_id) {
	case PROP_PATH:
		g_value_set_string (value, priv->path);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
bluetooth_agent_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	BluetoothAgent *agent = BLUETOOTH_AGENT (object);
	BluetoothAgentPrivate *priv = BLUETOOTH_AGENT_GET_PRIVATE (agent);

	switch (prop_id) {
	case PROP_PATH:
		priv->path = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
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
	object_class->set_property = bluetooth_agent_set_property;
	object_class->get_property = bluetooth_agent_get_property;

	props[PROP_PATH] =
		g_param_spec_string ("path", "Path",
				     "Object path for the agent",
				     BLUEZ_AGENT_PATH,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class,
					   PROP_LAST,
					   props);

}

BluetoothAgent *
bluetooth_agent_new (const char *path)
{
	if (path != NULL)
		return BLUETOOTH_AGENT (g_object_new (BLUETOOTH_TYPE_AGENT,
						      "path", path,
						      NULL));
	else
		return BLUETOOTH_AGENT (g_object_new (BLUETOOTH_TYPE_AGENT,
						      NULL));
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
		GError *error = NULL;
		error = g_error_new (AGENT_ERROR, AGENT_ERROR_REJECT,
				     "Permission Denied");
		g_dbus_method_invocation_take_error(invocation, error);
		return;
	}

	if (g_strcmp0 (method_name, "Release") == 0) {
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "RequestPinCode") == 0) {
		const char *path;

		g_variant_get (parameters, "(&o)", &path);
		bluetooth_agent_request_pincode (agent, path, invocation);
	} else if (g_strcmp0 (method_name, "RequestPasskey") == 0) {
		const char *path;

		g_variant_get (parameters, "(&o)", &path);
		bluetooth_agent_request_passkey (agent, path, invocation);
	} else if (g_strcmp0 (method_name, "DisplayPasskey") == 0) {
		const char *path;
		guint32 passkey;
		guint16 entered;

		g_variant_get (parameters, "(&ouq)", &path, &passkey, &entered);
		bluetooth_agent_display_passkey (agent, path, passkey, entered, invocation);
	} else if (g_strcmp0 (method_name, "DisplayPinCode") == 0) {
		const char *path;
		const char *pincode;

		g_variant_get (parameters, "(&o&s)", &path, &pincode);
		bluetooth_agent_display_pincode (agent, path, pincode, invocation);
	} else if (g_strcmp0 (method_name, "RequestConfirmation") == 0) {
		const char *path;
		guint32 passkey;

		g_variant_get (parameters, "(&ou)", &path, &passkey);
		bluetooth_agent_request_confirmation (agent, path, passkey, invocation);
	} else if (g_strcmp0 (method_name, "RequestAuthorization") == 0) {
		const char *path;

		g_variant_get (parameters, "(&o)", &path);
		bluetooth_agent_request_authorization (agent, path, invocation);
	} else if (g_strcmp0 (method_name, "AuthorizeService") == 0) {
		const char *path;
		const char *uuid;

		g_variant_get (parameters, "(&o&s)", &path, &uuid);
		bluetooth_agent_authorize_service (agent, path, uuid, invocation);
	} else if (g_strcmp0 (method_name, "Cancel") == 0) {
		bluetooth_agent_cancel (agent, invocation);
	}
}

static const GDBusInterfaceVTable interface_vtable =
{
	handle_method_call,
	NULL, /* GetProperty */
	NULL, /* SetProperty */
};

gboolean bluetooth_agent_register(BluetoothAgent *agent)
{
	BluetoothAgentPrivate *priv;
	GError *error = NULL;

	g_return_val_if_fail (BLUETOOTH_IS_AGENT (agent), FALSE);

	priv = BLUETOOTH_AGENT_GET_PRIVATE (agent);

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

	if (priv->agent_manager != NULL)
		register_agent (priv);

	return TRUE;
}

static gboolean
error_matches_remote_error (GError     *error,
			    const char *remote_error)
{
	char *str;
	gboolean ret;

	if (error == NULL)
		return FALSE;

	str = g_dbus_error_get_remote_error (error);
	ret = (g_strcmp0 (str, remote_error) == 0);
	g_free (str);

	return ret;
}

gboolean bluetooth_agent_unregister(BluetoothAgent *agent)
{
	BluetoothAgentPrivate *priv;
	GError *error = NULL;

	g_return_val_if_fail (BLUETOOTH_IS_AGENT (agent), FALSE);

	priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	if (priv->agent_manager == NULL)
		return FALSE;

	if (agent_manager1_call_unregister_agent_sync (priv->agent_manager,
						       priv->path,
						       NULL, &error) == FALSE) {
		/* Ignore errors if the adapter is gone */
		if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD) == FALSE &&
		    error_matches_remote_error (error, "org.bluez.Error.DoesNotExist") == FALSE) {
			g_printerr ("Agent unregistration failed: %s '%s'\n",
				    error->message,
				    g_quark_to_string (error->domain));
		}
		g_error_free(error);
	}

	g_object_unref(priv->agent_manager);
	priv->agent_manager = NULL;

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

void bluetooth_agent_set_display_pincode_func(BluetoothAgent *agent,
				BluetoothAgentDisplayPinCodeFunc func, gpointer data)
{
	BluetoothAgentPrivate *priv;

	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	priv->display_pincode_func = func;
	priv->display_pincode_data = data;
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

void bluetooth_agent_set_authorize_service_func(BluetoothAgent *agent,
				BluetoothAgentAuthorizeServiceFunc func, gpointer data)
{
	BluetoothAgentPrivate *priv;

	g_return_if_fail (BLUETOOTH_IS_AGENT (agent));

	priv = BLUETOOTH_AGENT_GET_PRIVATE(agent);

	priv->authorize_service_func = func;
	priv->authorize_service_data = data;
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

