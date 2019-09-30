/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2010       Giovanni Campagna <scampa.giovanni@gmail.com>
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

/**
 * SECTION:bluetooth-client
 * @short_description: Bluetooth client object
 * @stability: Stable
 * @include: bluetooth-client.h
 *
 * The #BluetoothClient object is used to query the state of Bluetooth
 * devices and adapters.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "bluetooth-client.h"
#include "bluetooth-client-private.h"
#include "bluetooth-client-glue.h"
#include "bluetooth-utils.h"
#include "gnome-bluetooth-enum-types.h"

#define BLUEZ_SERVICE			"org.bluez"
#define BLUEZ_MANAGER_PATH		"/"
#define BLUEZ_MANAGER_INTERFACE		"org.bluez.Manager"
#define BLUEZ_ADAPTER_INTERFACE		"org.bluez.Adapter"
#define BLUEZ_DEVICE_INTERFACE		"org.bluez.Device"

static char * detectable_interfaces[] = {
	"org.bluez.Headset",
	"org.bluez.AudioSink",
	"org.bluez.Audio",
	"org.bluez.Input",
};

static char * connectable_interfaces[] = {
	"org.bluez.Audio",
	"org.bluez.Input"
};

/* Keep in sync with above */
#define BLUEZ_INPUT_INTERFACE	(connectable_interfaces[1])
#define BLUEZ_AUDIO_INTERFACE (connectable_interfaces[0])
#define BLUEZ_HEADSET_INTERFACE (detectable_interfaces[0])
#define BLUEZ_AUDIOSINK_INTERFACE (detectable_interfaces[1])

#define BLUETOOTH_CLIENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
				BLUETOOTH_TYPE_CLIENT, BluetoothClientPrivate))

typedef struct _BluetoothClientPrivate BluetoothClientPrivate;

struct _BluetoothClientPrivate {
	guint owner_change_id;
	Manager *manager;
	GtkTreeStore *store;
	GtkTreeRowReference *default_adapter;
};

enum {
	PROP_0,
	PROP_DEFAULT_ADAPTER,
	PROP_DEFAULT_ADAPTER_POWERED,
	PROP_DEFAULT_ADAPTER_DISCOVERABLE,
	PROP_DEFAULT_ADAPTER_NAME,
	PROP_DEFAULT_ADAPTER_DISCOVERING
};

G_DEFINE_TYPE(BluetoothClient, bluetooth_client, G_TYPE_OBJECT)

typedef gboolean (*IterSearchFunc) (GtkTreeStore *store,
				GtkTreeIter *iter, gpointer user_data);

static gboolean iter_search(GtkTreeStore *store,
				GtkTreeIter *iter, GtkTreeIter *parent,
				IterSearchFunc func, gpointer user_data)
{
	gboolean cont, found = FALSE;

	if (parent == NULL)
		cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),
									iter);
	else
		cont = gtk_tree_model_iter_children(GTK_TREE_MODEL(store),
								iter, parent);

	while (cont == TRUE) {
		GtkTreeIter child;

		found = func(store, iter, user_data);
		if (found == TRUE)
			break;

		found = iter_search(store, &child, iter, func, user_data);
		if (found == TRUE) {
			*iter = child;
			break;
		}

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), iter);
	}

	return found;
}

static gboolean compare_path(GtkTreeStore *store,
					GtkTreeIter *iter, gpointer user_data)
{
	const gchar *path = user_data;
	GDBusProxy *object;
	gboolean found = FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(store), iter,
					BLUETOOTH_COLUMN_PROXY, &object, -1);

	if (object != NULL) {
		found = g_str_equal(path, g_dbus_proxy_get_object_path(object));
		g_object_unref(object);
	}

	return found;
}

static gboolean
compare_address (GtkTreeStore *store,
		 GtkTreeIter *iter,
		 gpointer user_data)
{
	const char *address = user_data;
	char *tmp_address;
	gboolean found = FALSE;

	gtk_tree_model_get (GTK_TREE_MODEL(store), iter,
			    BLUETOOTH_COLUMN_ADDRESS, &tmp_address, -1);
	found = g_str_equal (address, tmp_address);
	g_free (tmp_address);

	return found;
}

static gboolean
get_iter_from_path (GtkTreeStore *store,
		    GtkTreeIter *iter,
		    const char *path)
{
	return iter_search(store, iter, NULL, compare_path, (gpointer) path);
}

static gboolean
get_iter_from_proxy(GtkTreeStore *store,
		    GtkTreeIter *iter,
		    GDBusProxy *proxy)
{
	return iter_search(store, iter, NULL, compare_path,
			   (gpointer) g_dbus_proxy_get_object_path (proxy));
}

static gboolean
get_iter_from_address (GtkTreeStore *store,
		       GtkTreeIter  *iter,
		       const char   *address,
		       GDBusProxy   *adapter)
{
	GtkTreeIter parent_iter;

	if (get_iter_from_proxy (store, &parent_iter, adapter) == FALSE)
		return FALSE;

	return iter_search (store, iter, &parent_iter, compare_address, (gpointer) address);
}

static BluetoothStatus
status_from_variant (const char *property,
		     GVariant   *variant)
{
	BluetoothStatus status;

	status = BLUETOOTH_STATUS_INVALID;

	if (g_str_equal (property, "Connected") != FALSE) {
		status = g_variant_get_boolean (variant) ?
			BLUETOOTH_STATUS_CONNECTED :
			BLUETOOTH_STATUS_DISCONNECTED;
	} else if (g_str_equal (property, "State") != FALSE) {
		GEnumClass *eclass;
		GEnumValue *ev;
		eclass = g_type_class_ref (BLUETOOTH_TYPE_STATUS);
		ev = g_enum_get_value_by_nick (eclass, g_variant_get_string (variant, NULL));
		if (ev == NULL) {
			g_warning ("Unknown status '%s'", g_variant_get_string (variant, NULL));
			status = BLUETOOTH_STATUS_DISCONNECTED;
		} else {
			status = ev->value;
		}
		g_type_class_unref (eclass);
	}

	return status;
}

static void
proxy_g_signal (GDBusProxy      *proxy,
		gchar           *sender_name,
		gchar           *signal_name,
		GVariant        *parameters,
		BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	GtkTreePath *tree_path;
	GHashTable *table;
	const char *path;
	char *property;
	GVariant *variant;
	BluetoothStatus status;

	if (g_strcmp0 (signal_name, "PropertyChanged") != 0)
		return;

	g_variant_get (parameters, "(sv)", &property, &variant);

	path = g_dbus_proxy_get_object_path (proxy);
	if (get_iter_from_path (priv->store, &iter, path) == FALSE)
		return;

	status = status_from_variant (property, variant);
	if (status == BLUETOOTH_STATUS_INVALID)
		goto end;

	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
			    BLUETOOTH_COLUMN_SERVICES, &table,
			    -1);

	g_hash_table_insert (table,
			     (gpointer) g_dbus_proxy_get_interface_name (proxy),
			     GINT_TO_POINTER (status));

	tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), &iter);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (priv->store), tree_path, &iter);
	gtk_tree_path_free (tree_path);

end:
	g_free (property);
	g_variant_unref (variant);
}

static GDBusProxy *
get_proxy_for_iface (Device          *device,
		     const char      *interface,
		     BluetoothClient *client)
{
	GDBusProxy *proxy;

	proxy = g_object_get_data (G_OBJECT (device), interface);
	if (proxy != NULL)
		return g_object_ref (proxy);

	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
					       NULL,
					       g_dbus_proxy_get_name (G_DBUS_PROXY (device)),
					       g_dbus_proxy_get_object_path (G_DBUS_PROXY (device)),
					       interface,
					       NULL,
					       NULL);

	if (proxy == NULL)
		return NULL;

	g_object_set_data_full (G_OBJECT (device), interface, proxy, g_object_unref);
	g_signal_connect (G_OBJECT (proxy), "g-signal",
			  G_CALLBACK (proxy_g_signal), client);

	return g_object_ref (proxy);
}

