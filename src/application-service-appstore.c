/*
An object that stores the registration of all the application
indicators.  It also communicates this to the indicator visualization.

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

#include <libindicator/indicator-object.h>
#include "libappindicator/app-indicator.h"
#include "libappindicator/app-indicator-enum-types.h"
#include "application-service-appstore.h"
#include "application-service-marshal.h"
#include "dbus-shared.h"
#include "generate-id.h"

/* DBus Prototypes */
static GVariant * get_applications (ApplicationServiceAppstore * appstore);
static void bus_method_call (GDBusConnection * connection, const gchar * sender, const gchar * path, const gchar * interface, const gchar * method, GVariant * params, GDBusMethodInvocation * invocation, gpointer user_data);
static void props_cb (GObject * object, GAsyncResult * res, gpointer user_data);

#include "gen-application-service.xml.h"

#define NOTIFICATION_ITEM_PROP_ID                    "Id"
#define NOTIFICATION_ITEM_PROP_CATEGORY              "Category"
#define NOTIFICATION_ITEM_PROP_STATUS                "Status"
#define NOTIFICATION_ITEM_PROP_ICON_NAME             "IconName"
#define NOTIFICATION_ITEM_PROP_ICON_DESC             "IconAccessibleDesc"
#define NOTIFICATION_ITEM_PROP_AICON_NAME            "AttentionIconName"
#define NOTIFICATION_ITEM_PROP_AICON_DESC            "AttentionAccessibleDesc"
#define NOTIFICATION_ITEM_PROP_ICON_THEME_PATH       "IconThemePath"
#define NOTIFICATION_ITEM_PROP_MENU                  "Menu"
#define NOTIFICATION_ITEM_PROP_LABEL                 "XAyatanaLabel"
#define NOTIFICATION_ITEM_PROP_LABEL_GUIDE           "XAyatanaLabelGuide"
#define NOTIFICATION_ITEM_PROP_TITLE                 "Title"
#define NOTIFICATION_ITEM_PROP_ORDERING_INDEX        "XAyatanaOrderingIndex"

#define NOTIFICATION_ITEM_SIG_NEW_ICON               "NewIcon"
#define NOTIFICATION_ITEM_SIG_NEW_AICON              "NewAttentionIcon"
#define NOTIFICATION_ITEM_SIG_NEW_STATUS             "NewStatus"
#define NOTIFICATION_ITEM_SIG_NEW_LABEL              "XAyatanaNewLabel"
#define NOTIFICATION_ITEM_SIG_NEW_ICON_THEME_PATH    "NewIconThemePath"
#define NOTIFICATION_ITEM_SIG_NEW_TITLE              "NewTitle"

#define OVERRIDE_GROUP_NAME                          "Ordering Index Overrides"
#define OVERRIDE_FILE_NAME                           "ordering-override.keyfile"

/* Private Stuff */
struct _ApplicationServiceAppstorePrivate {
	GCancellable * bus_cancel;
	GDBusConnection * bus;
	guint dbus_registration;
	GList * applications;
	GHashTable * ordering_overrides;
};

typedef enum {
	VISIBLE_STATE_HIDDEN,
	VISIBLE_STATE_SHOWN
} visible_state_t;

#define STATE2STRING(x)  ((x) == VISIBLE_STATE_HIDDEN ? "hidden" : "visible")

typedef struct _Application Application;
struct _Application {
	gchar * id;
	gchar * category;
	gchar * dbus_name;
	gchar * dbus_object;
	ApplicationServiceAppstore * appstore; /* not ref'd */
	GCancellable * dbus_proxy_cancel;
	GDBusProxy * dbus_proxy;
	GCancellable * props_cancel;
	gboolean queued_props;
	GDBusProxy * props;
	gboolean validated; /* Whether we've gotten all the parameters and they look good. */
	AppIndicatorStatus status;
	gchar * icon;
	gchar * icon_desc;
	gchar * aicon;
	gchar * aicon_desc;
	gchar * menu;
	gchar * icon_theme_path;
	gchar * label;
	gchar * guide;
	gchar * title;
	gboolean currently_free;
	guint ordering_index;
	visible_state_t visible_state;
	guint name_watcher;
};

#define APPLICATION_SERVICE_APPSTORE_GET_PRIVATE(o) \
			(G_TYPE_INSTANCE_GET_PRIVATE ((o), APPLICATION_SERVICE_APPSTORE_TYPE, ApplicationServiceAppstorePrivate))

/* GDBus Stuff */
static GDBusNodeInfo *      node_info = NULL;
static GDBusInterfaceInfo * interface_info = NULL;
static GDBusInterfaceVTable interface_table = {
       method_call:    bus_method_call,
       get_property:   NULL, /* No properties */
       set_property:   NULL  /* No properties */
};

/* GObject stuff */
static void application_service_appstore_class_init (ApplicationServiceAppstoreClass *klass);
static void application_service_appstore_init       (ApplicationServiceAppstore *self);
static void application_service_appstore_dispose    (GObject *object);
static void application_service_appstore_finalize   (GObject *object);
static gint app_sort_func (gconstpointer a, gconstpointer b, gpointer userdata);
static void load_override_file (GHashTable * hash, const gchar * filename);
static AppIndicatorStatus string_to_status(const gchar * status_string);
static void apply_status (Application * app);
static AppIndicatorCategory string_to_cat(const gchar * cat_string);
static Application * find_application (ApplicationServiceAppstore * appstore, const gchar * address, const gchar * object);
static Application * find_application_by_menu (ApplicationServiceAppstore * appstore, const gchar * address, const gchar * menuobject);
static void bus_get_cb (GObject * object, GAsyncResult * res, gpointer user_data);
static void dbus_proxy_cb (GObject * object, GAsyncResult * res, gpointer user_data);
static void app_receive_signal (GDBusProxy * proxy, gchar * sender_name, gchar * signal_name, GVariant * parameters, gpointer user_data);
static void get_all_properties (Application * app);
static void application_free (Application * app);

