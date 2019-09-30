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

#include "obex-agent.h"

#ifdef DEBUG
#define DBG(fmt, arg...) printf("%s:%s() " fmt "\n", __FILE__, __FUNCTION__ , ## arg)
#else
#define DBG(fmt...)
#endif

#define OBEX_SERVICE	"org.openobex.client"

#define OBEX_CLIENT_PATH	"/"
#define OBEX_CLIENT_INTERFACE	"org.openobex.Client"
#define OBEX_TRANSFER_INTERFACE	"org.openobex.Transfer"

static const gchar introspection_xml[] =
"<node name='%s'>"
"  <interface name='org.openobex.Agent'>"
"    <method name='Request'>"
"      <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"      <arg type='o' name='transfer' direction='in'/>"
"      <arg type='s' name='name' direction='out'/>"
"    </method>"
""
"    <method name='Progress'>"
"      <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"      <arg type='o' name='transfer' direction='in'/>"
"      <arg type='t' name='transferred' direction='in'/>"
"    </method>"
""
"    <method name='Complete'>"
"      <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"      <arg type='o' name='transfer' direction='in'/>"
"    </method>"
""
"    <method name='Release'>"
"      <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"    </method>"
""
"    <method name='Error'>"
"      <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"      <arg type='o' name='transfer' direction='in'/>"
"      <arg type='s' name='message' direction='in'/>"
"    </method>"
"  </interface>"
"</node>";

#define OBEX_AGENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
					OBEX_TYPE_AGENT, ObexAgentPrivate))

typedef struct _ObexAgentPrivate ObexAgentPrivate;

struct _ObexAgentPrivate {
	GDBusConnection *conn;
	gchar *busname;
	gchar *path;
	GDBusNodeInfo *introspection_data;
	guint reg_id;
	guint watch_id;

	ObexAgentReleaseFunc release_func;
	gpointer release_data;

	ObexAgentRequestFunc request_func;
	gpointer request_data;

	ObexAgentProgressFunc progress_func;
	gpointer progress_data;

	ObexAgentCompleteFunc complete_func;
	gpointer complete_data;

	ObexAgentErrorFunc error_func;
	gpointer error_data;
};

G_DEFINE_TYPE(ObexAgent, obex_agent, G_TYPE_OBJECT)

static GDBusProxy *
get_proxy_from_path (ObexAgent *agent,
		     const char *path)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE (agent);

	return g_dbus_proxy_new_sync (priv->conn,
				      G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
				      NULL,
				      OBEX_SERVICE,
				      path,
				      OBEX_TRANSFER_INTERFACE,
				      NULL,
				      NULL);

}

static gboolean obex_agent_request(ObexAgent *agent, const char *path,
						GDBusMethodInvocation *invocation)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	DBG("agent %p", agent);

	if (priv->request_func) {
		GDBusProxy *proxy;

		proxy = get_proxy_from_path (agent, path);

		priv->request_func (invocation, proxy, priv->request_data);

		g_object_unref(proxy);
	} else {
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", ""));
	}

	return TRUE;
}

static gboolean obex_agent_progress(ObexAgent *agent, const char *path,
			guint64 transferred, GDBusMethodInvocation *invocation)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);
	gboolean result = FALSE;

	DBG("agent %p", agent);

	if (priv->progress_func) {
		GDBusProxy *proxy;

		proxy = get_proxy_from_path (agent, path);

		result = priv->progress_func(invocation, proxy, transferred,
							priv->progress_data);

		g_object_unref(proxy);
	} else {
		g_dbus_method_invocation_return_value (invocation, NULL);
	}

	return result;
}

static gboolean obex_agent_complete(ObexAgent *agent, const char *path,
						GDBusMethodInvocation *invocation)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);
	gboolean result = FALSE;

	DBG("agent %p", agent);

	if (priv->complete_func) {
		GDBusProxy *proxy;

		proxy = get_proxy_from_path (agent, path);

		result = priv->complete_func(invocation, proxy,
						priv->complete_data);

		g_object_unref(proxy);
	} else {
		g_dbus_method_invocation_return_value (invocation, NULL);
	}

	return result;
}

static gboolean obex_agent_release(ObexAgent *agent,
						GDBusMethodInvocation *invocation)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);
	gboolean result = FALSE;

	DBG("agent %p", agent);

	if (priv->release_func)
		result = priv->release_func(invocation, priv->release_data);
	else
		g_dbus_method_invocation_return_value (invocation, NULL);

	return result;
}

static gboolean obex_agent_error(ObexAgent *agent,
				 const char *path,
				 const char *message,
				 GDBusMethodInvocation *invocation)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);
	gboolean result = FALSE;

	DBG("agent %p", agent);

	if (priv->error_func) {
		GDBusProxy *proxy;

		proxy = get_proxy_from_path (agent, path);

		result = priv->error_func(invocation, proxy, message,
							priv->progress_data);

		g_object_unref(proxy);
	} else {
		g_dbus_method_invocation_return_value (invocation, NULL);
	}

	return result;
}