static GVariant *
get_properties_for_iface (GDBusProxy *proxy)
{
	GVariant *ret, *variant;

	variant = g_dbus_proxy_call_sync (proxy,
					  "GetProperties",
					  g_variant_new ("()"),
					  G_DBUS_CALL_FLAGS_NONE,
					  -1,
					  NULL,
					  NULL);
	if (variant == NULL)
		return NULL;
	g_variant_get (variant,
		       "(@a{sv})",
		       &ret);
	g_variant_unref (variant);
	return ret;
}

static GHashTable *
device_list_nodes (Device *device, BluetoothClient *client)
{
	GHashTable *table;
	guint i;

	if (device == NULL)
		return NULL;

	table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

	for (i = 0; i < G_N_ELEMENTS (detectable_interfaces); i++) {
		GDBusProxy *iface;
		GVariant *props;

		/* Don't add the input interface for devices that already have
		 * audio stuff */
		if (g_str_equal (detectable_interfaces[i], BLUEZ_INPUT_INTERFACE)
		    && g_hash_table_size (table) > 0)
			continue;

		/* Don't add the audio interface if there's no Headset or AudioSink,
		 * that means that it could only receive audio */
		if (g_str_equal (detectable_interfaces[i], BLUEZ_AUDIO_INTERFACE)) {
			if (g_hash_table_lookup (table, BLUEZ_HEADSET_INTERFACE) == NULL &&
			    g_hash_table_lookup (table, BLUEZ_AUDIOSINK_INTERFACE) == NULL) {
				continue;
			}
		}

		/* And skip interface if it's already in the hash table */
		if (g_hash_table_lookup (table, detectable_interfaces[i]) != NULL)
			continue;

		/* No such interface for this device? */
		iface = get_proxy_for_iface (device, detectable_interfaces[i], client);
		if (iface == NULL)
			continue;

		props = get_properties_for_iface (iface);
		if (props) {
			GVariant *value;
			BluetoothStatus status;

			status = BLUETOOTH_STATUS_INVALID;
			value = g_variant_lookup_value (props, "Connected", G_VARIANT_TYPE_BOOLEAN);
			if (value != NULL) {
				status = status_from_variant ("Connected", value);
			} else {
				value = g_variant_lookup_value (props, "State", G_VARIANT_TYPE_STRING);
				if (value != NULL)
					status = status_from_variant ("State", value);
			}

			if (status != BLUETOOTH_STATUS_INVALID)
				g_hash_table_insert (table, (gpointer) detectable_interfaces[i], GINT_TO_POINTER (status));
			g_variant_unref (props);
		}
		g_object_unref (iface);
	}

	if (g_hash_table_size (table) == 0) {
		g_hash_table_destroy (table);
		return NULL;
	}

	return table;
}

static char **
device_list_uuids (GVariant *variant)
{
	GPtrArray *ret;
	const char **uuids;
	guint i;

	if (variant == NULL)
		return NULL;

	uuids = g_variant_get_strv (variant, NULL);
	if (uuids == NULL)
		return NULL;

	ret = g_ptr_array_new ();

	for (i = 0; uuids[i] != NULL; i++) {
		const char *uuid;

		uuid = bluetooth_uuid_to_string (uuids[i]);
		if (uuid == NULL)
			continue;
		g_ptr_array_add (ret, g_strdup (uuid));
	}
	g_free (uuids);

	if (ret->len == 0) {
		g_ptr_array_free (ret, TRUE);
		return NULL;
	}

	g_ptr_array_add (ret, NULL);

	return (char **) g_ptr_array_free (ret, FALSE);
}

static void
device_changed (Device          *device,
		const char      *property,
		GVariant        *variant,
		BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;

	if (get_iter_from_proxy(priv->store, &iter, G_DBUS_PROXY (device)) == FALSE)
		return;

	if (g_str_equal(property, "Name") == TRUE) {
		const gchar *name = g_variant_get_string (variant, NULL);

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_NAME, name, -1);
	} else if (g_str_equal(property, "Alias") == TRUE) {
		const gchar *alias = g_variant_get_string(variant, NULL);

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_ALIAS, alias, -1);
	} else if (g_str_equal(property, "Icon") == TRUE) {
		const gchar *icon = g_variant_get_string(variant, NULL);

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_ICON, icon, -1);
	} else if (g_str_equal(property, "Paired") == TRUE) {
		gboolean paired = g_variant_get_boolean(variant);

		gtk_tree_store_set(priv->store, &iter,
				BLUETOOTH_COLUMN_PAIRED, paired, -1);
	} else if (g_str_equal(property, "Trusted") == TRUE) {
		gboolean trusted = g_variant_get_boolean(variant);

		gtk_tree_store_set(priv->store, &iter,
				BLUETOOTH_COLUMN_TRUSTED, trusted, -1);
	} else if (g_str_equal(property, "Connected") == TRUE) {
		gboolean connected = g_variant_get_boolean(variant);

		gtk_tree_store_set(priv->store, &iter,
				BLUETOOTH_COLUMN_CONNECTED, connected, -1);
	} else if (g_str_equal (property, "UUIDs") == TRUE) {
		GHashTable *services;
		char **uuids;

		uuids = device_list_uuids (variant);
		services = device_list_nodes (device, client);
		gtk_tree_store_set (priv->store, &iter,
				    BLUETOOTH_COLUMN_SERVICES, services,
				    BLUETOOTH_COLUMN_UUIDS, uuids, -1);
		g_strfreev (uuids);
		if (services != NULL)
			g_hash_table_unref (services);
	} else {
		g_debug ("Unhandled property: %s", property);
	}
}

static void
device_g_signal (GDBusProxy      *proxy,
		 gchar           *sender_name,
		 gchar           *signal_name,
		 GVariant        *parameters,
		 BluetoothClient *client)
{
	char *property;
	GVariant *variant;

	if (g_strcmp0 (signal_name, "PropertyChanged") != 0)
		return;

	g_variant_get (parameters, "(sv)", &property, &variant);
	device_changed (DEVICE (proxy), property, variant, client);
	g_free (property);
	g_variant_unref (variant);
}