G_DEFINE_TYPE (ApplicationServiceAppstore, application_service_appstore, G_TYPE_OBJECT);

static void
application_service_appstore_class_init (ApplicationServiceAppstoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (ApplicationServiceAppstorePrivate));

	object_class->dispose = application_service_appstore_dispose;
	object_class->finalize = application_service_appstore_finalize;

	/* Setting up the DBus interfaces */
	if (node_info == NULL) {
		GError * error = NULL;

		node_info = g_dbus_node_info_new_for_xml(_application_service, &error);
		if (error != NULL) {
			g_critical("Unable to parse Application Service Interface description: %s", error->message);
			g_error_free(error);
		}
	}

	if (interface_info == NULL) {
		interface_info = g_dbus_node_info_lookup_interface(node_info, INDICATOR_APPLICATION_DBUS_IFACE);

		if (interface_info == NULL) {
			g_critical("Unable to find interface '" INDICATOR_APPLICATION_DBUS_IFACE "'");
		}
	}

	return;
}

static void
application_service_appstore_init (ApplicationServiceAppstore *self)
{
    
	ApplicationServiceAppstorePrivate * priv = APPLICATION_SERVICE_APPSTORE_GET_PRIVATE (self);

	priv->applications = NULL;
	priv->bus_cancel = NULL;
	priv->dbus_registration = 0;

	priv->ordering_overrides = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	load_override_file(priv->ordering_overrides, DATADIR "/" OVERRIDE_FILE_NAME);
	gchar * userfile = g_build_filename(g_get_user_data_dir(), "indicators", "application", OVERRIDE_FILE_NAME, NULL);
	load_override_file(priv->ordering_overrides, userfile);
	g_free(userfile);

	priv->bus_cancel = g_cancellable_new();
	g_bus_get(G_BUS_TYPE_SESSION,
	          priv->bus_cancel,
	          bus_get_cb,
	          self);

	self->priv = priv;

	return;
}

static void
bus_get_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;
	GDBusConnection * connection = g_bus_get_finish(res, &error);

	if (error != NULL) {
		g_critical("OMG! Unable to get a connection to DBus: %s", error->message);
		g_error_free(error);
		return;
	}

	ApplicationServiceAppstorePrivate * priv = APPLICATION_SERVICE_APPSTORE_GET_PRIVATE (user_data);

	g_warn_if_fail(priv->bus == NULL);
	priv->bus = connection;

	if (priv->bus_cancel != NULL) {
		g_object_unref(priv->bus_cancel);
		priv->bus_cancel = NULL;
	}

	/* Now register our object on our new connection */
	priv->dbus_registration = g_dbus_connection_register_object(priv->bus,
	                                                            INDICATOR_APPLICATION_DBUS_OBJ,
	                                                            interface_info,
	                                                            &interface_table,
	                                                            user_data,
	                                                            NULL,
	                                                            &error);

	if (error != NULL) {
		g_critical("Unable to register the object to DBus: %s", error->message);
		g_error_free(error);
		return;
	}

	return;	
}

/* A method has been called from our dbus inteface.  Figure out what it
   is and dispatch it. */