static void
name_appeared_cb (GDBusConnection *connection,
		  const gchar     *name,
		  const gchar     *name_owner,
		  ObexAgent       *agent)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	g_free (priv->busname);
	priv->busname = g_strdup (name_owner);
}

static void
name_vanished_cb (GDBusConnection *connection,
		  const gchar     *name,
		  ObexAgent       *agent)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	g_free (priv->busname);
	priv->busname = NULL;
}

static void obex_agent_init(ObexAgent *agent)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	DBG("agent %p", agent);

	priv->conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
	priv->watch_id = g_bus_watch_name_on_connection (priv->conn,
							 OBEX_SERVICE,
							 G_BUS_NAME_WATCHER_FLAGS_NONE,
							 (GBusNameAppearedCallback) name_appeared_cb,
							 (GBusNameVanishedCallback) name_vanished_cb,
							 agent,
							 NULL);
}

static void obex_agent_finalize(GObject *agent)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	DBG("agent %p", agent);

	g_dbus_connection_unregister_object (priv->conn, priv->reg_id);
	g_bus_unwatch_name (priv->watch_id);
	g_free (priv->busname);
	g_free(priv->path);
	g_dbus_node_info_unref (priv->introspection_data);
	g_object_unref (priv->conn);

	G_OBJECT_CLASS(obex_agent_parent_class)->finalize(agent);
}

static void obex_agent_class_init(ObexAgentClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	DBG("class %p", klass);

	g_type_class_add_private(klass, sizeof(ObexAgentPrivate));

	object_class->finalize = obex_agent_finalize;
}

ObexAgent *obex_agent_new(void)
{
	ObexAgent *agent;

	agent = OBEX_AGENT(g_object_new(OBEX_TYPE_AGENT, NULL));

	DBG("agent %p", agent);

	return agent;
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
	ObexAgent *agent = (ObexAgent *) user_data;
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	if (g_str_equal (sender, priv->busname) == FALSE) {
		g_assert_not_reached ();
		/* FIXME, should this just be a D-Bus Error instead? */
	}

	if (g_strcmp0 (method_name, "Request") == 0) {
		char *path;
		g_variant_get (parameters, "(o)", &path);
		obex_agent_request (agent, path, invocation);
		g_free (path);
	} else if (g_strcmp0 (method_name, "Progress") == 0) {
		char *path;
		guint64 transferred;
		g_variant_get (parameters, "(ot)", &path, &transferred);
		obex_agent_progress (agent, path, transferred, invocation);
		g_free (path);
	} else if (g_strcmp0 (method_name, "Complete") == 0) {
		char *path;
		g_variant_get (parameters, "(o)", &path);
		obex_agent_complete (agent, path, invocation);
		g_free (path);
	} else if (g_strcmp0 (method_name, "Error") == 0) {
		char *path, *message;
		g_variant_get (parameters, "(os)", &path, &message);
		obex_agent_error (agent, path, message, invocation);
		g_free (path);
		g_free (message);
	} else if (g_strcmp0 (method_name, "Release") == 0) {
		obex_agent_release (agent, invocation);
	}
}

static const GDBusInterfaceVTable interface_vtable =
{
	handle_method_call,
	NULL, /* GetProperty */
	NULL, /* SetProperty */
};

gboolean obex_agent_setup(ObexAgent *agent, const char *path)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);
	GError *error = NULL;
	char *xml;

	DBG("agent %p path %s", agent, path);

	if (priv->path != NULL) {
		g_warning ("Obex Agent already setup on '%s'", priv->path);
		return FALSE;
	}

	priv->path = g_strdup (path);

	xml = g_strdup_printf (introspection_xml, path);
	priv->introspection_data = g_dbus_node_info_new_for_xml (xml, NULL);
	g_free (xml);
	g_assert (priv->introspection_data);

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

void obex_agent_set_release_func(ObexAgent *agent,
				ObexAgentReleaseFunc func, gpointer data)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	DBG("agent %p", agent);

	priv->release_func = func;
	priv->release_data = data;
}

void obex_agent_set_request_func(ObexAgent *agent,
				ObexAgentRequestFunc func, gpointer data)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	DBG("agent %p", agent);

	priv->request_func = func;
	priv->request_data = data;
}

void obex_agent_set_progress_func(ObexAgent *agent,
				ObexAgentProgressFunc func, gpointer data)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	DBG("agent %p", agent);

	priv->progress_func = func;
	priv->progress_data = data;
}

void obex_agent_set_complete_func(ObexAgent *agent,
				ObexAgentCompleteFunc func, gpointer data)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	DBG("agent %p", agent);

	priv->complete_func = func;
	priv->complete_data = data;
}

void obex_agent_set_error_func(ObexAgent *agent,
			       ObexAgentErrorFunc func, gpointer data)
{
	ObexAgentPrivate *priv = OBEX_AGENT_GET_PRIVATE(agent);

	DBG("agent %p", agent);

	priv->error_func = func;
	priv->error_data = data;
}