static void
add_device (Adapter         *adapter,
	    GtkTreeIter     *parent,
	    BluetoothClient *client,
	    const char      *path,
	    GVariant        *dict)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	Device *device;
	GVariant *v;
	const gchar *address, *alias, *name, *icon;
	char **uuids;
	gboolean paired, trusted, connected;
	int legacypairing;
	guint type;
	GtkTreeIter iter;
	GVariant *ret;

	if (path == NULL && dict == NULL)
		return;

	if (path != NULL) {
		ret = NULL;
		device = device_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
							G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
							BLUEZ_SERVICE,
							path,
							NULL,
							NULL);
		if (device == NULL)
			return;
		device_call_get_properties_sync (DEVICE (device), &ret, NULL, NULL);
		if (ret == NULL) {
			g_object_unref (device);
			return;
		}
	} else {
		device = NULL;
		ret = g_variant_ref (dict);
	}

	v = g_variant_lookup_value (ret, "Address", G_VARIANT_TYPE_STRING);
	address = v ? g_variant_get_string (v, NULL) : NULL;

	v = g_variant_lookup_value (ret, "Alias", G_VARIANT_TYPE_STRING);
	alias = v ? g_variant_get_string (v, NULL) : NULL;

	v = g_variant_lookup_value (ret, "Name", G_VARIANT_TYPE_STRING);
	name = v ? g_variant_get_string (v, NULL) : NULL;

	v = g_variant_lookup_value (ret, "Class", G_VARIANT_TYPE_UINT32);
	type = v ? bluetooth_class_to_type (g_variant_get_uint32 (v)) : BLUETOOTH_TYPE_ANY;

	v = g_variant_lookup_value (ret, "Icon", G_VARIANT_TYPE_STRING);
	icon = v ? g_variant_get_string (v, NULL) : "bluetooth";

	v = g_variant_lookup_value (ret, "Paired", G_VARIANT_TYPE_BOOLEAN);
	paired = v ? g_variant_get_boolean (v) : FALSE;

	v = g_variant_lookup_value (ret, "Trusted", G_VARIANT_TYPE_BOOLEAN);
	trusted = v ? g_variant_get_boolean (v) : FALSE;

	v = g_variant_lookup_value (ret, "Connected", G_VARIANT_TYPE_BOOLEAN);
	connected = v ? g_variant_get_boolean (v) : FALSE;

	v = g_variant_lookup_value (ret, "UUIDs", G_VARIANT_TYPE_STRING_ARRAY);
	uuids = device_list_uuids (v);

	v = g_variant_lookup_value (ret, "LegacyPairing", G_VARIANT_TYPE_BOOLEAN);
	legacypairing = v ? g_variant_get_boolean (v) : -1;

	if (get_iter_from_address (priv->store, &iter, address, G_DBUS_PROXY (adapter)) == FALSE)
		gtk_tree_store_insert (priv->store, &iter, parent, -1);

	gtk_tree_store_set(priv->store, &iter,
			   BLUETOOTH_COLUMN_ADDRESS, address,
			   BLUETOOTH_COLUMN_ALIAS, alias,
			   BLUETOOTH_COLUMN_NAME, name,
			   BLUETOOTH_COLUMN_TYPE, type,
			   BLUETOOTH_COLUMN_ICON, icon,
			   BLUETOOTH_COLUMN_LEGACYPAIRING, legacypairing,
			   BLUETOOTH_COLUMN_UUIDS, uuids,
			   BLUETOOTH_COLUMN_PAIRED, paired,
			   -1);
	g_strfreev (uuids);

	if (device != NULL) {
		GHashTable *services;

		services = device_list_nodes (device, client);

		gtk_tree_store_set(priv->store, &iter,
				   BLUETOOTH_COLUMN_PROXY, device,
				   BLUETOOTH_COLUMN_CONNECTED, connected,
				   BLUETOOTH_COLUMN_TRUSTED, trusted,
				   BLUETOOTH_COLUMN_SERVICES, services,
				   -1);

		if (services != NULL)
			g_hash_table_unref (services);
	}
	g_variant_unref (ret);

	if (device != NULL) {
		g_signal_connect (G_OBJECT (device), "g-signal",
				  G_CALLBACK (device_g_signal), client);
		g_object_unref (device);
	}
}

static void
device_found (Adapter         *adapter,
	      const char      *address,
	      GVariant        *dict,
	      BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;

	if (get_iter_from_proxy(priv->store, &iter, G_DBUS_PROXY (adapter)) == TRUE)
		add_device(adapter, &iter, client, NULL, dict);
}

static void
device_created (Adapter         *adapter,
		const char      *path,
		BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;

	if (get_iter_from_proxy(priv->store, &iter, G_DBUS_PROXY (adapter)) == TRUE)
		add_device(adapter, &iter, client, path, NULL);
}

static void
device_removed (GDBusProxy      *adapter,
		const char      *path,
		BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;

	if (get_iter_from_path(priv->store, &iter, path) == TRUE)
		gtk_tree_store_remove(priv->store, &iter);
}

static void
adapter_changed (GDBusProxy      *adapter,
		 const char      *property,
		 GVariant        *value,
		 BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	gboolean notify = FALSE;

	if (get_iter_from_proxy(priv->store, &iter, adapter) == FALSE)
		return;

	if (g_str_equal(property, "Name") == TRUE) {
		const gchar *name = g_variant_get_string (value, NULL);
		gboolean is_default;

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_NAME, name, -1);
		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
				   BLUETOOTH_COLUMN_DEFAULT, &is_default, -1);
		if (is_default != FALSE)
			g_object_notify (G_OBJECT (client), "default-adapter-powered");
		notify = TRUE;
	} else if (g_str_equal(property, "Discovering") == TRUE) {
		gboolean discovering = g_variant_get_boolean(value);

		gtk_tree_store_set(priv->store, &iter,
				BLUETOOTH_COLUMN_DISCOVERING, discovering, -1);
		notify = TRUE;
	} else if (g_str_equal(property, "Powered") == TRUE) {
		gboolean powered = g_variant_get_boolean(value);
		gboolean is_default;

		gtk_tree_store_set(priv->store, &iter,
				   BLUETOOTH_COLUMN_POWERED, powered, -1);
		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
				   BLUETOOTH_COLUMN_DEFAULT, &is_default, -1);
		if (is_default != FALSE)
			g_object_notify (G_OBJECT (client), "default-adapter-powered");
		notify = TRUE;
	} else if (g_str_equal(property, "Discoverable") == TRUE) {
		gboolean discoverable = g_variant_get_boolean(value);
		gboolean is_default;

		gtk_tree_store_set(priv->store, &iter,
				   BLUETOOTH_COLUMN_DISCOVERABLE, discoverable, -1);
		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
				   BLUETOOTH_COLUMN_DEFAULT, &is_default, -1);
		if (is_default != FALSE)
			g_object_notify (G_OBJECT (client), "default-adapter-discoverable");
		notify = TRUE;
	}

	if (notify != FALSE) {
		GtkTreePath *path;

		/* Tell the world */
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), &iter);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (priv->store), path, &iter);
		gtk_tree_path_free (path);
	}
}

static void
adapter_g_signal (GDBusProxy      *proxy,
		  gchar           *sender_name,
		  gchar           *signal_name,
		  GVariant        *parameters,
		  BluetoothClient *client)
{
	if (g_strcmp0 (signal_name, "PropertyChanged") == 0) {
		char *property;
		GVariant *variant;

		g_variant_get (parameters, "(sv)", &property, &variant);
		adapter_changed (proxy, property, variant, client);
		g_free (property);
		g_variant_unref (variant);
	} else if (g_strcmp0 (signal_name, "DeviceCreated") == 0) {
		char *object_path;
		g_variant_get (parameters, "(o)", &object_path);
		device_created (ADAPTER (proxy), object_path, client);
		g_free (object_path);
	} else if (g_strcmp0 (signal_name, "DeviceRemoved") == 0) {
		char *object_path;
		g_variant_get (parameters, "(o)", &object_path);
		device_removed (proxy, object_path, client);
		g_free (object_path);
	} else if (g_strcmp0 (signal_name, "DeviceFound") == 0) {
		char *address;
		GVariant *dict;
		g_variant_get (parameters, "(s@a{sv})", &address, &dict);
		device_found (ADAPTER (proxy), address, dict, client);
		g_free (address);
		g_variant_unref (dict);
	}
}

