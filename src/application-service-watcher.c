/*
An object implementing the NotificationWatcher interface and passes
the information into the app-store.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib-bindings.h>
#include "application-service-watcher.h"
#include "dbus-shared.h"

/* Enum for the properties so that they can be quickly
   found and looked up. */
enum {
	PROP_0,
	PROP_PROTOCOL_VERSION,
	PROP_IS_STATUS_NOTIFIER_HOST_REGISTERED,
	PROP_REGISTERED_STATUS_NOTIFIER_ITEMS
};

/* The strings so that they can be slowly looked up. */
#define PROP_PROTOCOL_VERSION_S                   "protocol-version"
#define PROP_IS_STATUS_NOTIFIER_HOST_REGISTERED_S "is-status-notifier-host-registered"
#define PROP_REGISTERED_STATUS_NOTIFIER_ITEMS_S   "registered-status-notifier-items"

#define CURRENT_PROTOCOL_VERSION 0

static gboolean _notification_watcher_server_register_status_notifier_item (ApplicationServiceWatcher * appwatcher, const gchar * service, DBusGMethodInvocation * method);
static gboolean _notification_watcher_server_register_status_notifier_host (ApplicationServiceWatcher * appwatcher, const gchar * host);
static void get_name_cb (DBusGProxy * proxy, guint status, GError * error, gpointer data);

#include "notification-watcher-server.h"

/* Private Stuff */
typedef struct _ApplicationServiceWatcherPrivate ApplicationServiceWatcherPrivate;
struct _ApplicationServiceWatcherPrivate {
	ApplicationServiceAppstore * appstore;
	DBusGProxy * dbus_proxy;
};

#define APPLICATION_SERVICE_WATCHER_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), APPLICATION_SERVICE_WATCHER_TYPE, ApplicationServiceWatcherPrivate))