static void
bus_method_call (GDBusConnection * connection, const gchar * sender,
                 const gchar * path, const gchar * interface,
                 const gchar * method, GVariant * params,
                 GDBusMethodInvocation * invocation, gpointer user_data)
{
	ApplicationServiceAppstore * service = APPLICATION_SERVICE_APPSTORE(user_data);
	GVariant * retval = NULL;
	Application *app = NULL;
	gchar *dbusaddress = NULL;
	gchar *dbusmenuobject = NULL;

	if (g_strcmp0(method, "GetApplications") == 0) {
		retval = get_applications(service);
	} else if (g_strcmp0(method, "ApplicationScrollEvent") == 0) {
		gchar *orientation = NULL;
		gint delta;
		guint direction;

		g_variant_get (params, "(ssiu)", &dbusaddress, &dbusmenuobject,
		                                   &delta, &direction);

		switch (direction) {
			case INDICATOR_OBJECT_SCROLL_UP:
				delta = -delta;
				orientation = "vertical";
				break;
			case INDICATOR_OBJECT_SCROLL_DOWN:
				/* delta unchanged */
				orientation = "vertical";
				break;
			case INDICATOR_OBJECT_SCROLL_LEFT:
				delta = -delta;
				orientation = "horizontal";
				break;
			case INDICATOR_OBJECT_SCROLL_RIGHT:
				/* delta unchanged */
				orientation = "horizontal";
				break;
			default:
				g_assert_not_reached();
				break;
		}

		app = find_application_by_menu(service, dbusaddress, dbusmenuobject);

		if (app != NULL && app->dbus_proxy != NULL && orientation != NULL) {
			g_dbus_proxy_call(app->dbus_proxy, "Scroll",
			                  g_variant_new("(is)", delta, orientation),
			                  G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
		}
	} else if (g_strcmp0(method, "ApplicationSecondaryActivateEvent") == 0) {
		guint time;

		g_variant_get (params, "(ssu)", &dbusaddress, &dbusmenuobject, &time);
		app = find_application_by_menu(service, dbusaddress, dbusmenuobject);

		if (app != NULL && app->dbus_proxy != NULL) {
			g_dbus_proxy_call(app->dbus_proxy, "XAyatanaSecondaryActivate",
			                  g_variant_new("(u)", time),
			                  G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
		}
	} else {
		g_warning("Calling method '%s' on the indicator service and it's unknown", method);
	}

	g_free(dbusaddress);
	g_free(dbusmenuobject);

	g_dbus_method_invocation_return_value(invocation, retval);
	return;
}

static void
application_service_appstore_dispose (GObject *object)
{
	ApplicationServiceAppstorePrivate * priv = APPLICATION_SERVICE_APPSTORE(object)->priv;

	while (priv->applications != NULL) {
		application_service_appstore_application_remove(APPLICATION_SERVICE_APPSTORE(object),
		                                           ((Application *)priv->applications->data)->dbus_name,
		                                           ((Application *)priv->applications->data)->dbus_object);
	}

	if (priv->dbus_registration != 0) {
		g_dbus_connection_unregister_object(priv->bus, priv->dbus_registration);
		/* Don't care if it fails, there's nothing we can do */
		priv->dbus_registration = 0;
	}

	if (priv->bus != NULL) {
		g_object_unref(priv->bus);
		priv->bus = NULL;
	}

	if (priv->bus_cancel != NULL) {
		g_cancellable_cancel(priv->bus_cancel);
		g_object_unref(priv->bus_cancel);
		priv->bus_cancel = NULL;
	}

	G_OBJECT_CLASS (application_service_appstore_parent_class)->dispose (object);
	return;
}

static void
application_service_appstore_finalize (GObject *object)
{
	ApplicationServiceAppstorePrivate * priv = APPLICATION_SERVICE_APPSTORE(object)->priv;

	if (priv->ordering_overrides != NULL) {
		g_hash_table_destroy(priv->ordering_overrides);
		priv->ordering_overrides = NULL;
	}

	G_OBJECT_CLASS (application_service_appstore_parent_class)->finalize (object);
	return;
}

/* Loads the file and adds the override entries to the table
   of overrides */
static void
load_override_file (GHashTable * hash, const gchar * filename)
{
	g_return_if_fail(hash != NULL);
	g_return_if_fail(filename != NULL);

	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		g_debug("Override file '%s' doesn't exist", filename);
		return;
	}

	g_debug("Loading overrides from: '%s'", filename);

	GError * error = NULL;
	GKeyFile * keyfile = g_key_file_new();
	g_key_file_load_from_file(keyfile, filename, G_KEY_FILE_NONE, &error);

	if (error != NULL) {
		g_warning("Unable to load keyfile '%s' because: %s", filename, error->message);
		g_error_free(error);
		g_key_file_free(keyfile);
		return;
	}

	gchar ** keys = g_key_file_get_keys(keyfile, OVERRIDE_GROUP_NAME, NULL, &error);
	if (error != NULL) {
		g_warning("Unable to get keys from keyfile '%s' because: %s", filename, error->message);
		g_error_free(error);
		g_key_file_free(keyfile);
		return;
	}

	gchar * key;
	gint i;

	for (i = 0; (key = keys[i]) != NULL; i++) {
		GError * valerror = NULL;
		gint val = g_key_file_get_integer(keyfile, OVERRIDE_GROUP_NAME, key, &valerror);

		if (valerror != NULL) {
			g_warning("Unable to get key '%s' out of file '%s' because: %s", key, filename, valerror->message);
			g_error_free(valerror);
			continue;
		}
		g_debug("%s: override '%s' with value '%d'", filename, key, val);

		g_hash_table_insert(hash, g_strdup(key), GINT_TO_POINTER(val));
	}
	g_strfreev(keys);
	g_key_file_free(keyfile);

	return;
}

/* Return from getting the properties from the item.  We're looking at those
   and making sure we have everything that we need.  If we do, then we'll
   move on up to sending this onto the indicator. */
static void
got_all_properties (GObject * source_object, GAsyncResult * res,
                    gpointer user_data)
{
	Application * app = (Application *)user_data;
	g_return_if_fail(app != NULL);

	GError * error = NULL;
	GVariant * menu = NULL, * id = NULL, * category = NULL,
	         * status = NULL, * icon_name = NULL, * aicon_name = NULL,
	         * icon_desc = NULL, * aicon_desc = NULL,
	         * icon_theme_path = NULL, * index = NULL, * label = NULL,
	         * guide = NULL, * title = NULL;

	GVariant * properties = g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object), res, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return; // Must exit before accessing freed memory
	}

	if (app->props_cancel != NULL) {
		g_object_unref(app->props_cancel);
		app->props_cancel = NULL;
	}

	if (error != NULL) {
		g_critical("Could not grab DBus properties for %s: %s", app->dbus_name, error->message);
		g_error_free(error);
		if (!app->validated)
			application_free(app);
		return;
	}

	ApplicationServiceAppstorePrivate * priv = app->appstore->priv;

	/* Grab all properties from variant */
	GVariantIter * iter = NULL;
	const gchar * name = NULL;
	GVariant * value = NULL;
	g_variant_get(properties, "(a{sv})", &iter);
	while (g_variant_iter_loop (iter, "{&sv}", &name, &value)) {
		if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_MENU) == 0) {
			menu = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_ID) == 0) {
			id = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_CATEGORY) == 0) {
			category = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_STATUS) == 0) {
			status = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_ICON_NAME) == 0) {
			icon_name = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_ICON_DESC) == 0) {
			icon_desc = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_AICON_NAME) == 0) {
			aicon_name = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_AICON_DESC) == 0) {
			aicon_desc = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_ICON_THEME_PATH) == 0) {
			icon_theme_path = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_ORDERING_INDEX) == 0) {
			index = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_LABEL) == 0) {
			label = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_LABEL_GUIDE) == 0) {
			guide = g_variant_ref(value);
		} else if (g_strcmp0(name, NOTIFICATION_ITEM_PROP_TITLE) == 0) {
			title = g_variant_ref(value);
		} /* else ignore */
	}
	g_variant_iter_free (iter);
	g_variant_unref(properties);

	if (menu == NULL || id == NULL || category == NULL || status == NULL ||
	    icon_name == NULL) {
		g_warning("Notification Item on object %s of %s doesn't have enough properties.", app->dbus_object, app->dbus_name);
		if (!app->validated)
			application_free(app);
	}
	else {
		app->validated = TRUE;

		/* It is possible we're coming through a second time and
		   getting the properties.  So we need to ensure we don't
		   already have them stored */
		g_free(app->id);
		g_free(app->category);
		g_free(app->icon);
		g_free(app->menu);

		app->id = g_variant_dup_string(id, NULL);
		app->category = g_variant_dup_string(category, NULL);
		app->status = string_to_status(g_variant_get_string(status, NULL));
		app->icon = g_variant_dup_string(icon_name, NULL);
		app->menu = g_variant_dup_string(menu, NULL);

		/* Now the optional properties */

		g_free(app->icon_desc);
		if (icon_desc != NULL) {
			app->icon_desc = g_variant_dup_string(icon_desc, NULL);
		}
		else {
			app->icon_desc = g_strdup("");
		}

		g_free(app->aicon);
		if (aicon_name != NULL) {
			app->aicon = g_variant_dup_string(aicon_name, NULL);
		} else {
			app->aicon = g_strdup("");
		}

		g_free(app->aicon_desc);
		if (aicon_desc != NULL) {
			app->aicon_desc = g_variant_dup_string(aicon_desc, NULL);
		}
		else {
			app->aicon_desc = g_strdup("");
		}

		g_free(app->icon_theme_path);
		if (icon_theme_path != NULL) {
			app->icon_theme_path = g_variant_dup_string(icon_theme_path, NULL);
		} else {
			app->icon_theme_path = g_strdup("");
		}

		gpointer ordering_index_over = g_hash_table_lookup(priv->ordering_overrides, app->id);
		if (ordering_index_over == NULL) {
			if (index == NULL || g_variant_get_uint32(index) == 0) {
				app->ordering_index = generate_id(string_to_cat(app->category), app->id);
			} else {
				app->ordering_index = g_variant_get_uint32(index);
			}
		} else {
			app->ordering_index = GPOINTER_TO_UINT(ordering_index_over);
		}
		g_debug("'%s' ordering index is '%X'", app->id, app->ordering_index);
		app->appstore->priv->applications = g_list_sort_with_data(app->appstore->priv->applications, app_sort_func, NULL);

		g_free(app->label);
		if (label != NULL) {
			app->label = g_variant_dup_string(label, NULL);
		} else {
			app->label = g_strdup("");
		}

		g_free(app->guide);
		if (guide != NULL) {
			app->guide = g_variant_dup_string(guide, NULL);
		} else {
			app->guide = g_strdup("");
		}

		g_free(app->title);
		if (title != NULL) {
			app->title = g_variant_dup_string(title, NULL);
		} else {
			app->title = g_strdup("");
		}

		apply_status(app);

		if (app->queued_props) {
			get_all_properties(app);
			app->queued_props = FALSE;
		}
	}

	if (menu)            g_variant_unref (menu);
	if (id)              g_variant_unref (id);
	if (category)        g_variant_unref (category);
	if (status)          g_variant_unref (status);
	if (icon_name)       g_variant_unref (icon_name);
	if (icon_desc)       g_variant_unref (icon_desc);
	if (aicon_name)      g_variant_unref (aicon_name);
	if (aicon_desc)      g_variant_unref (aicon_desc);
	if (icon_theme_path) g_variant_unref (icon_theme_path);
	if (index)           g_variant_unref (index);
	if (label)           g_variant_unref (label);
	if (guide)           g_variant_unref (guide);
	if (title)           g_variant_unref (title);

	return;
}