static void
adapter_added (Manager         *manager,
	       const char      *path,
	       BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	Adapter *adapter;
	const char **devices;
	GVariant *variant;
	const gchar *address, *name;
	gboolean discovering, discoverable, powered;

	variant = NULL;
	adapter = adapter_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
						  BLUEZ_SERVICE,
						  path,
						  NULL,
						  NULL);

	if (adapter_call_get_properties_sync (adapter, &variant, NULL, NULL) == TRUE) {
		GVariant *v;

		v = g_variant_lookup_value (variant, "Address", G_VARIANT_TYPE_STRING);
		address = v ? g_variant_get_string (v, NULL) : NULL;

		v = g_variant_lookup_value (variant, "Name", G_VARIANT_TYPE_STRING);
		name = v ? g_variant_get_string (v, NULL) : NULL;

		v = g_variant_lookup_value (variant, "Discovering", G_VARIANT_TYPE_BOOLEAN);
		discovering = v ? g_variant_get_boolean (v) : FALSE;

		v = g_variant_lookup_value (variant, "Powered", G_VARIANT_TYPE_BOOLEAN);
		powered = v ? g_variant_get_boolean (v) : FALSE;

		v = g_variant_lookup_value (variant, "Devices", G_VARIANT_TYPE_OBJECT_PATH_ARRAY);
		devices = v ? g_variant_get_objv (v, NULL) : NULL;

		v = g_variant_lookup_value (variant, "Discoverable", G_VARIANT_TYPE_BOOLEAN);
		discoverable = v ? g_variant_get_boolean (v) : FALSE;

		g_variant_unref (variant);
	} else {
		address = NULL;
		name = NULL;
		discovering = FALSE;
		discoverable = FALSE;
		powered = FALSE;
		devices = NULL;
	}

	gtk_tree_store_insert_with_values(priv->store, &iter, NULL, -1,
					  BLUETOOTH_COLUMN_PROXY, adapter,
					  BLUETOOTH_COLUMN_ADDRESS, address,
					  BLUETOOTH_COLUMN_NAME, name,
					  BLUETOOTH_COLUMN_DISCOVERING, discovering,
					  BLUETOOTH_COLUMN_DISCOVERABLE, discoverable,
					  BLUETOOTH_COLUMN_POWERED, powered,
					  -1);

	g_signal_connect (G_OBJECT (adapter), "g-signal",
			  G_CALLBACK (adapter_g_signal), client);

	if (devices != NULL) {
		guint i;

		for (i = 0; devices[i] != NULL; i++) {
			device_created (adapter, devices[i], client);
		}
		g_free (devices);
	}

	g_object_unref (adapter);
}

static void
adapter_removed (Manager         *manager,
		 const char      *path,
		 BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	gboolean cont;

	cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->store),
									&iter);

	while (cont == TRUE) {
		GDBusProxy *adapter;
		const char *adapter_path;
		gboolean found, was_default;

		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
				   BLUETOOTH_COLUMN_PROXY, &adapter,
				   BLUETOOTH_COLUMN_DEFAULT, &was_default, -1);

		adapter_path = g_dbus_proxy_get_object_path(adapter);

		found = g_str_equal(path, adapter_path);
		g_object_unref(adapter);

		if (found) {
			if (was_default) {
				gtk_tree_row_reference_free (priv->default_adapter);
				priv->default_adapter = NULL;
				g_object_notify (G_OBJECT (client), "default-adapter");
				g_object_notify (G_OBJECT (client), "default-adapter-powered");
				g_object_notify (G_OBJECT (client), "default-adapter-discoverable");
			}
			gtk_tree_store_remove(priv->store, &iter);
			break;
		}

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->store),
									&iter);
	}
}

static void
default_adapter_changed (Manager         *manager,
			 const char      *path,
			 BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	gboolean cont;

	cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->store),
									&iter);
	if (priv->default_adapter) {
		gtk_tree_row_reference_free (priv->default_adapter);
		priv->default_adapter = NULL;
	}

	while (cont == TRUE) {
		GDBusProxy *adapter;
		const char *adapter_path;
		gboolean found, powered;

		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
				   BLUETOOTH_COLUMN_PROXY, &adapter,
				   BLUETOOTH_COLUMN_POWERED, &powered, -1);

		adapter_path = g_dbus_proxy_get_object_path(adapter);

		found = g_str_equal(path, adapter_path);

		g_object_unref(adapter);

		if (found != FALSE) {
			GtkTreePath *tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), &iter);
			priv->default_adapter = gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->store), tree_path);
			gtk_tree_path_free (tree_path);
		}

		gtk_tree_store_set(priv->store, &iter,
					BLUETOOTH_COLUMN_DEFAULT, found, -1);

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->store),
									&iter);
	}

	/* Record the new default adapter */
	g_object_notify (G_OBJECT (client), "default-adapter");
	g_object_notify (G_OBJECT (client), "default-adapter-powered");
	g_object_notify (G_OBJECT (client), "default-adapter-discoverable");
}

static void
manager_g_signal (GDBusProxy      *proxy,
		  gchar           *sender_name,
		  gchar           *signal_name,
		  GVariant        *parameters,
		  BluetoothClient *client)
{
	char *object_path;

	if (g_strcmp0 (signal_name, "PropertyChanged") == 0)
		return;

	g_variant_get (parameters, "(o)", &object_path);

	if (g_strcmp0 (signal_name, "AdapterAdded") == 0) {
		adapter_added (MANAGER (proxy), object_path, client);
	} else if (g_strcmp0 (signal_name, "AdapterRemoved") == 0) {
		adapter_removed (MANAGER (proxy), object_path, client);
	} else if (g_strcmp0 (signal_name, "DefaultAdapterChanged") == 0) {
		default_adapter_changed (MANAGER (proxy), object_path, client);
	} else {
		g_assert_not_reached ();
	}

	g_free (object_path);
}

static void
bluez_appeared_cb (GDBusConnection *connection,
		   const gchar     *name,
		   const gchar     *name_owner,
		   BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GVariant *variant;
	const char **array;
	gchar *default_path = NULL;

	priv->manager = manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
							G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
							BLUEZ_SERVICE,
							BLUEZ_MANAGER_PATH,
							NULL,
							NULL);

	g_signal_connect (G_OBJECT (priv->manager), "g-signal",
			  G_CALLBACK (manager_g_signal), client);

	variant = NULL;
	manager_call_get_properties_sync (MANAGER (priv->manager), &variant, NULL, NULL);
	if (variant != NULL) {
		GVariant *v;
		guint i;

		v = g_variant_lookup_value (variant, "Adapters", G_VARIANT_TYPE_OBJECT_PATH_ARRAY);
		array = v ? g_variant_get_objv (v, NULL) : NULL;

		if (array != NULL) {
			for (i = 0; array[i] != NULL; i++)
				adapter_added(priv->manager, array[i], client);
			g_free (array);
		}

		g_variant_unref (variant);
	}

	manager_call_default_adapter_sync (priv->manager, &default_path, NULL, NULL);
	if (default_path != NULL) {
		default_adapter_changed (priv->manager, default_path, client);
		g_free(default_path);
	}
}

static void
bluez_vanished_cb (GDBusConnection *connection,
		   const gchar     *name,
		   BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);

	if (priv->default_adapter) {
		gtk_tree_row_reference_free (priv->default_adapter);
		priv->default_adapter = NULL;
	}

	gtk_tree_store_clear (priv->store);

	if (priv->manager) {
		g_object_unref (priv->manager);
		priv->manager = NULL;
	}
}

static void bluetooth_client_init(BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);

	g_type_class_ref (BLUETOOTH_TYPE_STATUS);

	priv->store = gtk_tree_store_new(_BLUETOOTH_NUM_COLUMNS, G_TYPE_OBJECT,
					 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
					 G_TYPE_UINT, G_TYPE_STRING,
					 G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
					 G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_INT,
					 G_TYPE_BOOLEAN, G_TYPE_HASH_TABLE, G_TYPE_STRV);

	priv->owner_change_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
						  BLUEZ_SERVICE,
						  G_BUS_NAME_WATCHER_FLAGS_NONE,
						  (GBusNameAppearedCallback) bluez_appeared_cb,
						  (GBusNameVanishedCallback) bluez_vanished_cb,
						  client, NULL);
}