/* Signals Stuff */
enum {
	STATUS_NOTIFIER_ITEM_REGISTERED,
	STATUS_NOTIFIER_ITEM_UNREGISTERED,
	STATUS_NOTIFIER_HOST_REGISTERED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* GObject stuff */
static void application_service_watcher_class_init (ApplicationServiceWatcherClass *klass);
static void application_service_watcher_init       (ApplicationServiceWatcher *self);
static void application_service_watcher_dispose    (GObject *object);
static void application_service_watcher_finalize   (GObject *object);
static void application_service_watcher_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void application_service_watcher_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

G_DEFINE_TYPE (ApplicationServiceWatcher, application_service_watcher, G_TYPE_OBJECT);

static void
application_service_watcher_class_init (ApplicationServiceWatcherClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ApplicationServiceWatcherPrivate));

	object_class->dispose = application_service_watcher_dispose;
	object_class->finalize = application_service_watcher_finalize;

	/* Property funcs */
	object_class->set_property = application_service_watcher_set_property;
	object_class->get_property = application_service_watcher_get_property;

	/* Properties */
	g_object_class_install_property (object_class,
	                                 PROP_PROTOCOL_VERSION,
	                                 g_param_spec_int(PROP_PROTOCOL_VERSION_S,
	                                                  "Protocol Version",
	                                                  "Which version of the StatusNotifierProtocol this watcher implements",
	                                                  0, G_MAXINT,
	                                                  CURRENT_PROTOCOL_VERSION,
	                                                  G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
	                                 PROP_IS_STATUS_NOTIFIER_HOST_REGISTERED,
	                                 g_param_spec_boolean(PROP_IS_STATUS_NOTIFIER_HOST_REGISTERED_S,
	                                                      "Is StatusNotifierHost Registered",
	                                                      "True if there is at least one StatusNotifierHost registered",
	                                                      FALSE,
	                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
	                                 PROP_REGISTERED_STATUS_NOTIFIER_ITEMS,
	                                 g_param_spec_boxed(PROP_REGISTERED_STATUS_NOTIFIER_ITEMS_S,
	                                                    "Registered StatusNotifierItems",
	                                                    "The list of StatusNotifierItems registered to this watcher",
	                                                    G_TYPE_STRV,
	                                                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	/* Signals */
	signals[STATUS_NOTIFIER_ITEM_REGISTERED] = g_signal_new ("status-notifier-item-registered",
	                                           G_TYPE_FROM_CLASS(klass),
	                                           G_SIGNAL_RUN_LAST,
	                                           G_STRUCT_OFFSET (ApplicationServiceWatcherClass, status_notifier_item_registered),
	                                           NULL, NULL,
	                                           g_cclosure_marshal_VOID__STRING,
	                                           G_TYPE_NONE, 1, G_TYPE_STRING, G_TYPE_NONE);
	signals[STATUS_NOTIFIER_ITEM_UNREGISTERED] = g_signal_new ("status-notifier-item-unregistered",
	                                           G_TYPE_FROM_CLASS(klass),
	                                           G_SIGNAL_RUN_LAST,
	                                           G_STRUCT_OFFSET (ApplicationServiceWatcherClass, status_notifier_item_unregistered),
	                                           NULL, NULL,
	                                           g_cclosure_marshal_VOID__STRING,
	                                           G_TYPE_NONE, 1, G_TYPE_STRING, G_TYPE_NONE);
	signals[STATUS_NOTIFIER_HOST_REGISTERED] = g_signal_new ("status-notifier-host-registered",
	                                           G_TYPE_FROM_CLASS(klass),
	                                           G_SIGNAL_RUN_LAST,
	                                           G_STRUCT_OFFSET (ApplicationServiceWatcherClass, status_notifier_host_registered),
	                                           NULL, NULL,
	                                           g_cclosure_marshal_VOID__VOID,
	                                           G_TYPE_NONE, 0, G_TYPE_NONE);

	dbus_g_object_type_install_info(APPLICATION_SERVICE_WATCHER_TYPE,
	                                &dbus_glib__notification_watcher_server_object_info);

	return;
}

static void
application_service_watcher_init (ApplicationServiceWatcher *self)
{
	ApplicationServiceWatcherPrivate * priv = APPLICATION_SERVICE_WATCHER_GET_PRIVATE(self);

	priv->appstore = NULL;

	GError * error = NULL;
	DBusGConnection * session_bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (error != NULL) {
		g_warning("Unable to get session bus: %s", error->message);
		g_error_free(error);
		exit(0);
		return;
	}

	dbus_g_connection_register_g_object(session_bus,
	                                    NOTIFICATION_WATCHER_DBUS_OBJ,
	                                    G_OBJECT(self));

	priv->dbus_proxy = dbus_g_proxy_new_for_name_owner(session_bus,
	                                                   DBUS_SERVICE_DBUS,
	                                                   DBUS_PATH_DBUS,
	                                                   DBUS_INTERFACE_DBUS,
	                                                   &error);
	if (error != NULL) {
		g_error("Ah, can't get proxy to dbus: %s", error->message);
		g_error_free(error);
		return;
	}

	org_freedesktop_DBus_request_name_async(priv->dbus_proxy,
	                                        NOTIFICATION_WATCHER_DBUS_ADDR,
	                                        DBUS_NAME_FLAG_DO_NOT_QUEUE,
	                                        get_name_cb,
	                                        self);

	return;
}

static void
application_service_watcher_dispose (GObject *object)
{
	ApplicationServiceWatcherPrivate * priv = APPLICATION_SERVICE_WATCHER_GET_PRIVATE(object);
	
	if (priv->appstore != NULL) {
		g_object_unref(G_OBJECT(priv->appstore));
		priv->appstore = NULL;
	}

	G_OBJECT_CLASS (application_service_watcher_parent_class)->dispose (object);
	return;
}

static void
application_service_watcher_finalize (GObject *object)
{

	G_OBJECT_CLASS (application_service_watcher_parent_class)->finalize (object);
	return;
}

static void
application_service_watcher_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
	/* There are no writable properties for now */
}

static void
application_service_watcher_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
	ApplicationServiceWatcherPrivate * priv = APPLICATION_SERVICE_WATCHER_GET_PRIVATE(object);
	switch (prop_id) {
	case PROP_PROTOCOL_VERSION:
		g_value_set_int (value, CURRENT_PROTOCOL_VERSION);
		break;
	case PROP_IS_STATUS_NOTIFIER_HOST_REGISTERED:
		g_value_set_boolean (value, TRUE);
		break;
	case PROP_REGISTERED_STATUS_NOTIFIER_ITEMS:
		g_value_set_boxed (value, application_service_appstore_application_get_list(priv->appstore));
		break;
	}
}

ApplicationServiceWatcher *
application_service_watcher_new (ApplicationServiceAppstore * appstore)
{
	GObject * obj = g_object_new(APPLICATION_SERVICE_WATCHER_TYPE, NULL);
	ApplicationServiceWatcherPrivate * priv = APPLICATION_SERVICE_WATCHER_GET_PRIVATE(obj);
	priv->appstore = appstore;
	g_object_ref(G_OBJECT(priv->appstore));
	return APPLICATION_SERVICE_WATCHER(obj);
}

static gboolean
_notification_watcher_server_register_status_notifier_item (ApplicationServiceWatcher * appwatcher, const gchar * service, DBusGMethodInvocation * method)
{
	ApplicationServiceWatcherPrivate * priv = APPLICATION_SERVICE_WATCHER_GET_PRIVATE(appwatcher);

	if (service[0] == '/') {
		char * sender = dbus_g_method_get_sender(method);
		application_service_appstore_application_add(priv->appstore,
		                                             sender,
		                                             service);
		g_free(sender);
	} else {
		application_service_appstore_application_add(priv->appstore,
		                                             service,
		                                             NOTIFICATION_ITEM_DEFAULT_OBJ);
	}

	dbus_g_method_return(method, G_TYPE_NONE);
	return TRUE;
}

static gboolean
_notification_watcher_server_register_status_notifier_host (ApplicationServiceWatcher * appwatcher, const gchar * host)
{

	return FALSE;
}

/* Function to handle the return of the get name.  There isn't a whole
   lot that can be done, but we're atleast going to tell people. */
static void
get_name_cb (DBusGProxy * proxy, guint status, GError * error, gpointer data)
{
	if (error != NULL) {
		g_warning("Unable to get watcher name '%s' because: %s", NOTIFICATION_WATCHER_DBUS_ADDR, error->message);
		return;
	}

	if (status != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER &&
			status != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER) {
		g_warning("Unable to get watcher name '%s'", NOTIFICATION_WATCHER_DBUS_ADDR);
		return;
	}

	/* After we've got the name we can request upstart to trigger
	   the jobs of any application indicators that need to start
	   at desktop init time. */

	GError * spawn_error = NULL;
	gchar * argv[] = {
	          "initctl",
	          "--session",
	          "--user",
	          "emit",
	          "--no-wait",
	          "appindicators-start",
	          NULL,
	          };

	g_spawn_async(NULL, /* Working Directory */
	              argv,
	              NULL, /* environment */
	              G_SPAWN_SEARCH_PATH,
	              NULL, NULL, /* child setup function */
	              NULL, /* Pid */
	              &spawn_error);

	if (spawn_error != NULL) {
		/* NOTE: When we get to the point where we can start
		   assuming upstart user sessions this can be escillated
		   to a warning or higher */
		g_debug("Unable to signal appindicators-start to upstart: %s", spawn_error->message);
		g_error_free(spawn_error);
	}

	return;
}