static void
get_all_properties (Application * app)
{
	if (app->props != NULL && app->props_cancel == NULL) {
		app->props_cancel = g_cancellable_new();
		g_dbus_proxy_call(app->props, "GetAll",
		                  g_variant_new("(s)", NOTIFICATION_ITEM_DBUS_IFACE),
		                  G_DBUS_CALL_FLAGS_NONE, -1, app->props_cancel,
		                  got_all_properties, app);
	}
	else {
		g_debug("Queuing a properties check");
		app->queued_props = TRUE;
	}
}

/* Simple translation function -- could be optimized */
static AppIndicatorStatus
string_to_status(const gchar * status_string)
{
	GEnumClass * klass = G_ENUM_CLASS(g_type_class_ref(APP_INDICATOR_TYPE_INDICATOR_STATUS));
	g_return_val_if_fail(klass != NULL, APP_INDICATOR_STATUS_PASSIVE);

	AppIndicatorStatus retval = APP_INDICATOR_STATUS_PASSIVE;

	GEnumValue * val = g_enum_get_value_by_nick(klass, status_string);
	if (val == NULL) {
		g_warning("Unrecognized status '%s' assuming passive.", status_string);
	} else {
		retval = (AppIndicatorStatus)val->value;
	}

	g_type_class_unref(klass);

	return retval;
}