static GDBusProxy *
_bluetooth_client_get_default_adapter(BluetoothClient *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreePath *path;
	GtkTreeIter iter;
	GDBusProxy *adapter;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	if (priv->default_adapter == NULL)
		return NULL;

	path = gtk_tree_row_reference_get_path (priv->default_adapter);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
			    BLUETOOTH_COLUMN_PROXY, &adapter, -1);
	gtk_tree_path_free (path);

	return adapter;
}

static const char*
_bluetooth_client_get_default_adapter_path (BluetoothClient *self)
{
	GDBusProxy *adapter = _bluetooth_client_get_default_adapter (self);

	if (adapter != NULL) {
		const char *ret = g_dbus_proxy_get_object_path (adapter);
		g_object_unref (adapter);
		return ret;
	}
	return NULL;
}

static gboolean
_bluetooth_client_get_default_adapter_powered (BluetoothClient *self)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE (self);
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean ret;

	if (priv->default_adapter == NULL)
		return FALSE;

	path = gtk_tree_row_reference_get_path (priv->default_adapter);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, BLUETOOTH_COLUMN_POWERED, &ret, -1);
	gtk_tree_path_free (path);

	return ret;
}

static char *
_bluetooth_client_get_default_adapter_name (BluetoothClient *self)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE (self);
	GtkTreePath *path;
	GtkTreeIter iter;
	char *ret;

	if (priv->default_adapter == NULL)
		return NULL;

	path = gtk_tree_row_reference_get_path (priv->default_adapter);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, BLUETOOTH_COLUMN_NAME, &ret, -1);
	gtk_tree_path_free (path);

	return ret;
}

/**
 * _bluetooth_client_get_discoverable:
 * @client: a #BluetoothClient
 *
 * Gets the default adapter's discoverable status, cached in the adapter model.
 *
 * Returns: the discoverable status, or FALSE if no default adapter exists
 */
static gboolean
_bluetooth_client_get_discoverable (BluetoothClient *client)
{
	BluetoothClientPrivate *priv;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean ret;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);

	priv = BLUETOOTH_CLIENT_GET_PRIVATE (client);
	if (priv->default_adapter == NULL)
		return FALSE;

	path = gtk_tree_row_reference_get_path (priv->default_adapter);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
                            BLUETOOTH_COLUMN_DISCOVERABLE, &ret, -1);

	return ret;
}

/**
 * _bluetooth_client_set_discoverable:
 * @client: a #BluetoothClient object
 * @discoverable: whether the device should be discoverable
 * @timeout: timeout in seconds for making undiscoverable, or 0 for never
 *
 * Sets the default adapter's discoverable status.
 *
 * Return value: Whether setting the state on the default adapter was successful.
 **/
static gboolean
_bluetooth_client_set_discoverable (BluetoothClient *client,
				    gboolean discoverable,
				    guint timeout)
{
	GError *error = NULL;
	GDBusProxy *adapter;
	gboolean ret;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);

	adapter = _bluetooth_client_get_default_adapter (client);
	if (adapter == NULL)
		return FALSE;

	if (discoverable) {
		ret = adapter_call_set_property_sync (ADAPTER (adapter),
						      "DiscoverableTimeout",
						      g_variant_new_variant (g_variant_new_uint32 (timeout)),
						      NULL, &error);
		if (ret == FALSE) {
			g_warning ("Failed to set DiscoverableTimeout to %d: %s", timeout, error->message);
			g_error_free (error);
			g_object_unref (adapter);
			return ret;
		}
	}

	ret = adapter_call_set_property_sync (ADAPTER (adapter),
					      "Discoverable",
					      g_variant_new_variant (g_variant_new_boolean (discoverable)),
					      NULL, &error);
	if (ret == FALSE) {
		g_warning ("Failed to set Discoverable to %d: %s", discoverable, error->message);
		g_error_free (error);
	}

	g_object_unref(adapter);

	return ret;
}

static void
_bluetooth_client_set_default_adapter_discovering (BluetoothClient *client,
						   gboolean         discover)
{
	GDBusProxy *adapter;

	adapter = _bluetooth_client_get_default_adapter (client);
	if (adapter == NULL)
		return;

	if (discover)
		adapter_call_start_discovery_sync (ADAPTER (adapter), NULL, NULL);
	else
		adapter_call_stop_discovery_sync (ADAPTER (adapter), NULL, NULL);

	g_object_unref(adapter);
}

static gboolean
_bluetooth_client_get_default_adapter_discovering (BluetoothClient *self)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE (self);
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean ret;

	if (priv->default_adapter == NULL)
		return FALSE;

	path = gtk_tree_row_reference_get_path (priv->default_adapter);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, BLUETOOTH_COLUMN_DISCOVERING, &ret, -1);
	gtk_tree_path_free (path);

	return ret;
}