/* Simple translation function -- could be optimized */
static AppIndicatorCategory
string_to_cat(const gchar * cat_string)
{
	GEnumClass * klass = G_ENUM_CLASS(g_type_class_ref(APP_INDICATOR_TYPE_INDICATOR_CATEGORY));
	g_return_val_if_fail(klass != NULL, APP_INDICATOR_CATEGORY_OTHER);

	AppIndicatorCategory retval = APP_INDICATOR_CATEGORY_OTHER;

	GEnumValue * val = g_enum_get_value_by_nick(klass, cat_string);
	if (val == NULL) {
		g_warning("Unrecognized status '%s' assuming other.", cat_string);
	} else {
		retval = (AppIndicatorCategory)val->value;
	}

	g_type_class_unref(klass);

	return retval;
}


/* A small helper function to get the position of an application
   in the app list of the applications that are visible. */
static gint 
get_position (Application * app) {
	ApplicationServiceAppstore * appstore = app->appstore;
	ApplicationServiceAppstorePrivate * priv = appstore->priv;

	GList * lapp;
	gint count;

	/* Go through the list and try to find ours */
	for (lapp = priv->applications, count = 0; lapp != NULL; lapp = g_list_next(lapp), count++) {
		if (lapp->data == app) {
			break;
		}

		/* If the selected app isn't visible let's not
		   count it's position */
		Application * thisapp = (Application *)(lapp->data);
		if (thisapp->visible_state == VISIBLE_STATE_HIDDEN) {
			count--;
		}
	}

	if (lapp == NULL) {
		g_warning("Unable to find position for app '%s'", app->id);
		return -1;
	}
	
	return count;
}

/* A simple global function for dealing with freeing the information
   in an Application structure */
static void
application_free (Application * app)
{
	if (app == NULL) return;
	g_debug("Application free '%s'", app->id);
	
	/* Handle the case where this could be called by unref'ing one of
	   the proxy objects. */
	if (app->currently_free) return;
	app->currently_free = TRUE;
	
	/* Remove from the application list */
	app->appstore->priv->applications = g_list_remove(app->appstore->priv->applications, app);

	if (app->name_watcher != 0) {
		g_dbus_connection_signal_unsubscribe(g_dbus_proxy_get_connection(app->dbus_proxy), app->name_watcher);
		app->name_watcher = 0;
	}

	if (app->props) {
		g_object_unref(app->props);
	}

	if (app->props_cancel != NULL) {
		g_cancellable_cancel(app->props_cancel);
		g_object_unref(app->props_cancel);
		app->props_cancel = NULL;
	}

	if (app->dbus_proxy) {
		g_object_unref(app->dbus_proxy);
	}

	if (app->dbus_proxy_cancel != NULL) {
		g_cancellable_cancel(app->dbus_proxy_cancel);
		g_object_unref(app->dbus_proxy_cancel);
		app->dbus_proxy_cancel = NULL;
	}

	if (app->id != NULL) {
		g_free(app->id);
	}
	if (app->category != NULL) {
		g_free(app->category);
	}
	if (app->dbus_name != NULL) {
		g_free(app->dbus_name);
	}
	if (app->dbus_object != NULL) {
		g_free(app->dbus_object);
	}
	if (app->icon != NULL) {
		g_free(app->icon);
	}
	if (app->icon_desc != NULL) {
		g_free(app->icon_desc);
	}
	if (app->aicon != NULL) {
		g_free(app->aicon);
	}
	if (app->aicon_desc != NULL) {
		g_free(app->aicon_desc);
	}
	if (app->menu != NULL) {
		g_free(app->menu);
	}
	if (app->icon_theme_path != NULL) {
		g_free(app->icon_theme_path);
	}
	if (app->label != NULL) {
		g_free(app->label);
	}
	if (app->guide != NULL) {
		g_free(app->guide);
	}
	if (app->title != NULL) {
		g_free(app->title);
	}

	g_free(app);
	return;
}

/* Gets called when the proxy changes owners, which is usually when it
   drops off of the bus. */
static void
application_died (Application * app)
{
	/* Application died */
	g_debug("Application proxy destroyed '%s'", app->id);

	/* Remove from the panel */
	app->status = APP_INDICATOR_STATUS_PASSIVE;
	apply_status(app);

	/* Destroy the data */
	application_free(app);
	return;
}

/* This function takes two Application structure
   pointers and uses their ordering index to compare them. */
static gint
app_sort_func (gconstpointer a, gconstpointer b, gpointer userdata)
{
	Application * appa = (Application *)a;
	Application * appb = (Application *)b;
	return (appb->ordering_index/2) - (appa->ordering_index/2);
}

static void
emit_signal (ApplicationServiceAppstore * appstore, const gchar * name,
             GVariant * variant)
{
	ApplicationServiceAppstorePrivate * priv = appstore->priv;
	GError * error = NULL;

	g_dbus_connection_emit_signal (priv->bus,
		                       NULL,
		                       INDICATOR_APPLICATION_DBUS_OBJ,
		                       INDICATOR_APPLICATION_DBUS_IFACE,
		                       name,
		                       variant,
		                       &error);

	if (error != NULL) {
		g_critical("Unable to send %s signal: %s", name, error->message);
		g_error_free(error);
		return;
	}

	return;
}

/* Change the status of the application.  If we're going passive
   it removes it from the panel.  If we're coming online, then
   it add it to the panel.  Otherwise it changes the icon. */