static void
bluetooth_client_get_property (GObject        *object,
			       guint           property_id,
			       GValue         *value,
			       GParamSpec     *pspec)
{
	BluetoothClient *self = BLUETOOTH_CLIENT (object);

	switch (property_id) {
	case PROP_DEFAULT_ADAPTER:
		g_value_set_string (value, _bluetooth_client_get_default_adapter_path (self));
		break;
	case PROP_DEFAULT_ADAPTER_POWERED:
		g_value_set_boolean (value, _bluetooth_client_get_default_adapter_powered (self));
		break;
	case PROP_DEFAULT_ADAPTER_NAME:
		g_value_take_string (value, _bluetooth_client_get_default_adapter_name (self));
		break;
	case PROP_DEFAULT_ADAPTER_DISCOVERABLE:
		g_value_set_boolean (value, _bluetooth_client_get_discoverable (self));
		break;
	case PROP_DEFAULT_ADAPTER_DISCOVERING:
		g_value_set_boolean (value, _bluetooth_client_get_default_adapter_discovering (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
bluetooth_client_set_property (GObject        *object,
			       guint           property_id,
			       const GValue   *value,
			       GParamSpec     *pspec)
{
	BluetoothClient *self = BLUETOOTH_CLIENT (object);

	switch (property_id) {
	case PROP_DEFAULT_ADAPTER_DISCOVERABLE:
	        _bluetooth_client_set_discoverable (self, g_value_get_boolean (value), 0);
		break;
	case PROP_DEFAULT_ADAPTER_DISCOVERING:
		_bluetooth_client_set_default_adapter_discovering (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void bluetooth_client_finalize(GObject *client)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);

	g_bus_unwatch_name (priv->owner_change_id);

	g_type_class_unref (g_type_class_peek (BLUETOOTH_TYPE_STATUS));

	if (priv->manager)
		g_object_unref (priv->manager);

	g_object_unref(priv->store);

	gtk_tree_row_reference_free (priv->default_adapter);

	G_OBJECT_CLASS(bluetooth_client_parent_class)->finalize(client);
}

static void bluetooth_client_class_init(BluetoothClientClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GError *error = NULL;

	g_type_class_add_private(klass, sizeof(BluetoothClientPrivate));

	object_class->finalize = bluetooth_client_finalize;
	object_class->get_property = bluetooth_client_get_property;
	object_class->set_property = bluetooth_client_set_property;

	/**
	 * BluetoothClient:default-adapter:
	 *
	 * The D-Bus path of the default Bluetooth adapter or %NULL.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER,
					 g_param_spec_string ("default-adapter", NULL,
							      "The D-Bus path of the default adapter",
							      NULL, G_PARAM_READABLE));
	/**
	 * BluetoothClient:default-adapter-powered:
	 *
	 * %TRUE if the default Bluetooth adapter is powered.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_POWERED,
					 g_param_spec_boolean ("default-adapter-powered", NULL,
							      "Whether the default adapter is powered",
							       FALSE, G_PARAM_READABLE));
	/**
	 * BluetoothClient:default-adapter-discoverable:
	 *
	 * %TRUE if the default Bluetooth adapter is discoverable.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_DISCOVERABLE,
					 g_param_spec_boolean ("default-adapter-discoverable", NULL,
							      "Whether the default adapter is visible by other devices",
							       FALSE, G_PARAM_READWRITE));
	/**
	 * BluetoothClient:default-adapter-name:
	 *
	 * The name of the default Bluetooth adapter or %NULL.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_NAME,
					 g_param_spec_string ("default-adapter-name", NULL,
							      "The human readable name of the default adapter",
							      NULL, G_PARAM_READABLE));
	/**
	 * BluetoothClient:default-adapter-discovering:
	 *
	 * %TRUE if the default Bluetooth adapter is discovering.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_DISCOVERING,
					 g_param_spec_boolean ("default-adapter-discovering", NULL,
							      "Whether the default adapter is searching for devices",
							       FALSE, G_PARAM_READWRITE));

	if (error != NULL) {
		g_printerr("Connecting to system bus failed: %s\n",
							error->message);
		g_error_free(error);
	}
}

/**
 * bluetooth_client_new:
 *
 * Returns a reference to the #BluetoothClient singleton. Use g_object_unref() when done with the object.
 *
 * Return value: (transfer full): a #BluetoothClient object.
 **/
BluetoothClient *bluetooth_client_new(void)
{
	static BluetoothClient *bluetooth_client = NULL;

	if (bluetooth_client != NULL)
		return g_object_ref(bluetooth_client);

	bluetooth_client = BLUETOOTH_CLIENT (g_object_new (BLUETOOTH_TYPE_CLIENT, NULL));
	g_object_add_weak_pointer (G_OBJECT (bluetooth_client),
				   (gpointer) &bluetooth_client);

	return bluetooth_client;
}

/**
 * bluetooth_client_get_model:
 * @client: a #BluetoothClient object
 *
 * Returns an unfiltered #GtkTreeModel representing the adapter and devices available on the system.
 *
 * Return value: (transfer full): a #GtkTreeModel object.
 **/
GtkTreeModel *bluetooth_client_get_model (BluetoothClient *client)
{
	BluetoothClientPrivate *priv;
	GtkTreeModel *model;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	model = g_object_ref(priv->store);

	return model;
}

/**
 * bluetooth_client_get_filter_model:
 * @client: a #BluetoothClient object
 * @func: a #GtkTreeModelFilterVisibleFunc
 * @data: user data to pass to gtk_tree_model_filter_set_visible_func()
 * @destroy: a destroy function for gtk_tree_model_filter_set_visible_func()
 *
 * Returns a #GtkTreeModelFilter of devices filtered using the @func, @data and @destroy arguments to pass to gtk_tree_model_filter_set_visible_func().
 *
 * Return value: (transfer full): a #GtkTreeModel object.
 **/
GtkTreeModel *bluetooth_client_get_filter_model (BluetoothClient *client,
						 GtkTreeModelFilterVisibleFunc func,
						 gpointer data, GDestroyNotify destroy)
{
	BluetoothClientPrivate *priv;
	GtkTreeModel *model;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	model = gtk_tree_model_filter_new(GTK_TREE_MODEL(priv->store), NULL);

	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(model),
							func, data, destroy);

	return model;
}

static gboolean adapter_filter(GtkTreeModel *model,
					GtkTreeIter *iter, gpointer user_data)
{
	GDBusProxy *proxy;
	gboolean active;

	gtk_tree_model_get(model, iter, BLUETOOTH_COLUMN_PROXY, &proxy, -1);

	if (proxy == NULL)
		return FALSE;

	active = g_str_equal(BLUEZ_ADAPTER_INTERFACE,
					g_dbus_proxy_get_interface_name(proxy));

	g_object_unref(proxy);

	return active;
}

/**
 * bluetooth_client_get_adapter_model:
 * @client: a #BluetoothClient object
 *
 * Returns a #GtkTreeModelFilter with only adapters present.
 *
 * Return value: (transfer full): a #GtkTreeModel object.
 **/
GtkTreeModel *bluetooth_client_get_adapter_model (BluetoothClient *client)
{
	return bluetooth_client_get_filter_model (client, adapter_filter,
						  NULL, NULL);
}

/**
 * bluetooth_client_get_device_model:
 * @client: a #BluetoothClient object
 *
 * Returns a #GtkTreeModelFilter with only devices belonging to the default adapter listed.
 * Note that the model will follow a specific adapter, and will not follow the default adapter.
 * Also note that due to the way #GtkTreeModelFilter works, you will probably want to
 * monitor signals on the "child-model" #GtkTreeModel to monitor for changes.
 *
 * Return value: (transfer full): a #GtkTreeModel object.
 **/
GtkTreeModel *bluetooth_client_get_device_model (BluetoothClient *client)
{
	BluetoothClientPrivate *priv;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean cont, found = FALSE;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), NULL);

	priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->store),
									&iter);

	while (cont == TRUE) {
		gboolean is_default;

		gtk_tree_model_get(GTK_TREE_MODEL(priv->store), &iter,
				BLUETOOTH_COLUMN_DEFAULT, &is_default, -1);

		if (is_default == TRUE) {
			found = TRUE;
			break;
		}

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->store),
									&iter);
	}

	if (found == TRUE) {
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->store),
									&iter);
		model = gtk_tree_model_filter_new(GTK_TREE_MODEL(priv->store),
									path);
		gtk_tree_path_free(path);
	} else
		model = NULL;

	return model;
}

typedef struct {
	BluetoothClientCreateDeviceFunc func;
	gpointer data;
	BluetoothClient *client;
} CreateDeviceData;

static void
create_device_callback (GDBusProxy       *proxy,
			GAsyncResult     *res,
			CreateDeviceData *devdata)
{
	GError *error = NULL;
	char *path = NULL;
	GVariant *ret;

	ret = g_dbus_proxy_call_finish (proxy, res, &error);
	if (ret == NULL) {
		g_warning ("CreateDevice failed: %s", error->message);
	} else {
		g_variant_get (ret, "(o)", &path);
		g_variant_unref (ret);
	}

	if (devdata->func)
		devdata->func(devdata->client, path, error, devdata->data);
	g_free (path);

	if (error != NULL)
		g_error_free (error);

	g_object_unref (devdata->client);
	g_free (devdata);
}

gboolean bluetooth_client_create_device (BluetoothClient *client,
					 const char *address,
					 const char *agent,
					 BluetoothClientCreateDeviceFunc func,
					 gpointer data)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	CreateDeviceData *devdata;
	GDBusProxy *adapter;
	GtkTreeIter iter;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (address != NULL, FALSE);

	adapter = _bluetooth_client_get_default_adapter(client);
	if (adapter == NULL)
		return FALSE;

	/* Remove the pairing if it already exists, but only for pairings */
	if (agent != NULL &&
	    get_iter_from_address(priv->store, &iter, address, adapter) == TRUE) {
		GDBusProxy *device;
		gboolean paired;
		GError *err = NULL;

		gtk_tree_model_get (GTK_TREE_MODEL(priv->store), &iter,
				    BLUETOOTH_COLUMN_PROXY, &device,
				    BLUETOOTH_COLUMN_PAIRED, &paired, -1);
		if (device != NULL &&
		    paired != FALSE &&
		    adapter_call_remove_device_sync (ADAPTER (adapter),
		    				     g_dbus_proxy_get_object_path (device),
		    				     NULL, &err) == FALSE) {
			g_warning ("Failed to remove device '%s': %s", address,
				   err->message);
			g_error_free (err);
		}
		if (device != NULL)
			g_object_unref (device);
	}

	devdata = g_new0 (CreateDeviceData, 1);
	devdata->func = func;
	devdata->data = data;
	devdata->client = g_object_ref (client);

	if (agent != NULL)
		g_dbus_proxy_call (adapter,
				   "CreatePairedDevice",
				   g_variant_new ("(sos)", address, agent, "DisplayYesNo"),
				   G_DBUS_CALL_FLAGS_NONE,
				   90 * 1000,
				   NULL,
				   (GAsyncReadyCallback) create_device_callback,
				   devdata);
	else
		g_dbus_proxy_call (adapter,
				   "CreateDevice",
				   g_variant_new ("(s)", address),
				   G_DBUS_CALL_FLAGS_NONE,
				   90 * 1000,
				   NULL,
				   (GAsyncReadyCallback) create_device_callback,
				   devdata);
	g_object_unref (adapter);

	return TRUE;
}

gboolean
bluetooth_client_set_trusted (BluetoothClient *client,
			      const char      *device,
			      gboolean         trusted)
{
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	GDBusProxy *proxy;

	g_return_val_if_fail (BLUETOOTH_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (device != NULL, FALSE);

	if (get_iter_from_path (priv->store, &iter, device) == FALSE)
		return FALSE;

	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
			    BLUETOOTH_COLUMN_PROXY, &proxy,
			    -1);

	if (proxy == NULL)
		return FALSE;

	device_call_set_property_sync (DEVICE (proxy),
				       "Trusted",
				       g_variant_new_variant (g_variant_new_boolean (trusted)),
				       NULL, NULL);

	g_object_unref(proxy);

	return TRUE;
}

typedef struct {
	GSimpleAsyncResult *simple;
	/* used for disconnect */
	GList *services;
	Device *device;
} ConnectData;

static void
connect_callback (GDBusProxy   *proxy,
		  GAsyncResult *res,
		  ConnectData  *conndata)
{
	GVariant *variant;
	gboolean retval;
	GError *error = NULL;

	variant = g_dbus_proxy_call_finish (proxy, res, &error);
	if (variant == NULL) {
		retval = FALSE;
		g_debug ("Connect failed for %s: %s",
			 g_dbus_proxy_get_object_path (proxy), error->message);
		g_error_free (error);
	} else {
		g_debug ("Connect succeeded for %s",
			 g_dbus_proxy_get_object_path (proxy));
		g_variant_unref (variant);
		retval = TRUE;
	}

	g_simple_async_result_set_op_res_gboolean (conndata->simple, retval);
	g_simple_async_result_complete_in_idle (conndata->simple);

	g_object_unref (conndata->simple);
	g_object_unref (proxy);
	g_free (conndata);
}

static void
disconnect_callback (GDBusProxy   *proxy,
		     GAsyncResult *res,
		     ConnectData  *conndata)
{
	gboolean retval;
	GError *error = NULL;

	if (conndata->services == NULL) {
		retval = device_call_disconnect_finish (conndata->device, res, &error);
		if (retval == FALSE) {
			g_debug ("Disconnect failed for %s: %s",
				 g_dbus_proxy_get_object_path (G_DBUS_PROXY (conndata->device)),
				 error->message);
			g_error_free (error);
		} else {
			g_debug ("Disconnect succeeded for %s",
				 g_dbus_proxy_get_object_path (G_DBUS_PROXY (conndata->device)));
		}
	} else {
		GDBusProxy *service;
		BluetoothClient *client;
		GVariant *variant;

		variant = g_dbus_proxy_call_finish (proxy, res, &error);
		if (variant == NULL) {
			retval = FALSE;
			g_debug ("Disconnect failed for %s on %s: %s",
				 g_dbus_proxy_get_object_path (proxy),
				 g_dbus_proxy_get_interface_name (proxy),
				 error->message);
			g_error_free (error);
		} else {
			g_debug ("Disconnect succeeded for %s on %s",
				 g_dbus_proxy_get_object_path (proxy),
				 g_dbus_proxy_get_interface_name (proxy));
			g_variant_unref (variant);
			retval = TRUE;
		}
		g_object_unref (proxy);

		client = (BluetoothClient *) g_async_result_get_source_object (G_ASYNC_RESULT (conndata->simple));
		service = get_proxy_for_iface (DEVICE (conndata->device), conndata->services->data, client);
		g_object_unref (client);

		conndata->services = g_list_remove (conndata->services, conndata->services->data);

		g_dbus_proxy_call (G_DBUS_PROXY (service),
				   "Disconnect",
				   g_variant_new ("()"),
				   G_DBUS_CALL_FLAGS_NONE,
				   -1,
				   NULL,
				   (GAsyncReadyCallback) disconnect_callback,
				   conndata);

		return;
	}

	g_simple_async_result_set_op_res_gboolean (conndata->simple, retval);
	g_simple_async_result_complete_in_idle (conndata->simple);

	g_object_unref (proxy);
	g_clear_object (&conndata->device);
	g_object_unref (conndata->simple);
	g_free (conndata);
}

static int
service_to_index (const char *service)
{
	guint i;

	g_return_val_if_fail (service != NULL, -1);

	for (i = 0; i < G_N_ELEMENTS (connectable_interfaces); i++) {
		if (g_str_equal (connectable_interfaces[i], service) != FALSE)
			return i;
	}
	for (i = 0; i < G_N_ELEMENTS (detectable_interfaces); i++) {
		if (g_str_equal (detectable_interfaces[i], service) != FALSE)
			return i + G_N_ELEMENTS (connectable_interfaces);
	}
	g_assert_not_reached ();

	return -1;
}

static int
rev_sort_services (const char *servicea, const char *serviceb)
{
	int a, b;

	a = service_to_index (servicea);
	b = service_to_index (serviceb);

	if (a < b)
		return 1;
	if (a > b)
		return -1;
	return 0;
}

/**
 * bluetooth_client_connect_service:
 * @client: a #BluetoothClient
 * @device: the DBUS path on which to operate
 * @connect: Whether try to connect or disconnect from services on a device
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the connection is complete
 * @user_data: the data to pass to callback function
 *
 * When the connection operation is finished, @callback will be called. You can
 * then call bluetooth_client_connect_service_finish() to get the result of the
 * operation.
 **/