static void
apply_status (Application * app)
{
	ApplicationServiceAppstore * appstore = app->appstore;

	/* g_debug("Applying status.  Status: %d  Visible: %d", app->status, app->visible_state); */

	visible_state_t goal_state = VISIBLE_STATE_HIDDEN;

	if (app->status != APP_INDICATOR_STATUS_PASSIVE) {
		goal_state = VISIBLE_STATE_SHOWN;
	}

	/* Nothing needs to change, we're good */
	if (app->visible_state == goal_state /* ) { */
		&& goal_state == VISIBLE_STATE_HIDDEN) {
		/* TODO: Uhg, this is a little wrong in that we're going to
		   send an icon every time the status changes and the indicator
		   is visible even though it might not be updating.  But, at
		   this point we need a small patch that is harmless.  In the
		   future we need to track which icon is shown and remove the
		   duplicate message. */
		return;
	}

	if (app->visible_state != goal_state) {
		g_debug("Changing app '%s' state from %s to %s", app->id, STATE2STRING(app->visible_state), STATE2STRING(goal_state));
	}

	/* This means we're going off line */
	if (goal_state == VISIBLE_STATE_HIDDEN) {
		gint position = get_position(app);
		if (position == -1) return;

		emit_signal (appstore, "ApplicationRemoved",
		             g_variant_new ("(i)", position));
	} else {
		/* Figure out which icon we should be using */
		gchar * newicon = app->icon;
		gchar * newdesc = app->icon_desc;
		if (app->status == APP_INDICATOR_STATUS_ATTENTION && app->aicon != NULL && app->aicon[0] != '\0') {
			newicon = app->aicon;
			newdesc = app->aicon_desc;
		}

		if (newdesc == NULL) {
			newdesc = "";
		}

		/* Determine whether we're already shown or not */
		if (app->visible_state == VISIBLE_STATE_HIDDEN) {
			/* Put on panel */
			emit_signal (appstore, "ApplicationAdded",
				     g_variant_new ("(sisossssss)", newicon,
			                            get_position(app),
			                            app->dbus_name, app->menu,
			                            app->icon_theme_path,
			                            app->label, app->guide,
			                            newdesc, app->id, app->title));
		} else {
			/* Icon update */
			gint position = get_position(app);
			if (position == -1) return;

			emit_signal (appstore, "ApplicationIconChanged",
				     g_variant_new ("(iss)", position, newicon, newdesc));
			emit_signal (appstore, "ApplicationLabelChanged",
				     g_variant_new ("(iss)", position, 
		                                    app->label != NULL ? app->label : "",
		                                    app->guide != NULL ? app->guide : ""));
			emit_signal (appstore, "ApplicationTitleChanged",
				     g_variant_new ("(is)", position,
		                                    app->title != NULL ? app->title : ""));
		}
	}

	app->visible_state = goal_state;

	return;
}

/* Called when the Notification Item signals that it
   has a new status. */
static void
new_status (Application * app, const gchar * status)
{
	app->status = string_to_status(status);
	apply_status(app);

	return;
}

/* Called when the Notification Item signals that it
   has a new icon theme path. */
static void
new_icon_theme_path (Application * app, const gchar * icon_theme_path)
{
	if (g_strcmp0(icon_theme_path, app->icon_theme_path)) {
		/* If the new icon theme path is actually a new icon theme path */
		if (app->icon_theme_path != NULL) g_free(app->icon_theme_path);
		app->icon_theme_path = g_strdup(icon_theme_path);

		if (app->visible_state != VISIBLE_STATE_HIDDEN) {
			gint position = get_position(app);
			if (position == -1) return;

			emit_signal (app->appstore,
			             "ApplicationIconThemePathChanged",
				     g_variant_new ("(is)", position,
			                            app->icon_theme_path));
		}
	}

	return;
}

/* Called when the Notification Item signals that it
   has a new label. */
static void
new_label (Application * app, const gchar * label, const gchar * guide)
{
	gboolean changed = FALSE;

	if (g_strcmp0(app->label, label) != 0) {
		changed = TRUE;
		if (app->label != NULL) {
			g_free(app->label);
			app->label = NULL;
		}
		app->label = g_strdup(label);
	}

	if (g_strcmp0(app->guide, guide) != 0) {
		changed = TRUE;
		if (app->guide != NULL) {
			g_free(app->guide);
			app->guide = NULL;
		}
		app->guide = g_strdup(guide);
	}

	if (changed) {
		gint position = get_position(app);
		if (position == -1) return;

		emit_signal (app->appstore, "ApplicationLabelChanged",
			     g_variant_new ("(iss)", position,
		                            app->label != NULL ? app->label : "",
		                            app->guide != NULL ? app->guide : ""));
	}

	return;
}

/* Adding a new NotificationItem object from DBus in to the
   appstore.  First, we need to get the information on it
   though. */