void
bluetooth_client_connect_service (BluetoothClient     *client,
				  const char          *device,
				  gboolean             connect,
				  GCancellable        *cancellable,
				  GAsyncReadyCallback  callback,
				  gpointer             user_data)
{
	GSimpleAsyncResult *simple;
	BluetoothClientPrivate *priv = BLUETOOTH_CLIENT_GET_PRIVATE(client);
	ConnectData *conndata;
	GDBusProxy *proxy;
	GHashTable *table;
	GtkTreeIter iter;
	guint i;
	gboolean res;

	g_return_if_fail (BLUETOOTH_IS_CLIENT (client));
	g_return_if_fail (device != NULL);

	simple = g_simple_async_result_new (G_OBJECT (client),
					    callback,
					    user_data,
					    bluetooth_client_connect_service);
	res = FALSE;

	if (get_iter_from_path (priv->store, &iter, device) == FALSE)
		goto bail;

	if (cancellable != NULL) {
		g_object_set_data_full (G_OBJECT (simple), "cancellable",
					g_object_ref (cancellable), g_object_unref);
		g_object_set_data_full (G_OBJECT (simple), "device",
					g_strdup (device), g_free);
	}

	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
			    BLUETOOTH_COLUMN_PROXY, &proxy,
			    BLUETOOTH_COLUMN_SERVICES, &table,
			    -1);

	/* No proxy? Let's leave it there */
	if (proxy == NULL) {
		if (table != NULL)
			g_hash_table_unref (table);
		g_debug ("Device '%s' has a services table, but no proxy", device);
		goto bail;
	}

	if (connect && table == NULL) {
		g_object_unref (proxy);
		res = TRUE;
		goto bail;
	}

	conndata = g_new0 (ConnectData, 1);
	conndata->simple = simple;

	if (connect) {
		GDBusProxy *service;
		const char *iface_name;

		iface_name = NULL;
		for (i = 0; i < G_N_ELEMENTS (connectable_interfaces); i++) {
			if (g_hash_table_lookup_extended (table, connectable_interfaces[i], NULL, NULL) != FALSE) {
				iface_name = connectable_interfaces[i];
				break;
			}
		}

		if (iface_name == NULL) {
			g_printerr("No supported services on the '%s' device\n", device);
			g_hash_table_unref (table);
			g_free (conndata);
			g_object_unref (proxy);
			goto bail;
		}

		service = get_proxy_for_iface (DEVICE (proxy), iface_name, client);

		g_debug ("Calling 'Connect' on interface %s for %s",
			 iface_name, g_dbus_proxy_get_object_path (service));

		g_dbus_proxy_call (G_DBUS_PROXY (service),
				   "Connect",
				   NULL,
				   G_DBUS_CALL_FLAGS_NONE,
				   -1,
				   NULL,
				   (GAsyncReadyCallback) connect_callback,
				   conndata);
	} else if (table != NULL) {
		GDBusProxy *service;

		conndata->device = g_object_ref (DEVICE (proxy));
		conndata->services = g_hash_table_get_keys (table);
		g_hash_table_unref (table);
		conndata->services = g_list_sort (conndata->services, (GCompareFunc) rev_sort_services);

		service = get_proxy_for_iface (conndata->device, conndata->services->data, client);

		g_debug ("Calling 'Disconnect' on interface %s for %s",
			 (char *) conndata->services->data, g_dbus_proxy_get_object_path (service));

		conndata->services = g_list_remove (conndata->services, conndata->services->data);

		g_dbus_proxy_call (G_DBUS_PROXY (service),
				   "Disconnect",
				   NULL,
				   G_DBUS_CALL_FLAGS_NONE,
				   -1,
				   NULL,
				   (GAsyncReadyCallback) disconnect_callback,
				   conndata);
	} else if (table == NULL) {
		g_debug ("Calling device_call_disconnect() for %s",
			 g_dbus_proxy_get_object_path (proxy));
		device_call_disconnect (DEVICE (proxy),
					NULL,
					(GAsyncReadyCallback) disconnect_callback,
					conndata);
	}

	return;

bail:
	g_simple_async_result_set_op_res_gboolean (simple, res);
	g_simple_async_result_complete_in_idle (simple);
	g_object_unref (simple);
}

/**
 * bluetooth_client_connect_service_finish:
 * @client: a #BluetoothClient
 * @res: a #GAsyncResult
 * @error: a #GError
 *
 * Finishes the connection operation, See bluetooth_client_connect_service().
 *
 * Returns: %TRUE if the connection operation succeeded, %FALSE otherwise.
 **/
gboolean
bluetooth_client_connect_service_finish (BluetoothClient *client,
					 GAsyncResult    *res,
					 GError         **error)
{
	GSimpleAsyncResult *simple;

	simple = (GSimpleAsyncResult *) res;

	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == bluetooth_client_connect_service);

	return g_simple_async_result_get_op_res_gboolean (simple);
}

#define BOOL_STR(x) (x ? "True" : "False")

static void
services_foreach (const char *service, gpointer _value, GString *str)
{
	GEnumClass *eclass;
	GEnumValue *ev;
	BluetoothStatus status = GPOINTER_TO_INT (_value);

	eclass = g_type_class_ref (BLUETOOTH_TYPE_STATUS);
	ev = g_enum_get_value (eclass, status);
	if (ev == NULL)
		g_warning ("Unknown status value %d", status);

	g_string_append_printf (str, "%s (%s) ", service, ev ? ev->value_nick : "unknown");
	g_type_class_unref (eclass);
}

void
bluetooth_client_dump_device (GtkTreeModel *model,
			      GtkTreeIter *iter)
{
	GDBusProxy *proxy;
	char *address, *alias, *name, *icon, **uuids;
	gboolean is_default, paired, trusted, connected, discoverable, discovering, powered, is_adapter;
	GHashTable *services;
	GtkTreeIter parent;
	guint type;

	gtk_tree_model_get (model, iter,
			    BLUETOOTH_COLUMN_ADDRESS, &address,
			    BLUETOOTH_COLUMN_ALIAS, &alias,
			    BLUETOOTH_COLUMN_NAME, &name,
			    BLUETOOTH_COLUMN_TYPE, &type,
			    BLUETOOTH_COLUMN_ICON, &icon,
			    BLUETOOTH_COLUMN_DEFAULT, &is_default,
			    BLUETOOTH_COLUMN_PAIRED, &paired,
			    BLUETOOTH_COLUMN_TRUSTED, &trusted,
			    BLUETOOTH_COLUMN_CONNECTED, &connected,
			    BLUETOOTH_COLUMN_DISCOVERABLE, &discoverable,
			    BLUETOOTH_COLUMN_DISCOVERING, &discovering,
			    BLUETOOTH_COLUMN_POWERED, &powered,
			    BLUETOOTH_COLUMN_SERVICES, &services,
			    BLUETOOTH_COLUMN_UUIDS, &uuids,
			    BLUETOOTH_COLUMN_PROXY, &proxy,
			    -1);
	if (proxy) {
		char *basename;
		basename = g_path_get_basename(g_dbus_proxy_get_object_path(proxy));
		is_adapter = !g_str_has_prefix (basename, "dev_");
		g_free (basename);
	} else {
		is_adapter = !gtk_tree_model_iter_parent (model, &parent, iter);
	}

	if (is_adapter != FALSE) {
		/* Adapter */
		g_print ("Adapter: %s (%s)\n", name, address);
		if (is_default)
			g_print ("\tDefault adapter\n");
		g_print ("\tD-Bus Path: %s\n", proxy ? g_dbus_proxy_get_object_path (proxy) : "(none)");
		g_print ("\tDiscoverable: %s\n", BOOL_STR (discoverable));
		if (discovering)
			g_print ("\tDiscovery in progress\n");
		g_print ("\t%s\n", powered ? "Is powered" : "Is not powered");
	} else {
		/* Device */
		g_print ("Device: %s (%s)\n", alias, address);
		g_print ("\tD-Bus Path: %s\n", proxy ? g_dbus_proxy_get_object_path (proxy) : "(none)");
		g_print ("\tType: %s Icon: %s\n", bluetooth_type_to_string (type), icon);
		g_print ("\tPaired: %s Trusted: %s Connected: %s\n", BOOL_STR(paired), BOOL_STR(trusted), BOOL_STR(connected));
		if (services != NULL) {
			GString *str;

			str = g_string_new (NULL);
			g_hash_table_foreach (services, (GHFunc) services_foreach, str);
			g_print ("\tServices: %s\n", str->str);
			g_string_free (str, TRUE);
		}
		if (uuids != NULL) {
			guint i;
			g_print ("\tUUIDs: ");
			for (i = 0; uuids[i] != NULL; i++)
				g_print ("%s ", uuids[i]);
			g_print ("\n");
		}
	}
	g_print ("\n");

	g_free (alias);
	g_free (address);
	g_free (icon);
	if (proxy != NULL)
		g_object_unref (proxy);
	if (services != NULL)
		g_hash_table_unref (services);
	g_strfreev (uuids);
}