void
application_service_appstore_application_add (ApplicationServiceAppstore * appstore, const gchar * dbus_name, const gchar * dbus_object)
{
	g_debug("Adding new application: %s:%s", dbus_name, dbus_object);

	/* Make sure we got a sensible request */
	g_return_if_fail(IS_APPLICATION_SERVICE_APPSTORE(appstore));
	g_return_if_fail(dbus_name != NULL && dbus_name[0] != '\0');
	g_return_if_fail(dbus_object != NULL && dbus_object[0] != '\0');
	Application * app = find_application(appstore, dbus_name, dbus_object);

	if (app != NULL) {
		g_debug("Application already exists, re-requesting properties.");
		get_all_properties(app);
		return;
	}

	/* Build the application entry.  This will be carried
	   along until we're sure we've got everything. */
	app = g_new0(Application, 1);

	app->validated = FALSE;
	app->dbus_name = g_strdup(dbus_name);
	app->dbus_object = g_strdup(dbus_object);
	app->appstore = appstore;
	app->status = APP_INDICATOR_STATUS_PASSIVE;
	app->icon = NULL;
	app->aicon = NULL;
	app->menu = NULL;
	app->icon_theme_path = NULL;
	app->label = NULL;
	app->guide = NULL;
	app->title = NULL;
	app->currently_free = FALSE;
	app->ordering_index = 0;
	app->visible_state = VISIBLE_STATE_HIDDEN;
	app->name_watcher = 0;
	app->props_cancel = NULL;
	app->props = NULL;
	app->queued_props = FALSE;

	/* Get the DBus proxy for the NotificationItem interface */
	app->dbus_proxy_cancel = g_cancellable_new();
	g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION,
			         G_DBUS_PROXY_FLAGS_NONE,
			         NULL,
	                         app->dbus_name,
	                         app->dbus_object,
	                         NOTIFICATION_ITEM_DBUS_IFACE,
			         app->dbus_proxy_cancel,
			         dbus_proxy_cb,
		                 app);

	appstore->priv->applications = g_list_insert_sorted_with_data (appstore->priv->applications, app, app_sort_func, NULL);

	/* We're returning, nothing is yet added until the properties
	   come back and give us more info. */
	return;
}

static void
name_changed (GDBusConnection * connection, const gchar * sender_name,
              const gchar * object_path, const gchar * interface_name,
              const gchar * signal_name, GVariant * parameters,
              gpointer user_data)
{
	Application * app = (Application *)user_data;

	gchar * new_name = NULL;
	g_variant_get(parameters, "(sss)", NULL, NULL, &new_name);

	if (new_name == NULL || new_name[0] == 0)
		application_died(app);

	g_free(new_name);
}

/* Callback from trying to create the proxy for the app. */
static void
dbus_proxy_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;

	Application * app = (Application *)user_data;
	g_return_if_fail(app != NULL);

	GDBusProxy * proxy = g_dbus_proxy_new_for_bus_finish(res, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return; // Must exit before accessing freed memory
	}

	if (app->dbus_proxy_cancel != NULL) {
		g_object_unref(app->dbus_proxy_cancel);
		app->dbus_proxy_cancel = NULL;
	}

	if (error != NULL) {
		g_critical("Could not grab DBus proxy for %s: %s", app->dbus_name, error->message);
		g_error_free(error);
		application_free(app);
		return;
	}

	/* Okay, we're good to grab the proxy at this point, we're
	sure that it's ours. */
	app->dbus_proxy = proxy;

	/* We've got it, let's watch it for destruction */
	app->name_watcher = g_dbus_connection_signal_subscribe(
	                                   g_dbus_proxy_get_connection(proxy),
	                                   "org.freedesktop.DBus",
	                                   "org.freedesktop.DBus",
	                                   "NameOwnerChanged",
	                                   "/org/freedesktop/DBus",
	                                   g_dbus_proxy_get_name(proxy),
	                                   G_DBUS_SIGNAL_FLAGS_NONE,
	                                   name_changed,
	                                   app,
	                                   NULL);

	g_signal_connect(proxy, "g-signal", G_CALLBACK(app_receive_signal), app);

	app->props_cancel = g_cancellable_new();
	g_dbus_proxy_new(g_dbus_proxy_get_connection(proxy),
			              G_DBUS_PROXY_FLAGS_NONE,
			              NULL,
	                              app->dbus_name,
	                              app->dbus_object,
	                              "org.freedesktop.DBus.Properties",
		                      app->props_cancel,
		                      props_cb,
		                      app);

	return;
}

/* Callback from trying to create the proxy for the app. */
static void
props_cb (GObject * object, GAsyncResult * res, gpointer user_data)
{
	GError * error = NULL;

	Application * app = (Application *)user_data;
	g_return_if_fail(app != NULL);

	GDBusProxy * proxy = g_dbus_proxy_new_for_bus_finish(res, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return; // Must exit before accessing freed memory
	}

	if (app->props_cancel != NULL) {
		g_object_unref(app->props_cancel);
		app->props_cancel = NULL;
	}

	if (error != NULL) {
		g_critical("Could not grab Properties DBus proxy for %s: %s", app->dbus_name, error->message);
		g_error_free(error);
		application_free(app);
		return;
	}

	/* Okay, we're good to grab the proxy at this point, we're
	sure that it's ours. */
	app->props = proxy;

	get_all_properties(app);

	return;
}

/* Receives all signals from the service, routed to the appropriate functions */
static void
app_receive_signal (GDBusProxy * proxy, gchar * sender_name, gchar * signal_name,
                    GVariant * parameters, gpointer user_data)
{
	Application * app = (Application *)user_data;

	if (!app->validated) return;

	if (g_strcmp0(signal_name, NOTIFICATION_ITEM_SIG_NEW_ICON) == 0) {
		/* icon name isn't provided by signal, so look it up */
		get_all_properties(app);
	}
	else if (g_strcmp0(signal_name, NOTIFICATION_ITEM_SIG_NEW_AICON) == 0) {
		/* aicon name isn't provided by signal, so look it up */
		get_all_properties(app);
	}
	else if (g_strcmp0(signal_name, NOTIFICATION_ITEM_SIG_NEW_TITLE) == 0) {
		/* title name isn't provided by signal, so look it up */
		get_all_properties(app);
	}
	else if (g_strcmp0(signal_name, NOTIFICATION_ITEM_SIG_NEW_STATUS) == 0) {
		gchar * status = NULL;
		g_variant_get(parameters, "(s)", &status);
		new_status(app, status);
		g_free(status);
	}
	else if (g_strcmp0(signal_name, NOTIFICATION_ITEM_SIG_NEW_ICON_THEME_PATH) == 0) {
		gchar * icon_theme_path = NULL;
		g_variant_get(parameters, "(s)", &icon_theme_path);
		new_icon_theme_path(app, icon_theme_path);
		g_free(icon_theme_path);
	}
	else if (g_strcmp0(signal_name, NOTIFICATION_ITEM_SIG_NEW_LABEL) == 0) {
		gchar * label = NULL, * guide = NULL;
		g_variant_get(parameters, "(ss)", &label, &guide);
		new_label(app, label, guide);
		g_free(label);
		g_free(guide);
	}

	return;
}

/* Looks for an application in the list of applications */
static Application *
find_application (ApplicationServiceAppstore * appstore, const gchar * address, const gchar * object)
{
	ApplicationServiceAppstorePrivate * priv = appstore->priv;
	GList * listpntr;

	for (listpntr = priv->applications; listpntr != NULL; listpntr = g_list_next(listpntr)) {
		Application * app = (Application *)listpntr->data;

		if (!g_strcmp0(app->dbus_name, address) && !g_strcmp0(app->dbus_object, object)) {
			return app;
		}
	}

	return NULL;
}

/* Looks for an application in the list of applications with the matching menu */
static Application *
find_application_by_menu (ApplicationServiceAppstore * appstore, const gchar * address, const gchar * menuobject)
{
	g_return_val_if_fail(appstore, NULL);
	g_return_val_if_fail(address, NULL);
	g_return_val_if_fail(menuobject, NULL);

	ApplicationServiceAppstorePrivate * priv = appstore->priv;
	GList *l;

	for (l = priv->applications; l != NULL; l = l->next) {
		Application *a = l->data;
		if (g_strcmp0(a->dbus_name, address) == 0 &&
		      g_strcmp0(a->menu, menuobject) == 0) {
			return a;
		}
	}

	return NULL;
}

/* Removes an application.  Currently only works for the apps
   that are shown. */
void
application_service_appstore_application_remove (ApplicationServiceAppstore * appstore, const gchar * dbus_name, const gchar * dbus_object)
{
	g_return_if_fail(IS_APPLICATION_SERVICE_APPSTORE(appstore));
	g_return_if_fail(dbus_name != NULL && dbus_name[0] != '\0');
	g_return_if_fail(dbus_object != NULL && dbus_object[0] != '\0');

	Application * app = find_application(appstore, dbus_name, dbus_object);
	if (app != NULL) {
		application_died(app);
	} else {
		g_warning("Unable to find application %s:%s", dbus_name, dbus_object);
	}

	return;
}

gchar**
application_service_appstore_application_get_list (ApplicationServiceAppstore * appstore)
{
	ApplicationServiceAppstorePrivate * priv = appstore->priv;
	gchar ** out;
	gchar ** outpntr;
	GList * listpntr;

	out = g_new(gchar*, g_list_length(priv->applications) + 1);

	for (listpntr = priv->applications, outpntr = out; listpntr != NULL; listpntr = g_list_next(listpntr), ++outpntr) {
		Application * app = (Application *)listpntr->data;
		*outpntr = g_strdup_printf("%s%s", app->dbus_name, app->dbus_object);
	}
	*outpntr = 0;
	return out;
}

/* Creates a basic appstore object and attaches the
   LRU file object to it. */
ApplicationServiceAppstore *
application_service_appstore_new (void)
{
	ApplicationServiceAppstore * appstore = APPLICATION_SERVICE_APPSTORE(g_object_new(APPLICATION_SERVICE_APPSTORE_TYPE, NULL));
	return appstore;
}

/* DBus Interface */
static GVariant *
get_applications (ApplicationServiceAppstore * appstore)
{
	ApplicationServiceAppstorePrivate * priv = appstore->priv;
	GVariant * out = NULL;

	if (g_list_length(priv->applications) > 0) {
		GVariantBuilder builder;
		GList * listpntr;
		gint position = 0;

		g_variant_builder_init(&builder, G_VARIANT_TYPE ("a(sisossssss)"));

		for (listpntr = priv->applications; listpntr != NULL; listpntr = g_list_next(listpntr)) {
			Application * app = (Application *)listpntr->data;
			if (app->visible_state == VISIBLE_STATE_HIDDEN) {
				continue;
			}

			g_variant_builder_add (&builder, "(sisossssss)", app->icon,
			                       position++, app->dbus_name, app->menu,
			                       app->icon_theme_path, app->label,
			                       app->guide,
			                       (app->icon_desc != NULL) ? app->icon_desc : "",
			                       app->id, app->title);
		}

		out = g_variant_builder_end(&builder);
	} else {
		GError * error = NULL;
		out = g_variant_parse(g_variant_type_new("a(sisossssss)"), "[]", NULL, NULL, &error);
		if (error != NULL) {
			g_warning("Unable to parse '[]' as a 'a(sisossssss)': %s", error->message);
			out = NULL;
			g_error_free(error);
		}
	}

	if (out != NULL) {
		return g_variant_new_tuple(&out, 1);
	} else {
		return NULL;
	}
}
