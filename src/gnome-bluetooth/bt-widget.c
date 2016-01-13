/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 *  NetworkManager Applet
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
 * (C) Copyright 2009 - 2012 Red Hat, Inc.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <net/ethernet.h>
#include <netinet/ether.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>
#include <dbus/dbus.h>
#include <bluetooth-plugin.h>
#include <bluetooth-client.h>
#include <nm-remote-settings.h>
#include <nm-remote-connection.h>

#include "nma-bt-device.h"

typedef struct {
	NmaBtDevice *device;
	BluetoothClient *btclient;

	GSList *sigids;

	GtkWidget *pan_button;
	guint pan_toggled_id;

	GtkWidget *dun_button;
	guint dun_toggled_id;
	gboolean powered;

	GtkWidget *hbox;
	GtkWidget *status;
	GtkWidget *spinner;
} WidgetInfo;

/***************************************************************/

static GHashTable *devices = NULL;

static NmaBtDevice *
get_device (const char *bdaddr)
{
	if (G_UNLIKELY (devices == NULL))
		devices = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

	return g_hash_table_lookup (devices, bdaddr);
}

static void
add_device (NmaBtDevice *device)
{
	const char *bdaddr = nma_bt_device_get_bdaddr (device);

	if (get_device (bdaddr)) {
		g_warning ("%s already exists in the device table!", bdaddr);
		return;
	}

	g_hash_table_insert (devices, (gpointer) bdaddr, device);
}

static void
remove_device (NmaBtDevice *device)
{
	g_hash_table_remove (devices, device);
}

/***************************************************************/

static void
get_capabilities (const char *bdaddr,
                  const char **uuids,
                  gboolean *pan,
                  gboolean *dun)
{
	guint i;

	g_return_if_fail (bdaddr != NULL);
	g_return_if_fail (uuids != NULL);
	g_return_if_fail (pan != NULL);
	g_return_if_fail (*pan == FALSE);
	g_return_if_fail (dun != NULL);
	g_return_if_fail (*dun == FALSE);

	for (i = 0; uuids && uuids[i] != NULL; i++) {
		g_message ("has_config_widget %s %s", bdaddr, uuids[i]);
		if (g_str_equal (uuids[i], "NAP"))
			*pan = TRUE;
		if (g_str_equal (uuids[i], "DialupNetworking"))
			*dun = TRUE;
	}
}

static gboolean
has_config_widget (const char *bdaddr, const char **uuids)
{
	gboolean pan = FALSE, dun = FALSE;

	get_capabilities (bdaddr, uuids, &pan, &dun);
	return pan || dun;
}

/***************************************************************/

static gboolean
get_device_iter (GtkTreeModel *model, const char *bdaddr, GtkTreeIter *out_iter)
{
	GtkTreeIter iter;
	gboolean valid, child_valid;

	g_return_val_if_fail (model != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (bdaddr != NULL, FALSE);
	g_return_val_if_fail (out_iter != NULL, FALSE);

	/* Loop over adapters */
	valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid) {
		/* Loop over devices */
		if (gtk_tree_model_iter_n_children (model, &iter)) {
			child_valid = gtk_tree_model_iter_children (model, out_iter, &iter);
			while (child_valid) {
				char *addr = NULL;
				gboolean good;

				gtk_tree_model_get (model, out_iter, BLUETOOTH_COLUMN_ADDRESS, &addr, -1);
				good = (addr && !strcasecmp (addr, bdaddr));
				g_free (addr);

				if (good)
					return TRUE;  /* found */

				child_valid = gtk_tree_model_iter_next (model, out_iter);
			}
		}

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return FALSE;
}

/*******************************************************************/

static void
pan_button_toggled (GtkToggleButton *button, WidgetInfo *info)
{
	nma_bt_device_set_pan_enabled (info->device, gtk_toggle_button_get_active (button));
}

static void
dun_button_toggled (GtkToggleButton *button, WidgetInfo *info)
{
	GtkWidget *parent;

	/* Update the toplevel for the mobile wizard now that the widget is
	 * realized.
	 */
	parent = gtk_widget_get_toplevel (info->hbox);
	if (gtk_widget_is_toplevel (parent))
		nma_bt_device_set_parent_window (info->device, GTK_WINDOW (parent));

	nma_bt_device_set_dun_enabled (info->device, gtk_toggle_button_get_active (button));
}

static void
widget_info_destroy (gpointer data)
{
	WidgetInfo *info = data;
	GSList *iter;

	g_message ("%s: NM Bluetooth widget info being destroyed", __func__);

	g_signal_handlers_disconnect_matched (info->btclient,
	                                      G_SIGNAL_MATCH_DATA, 0, 0, NULL,
	                                      NULL, info);
	g_object_unref (info->btclient);

	for (iter = info->sigids; iter; iter = g_slist_next (iter))
		g_signal_handler_disconnect (info->device, GPOINTER_TO_UINT (iter->data));
	g_slist_free (info->sigids);

	g_object_unref (info->device);

	memset (info, 0, sizeof (WidgetInfo));
	g_free (info);
}

static void
set_dun_button_sensitive (WidgetInfo *info, gboolean sensitive)
{
	gtk_widget_set_sensitive (info->dun_button,
	                          sensitive && info->powered && !nma_bt_device_get_busy (info->device));
}

static void
default_adapter_powered_changed (GObject *object,
                                 GParamSpec *pspec,
                                 WidgetInfo *info)
{
	gboolean powered = TRUE;

	g_object_get (G_OBJECT (info->btclient), "default-adapter-powered", &powered, NULL);
	g_message ("Default Bluetooth adapter is %s", powered ? "powered" : "switched off");

	/* If the default adapter isn't powered we can't inspect the device
	 * and create a connection for it.
	 */
	info->powered = powered;
	if (powered) {
		if (info->dun_button) {
			gtk_label_set_text (GTK_LABEL (info->status), NULL); 
			set_dun_button_sensitive (info, TRUE);
		}
	} else {
		/* powered only matters for DUN */
		if (info->dun_button) {
			nma_bt_device_cancel_dun (info->device);
			/* Can't toggle the DUN button unless the adapter is powered */
			set_dun_button_sensitive (info, FALSE);
		}
	}
}

static void
default_adapter_changed (GObject *gobject,
                         GParamSpec *pspec,
                         WidgetInfo *info)
{
	char *adapter = NULL;

	g_object_get (G_OBJECT (gobject), "default-adapter", &adapter, NULL);
	g_message ("Default Bluetooth adapter changed: %s", adapter ? adapter : "(none)");
	g_free (adapter);

	default_adapter_powered_changed (G_OBJECT (info->btclient), NULL, info);
}

static void
device_pan_enabled_cb (NmaBtDevice *device, GParamSpec *pspec, WidgetInfo *info)
{
	g_signal_handler_block (info->pan_button, info->pan_toggled_id);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (info->pan_button),
	                              nma_bt_device_get_pan_enabled (device));
	g_signal_handler_unblock (info->pan_button, info->pan_toggled_id);
}

static void
device_dun_enabled_cb (NmaBtDevice *device, GParamSpec *pspec, WidgetInfo *info)
{
	g_signal_handler_block (info->dun_button, info->dun_toggled_id);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (info->dun_button),
	                              nma_bt_device_get_dun_enabled (device));
	g_signal_handler_unblock (info->dun_button, info->dun_toggled_id);
}

static void
device_busy_cb (NmaBtDevice *device, GParamSpec *pspec, WidgetInfo *info)
{
	gboolean busy = nma_bt_device_get_busy (device);

	if (info->pan_button)
		gtk_widget_set_sensitive (info->pan_button, !busy);
	if (info->dun_button)
		set_dun_button_sensitive (info, !busy);

	if (busy) {
		if (!info->spinner) {
			info->spinner = gtk_spinner_new ();
			gtk_box_pack_start (GTK_BOX (info->hbox), info->spinner, FALSE, FALSE, 6);
		}
		gtk_spinner_start (GTK_SPINNER (info->spinner));
		gtk_widget_show (info->spinner);
	} else {
		if (info->spinner) {
			gtk_spinner_stop (GTK_SPINNER (info->spinner));
			gtk_widget_destroy (info->spinner);
			info->spinner = NULL;
		}
	}
}

static void
device_status_cb (NmaBtDevice *device, GParamSpec *pspec, WidgetInfo *info)
{
	gtk_label_set_text (GTK_LABEL (info->status), nma_bt_device_get_status (device));
}

static gboolean
nm_is_running (void)
{
	DBusConnection *bus;
	DBusError error;
	gboolean running = FALSE;

	dbus_error_init (&error);
	bus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set (&error)) {
		g_message (_("Bluetooth configuration not possible (failed to connect to D-Bus: (%s) %s)."),
		           error.name, error.message);
		dbus_error_free (&error);
		return FALSE;
	}

	dbus_error_init (&error);
	running = dbus_bus_name_has_owner (bus, NM_DBUS_SERVICE, &error);
	if (dbus_error_is_set (&error)) {
		g_message (_("Bluetooth configuration not possible (error finding NetworkManager: (%s) %s)."),
		           error.name, error.message);
	}

	dbus_connection_unref (bus);
	return running;
}

static GtkWidget *
get_config_widgets (const char *bdaddr, const char **uuids)
{
	WidgetInfo *info;
	NmaBtDevice *device;
	GtkWidget *vbox, *hbox;
	gboolean pan = FALSE, dun = FALSE, busy;
	GtkTreeIter iter;
	BluetoothClient *btclient;
	GtkTreeModel *btmodel;
	guint id;

	/* Don't allow configuration if NM isn't running; it just confuses people
	 * if they see the checkboxes but the configuration doesn't seem to have
	 * any visible effect since they aren't running NM/nm-applet.
	 */
	if (!nm_is_running ())
		return NULL;

	get_capabilities (bdaddr, uuids, &pan, &dun);
	if (!pan && !dun)
		return NULL;

	/* BluetoothClient setup */
	btclient = bluetooth_client_new ();
	btmodel = bluetooth_client_get_model (btclient);
	g_assert (btmodel);

	device = get_device (bdaddr);
	if (!device) {
		const char *op = NULL;
		gpointer proxy;
		char *alias;

		if (!get_device_iter (btmodel, bdaddr, &iter)) {
			g_warning ("%s: failed to retrieve device %s from gnome-bluetooth!", __func__, bdaddr);
			g_object_unref (btmodel);
			g_object_unref (btclient);
			return NULL;
		}

		gtk_tree_model_get (btmodel, &iter,
		                    BLUETOOTH_COLUMN_ALIAS, &alias,
		                    BLUETOOTH_COLUMN_PROXY, &proxy,
		                    -1);
		g_assert (proxy);

		/* At some point gnome-bluetooth switched to gdbus, so we don't know
		* if the proxy will be a DBusGProxy (dbus-glib) or a GDBusProxy (gdbus).
		*/
		if (G_IS_DBUS_PROXY (proxy))
			op = g_dbus_proxy_get_object_path (G_DBUS_PROXY (proxy));
		else if (DBUS_IS_G_PROXY (proxy))
			op = dbus_g_proxy_get_path (DBUS_G_PROXY (proxy));
		else
			g_assert_not_reached ();

		device = nma_bt_device_new (bdaddr, alias, op, pan, dun);
		g_free (alias);
		g_object_unref (proxy);

		if (!device) {
			g_warning ("%s: failed to create Bluetooth proxy object!", bdaddr);
			g_object_unref (btmodel);
			g_object_unref (btclient);
			return NULL;
		}

		add_device (device);
	}

	info = g_malloc0 (sizeof (WidgetInfo));
	info->device = g_object_ref (device);
	info->btclient = btclient;

	g_signal_connect (G_OBJECT (btclient), "notify::default-adapter",
	                  G_CALLBACK (default_adapter_changed), info);
	g_signal_connect (G_OBJECT (btclient), "notify::default-adapter-powered",
	                  G_CALLBACK (default_adapter_powered_changed), info);

	id = g_signal_connect (device, "notify::" NMA_BT_DEVICE_PAN_ENABLED,
	                       G_CALLBACK (device_pan_enabled_cb), info);
	info->sigids = g_slist_prepend (info->sigids, GUINT_TO_POINTER (id));

	id = g_signal_connect (device, "notify::" NMA_BT_DEVICE_DUN_ENABLED,
	                       G_CALLBACK (device_dun_enabled_cb), info);
	info->sigids = g_slist_prepend (info->sigids, GUINT_TO_POINTER (id));

	id = g_signal_connect (device, "notify::" NMA_BT_DEVICE_BUSY,
	                       G_CALLBACK (device_busy_cb), info);
	info->sigids = g_slist_prepend (info->sigids, GUINT_TO_POINTER (id));

	id = g_signal_connect (device, "notify::" NMA_BT_DEVICE_STATUS,
	                       G_CALLBACK (device_status_cb), info);
	info->sigids = g_slist_prepend (info->sigids, GUINT_TO_POINTER (id));

	/* UI setup */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	g_object_set_data_full (G_OBJECT (vbox), "info", info, widget_info_destroy);

	busy = nma_bt_device_get_busy (device);

	if (pan) {
		info->pan_button = gtk_check_button_new_with_label (_("Use your mobile phone as a network device (PAN/NAP)"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (info->pan_button),
		                              nma_bt_device_get_pan_enabled (device));
		info->pan_toggled_id = g_signal_connect (G_OBJECT (info->pan_button), "toggled", G_CALLBACK (pan_button_toggled), info);
		gtk_box_pack_start (GTK_BOX (vbox), info->pan_button, FALSE, TRUE, 6);
		gtk_widget_set_sensitive (info->pan_button, !busy);
	}

	if (dun) {
		info->dun_button = gtk_check_button_new_with_label (_("Access the Internet using your mobile phone (DUN)"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (info->dun_button),
		                              nma_bt_device_get_dun_enabled (device));
		info->dun_toggled_id = g_signal_connect (G_OBJECT (info->dun_button), "toggled", G_CALLBACK (dun_button_toggled), info);
		gtk_box_pack_start (GTK_BOX (vbox), info->dun_button, FALSE, TRUE, 6);
		set_dun_button_sensitive (info, !busy);
	}

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 6);

	/* Spinner's hbox */
	info->hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (hbox), info->hbox, FALSE, FALSE, 0);

	device_busy_cb (device, NULL, info);

	/* Status label */
	info->status = gtk_label_new (nma_bt_device_get_status (device));
	gtk_label_set_max_width_chars (GTK_LABEL (info->status), 80);
	gtk_label_set_line_wrap (GTK_LABEL (info->status), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), info->status, FALSE, TRUE, 6);

	default_adapter_powered_changed (G_OBJECT (info->btclient), NULL, info);

	return vbox;
}

/**************************************************************/

typedef struct {
	NMRemoteSettings *settings;
	GByteArray *bdaddr;
	char *str_bdaddr;
	guint timeout_id;
} RemoveInfo;

static void
remove_cleanup (RemoveInfo *info)
{
	g_object_unref (info->settings);
	g_byte_array_free (info->bdaddr, TRUE);
	g_free (info->str_bdaddr);
	memset (info, 0, sizeof (RemoveInfo));
	g_free (info);
}

static void
delete_cb (NMRemoteConnection *connection, GError *error, gpointer user_data)
{
	if (error) {
		g_warning ("Error deleting connection: (%d) %s",
		           error ? error->code : -1,
		           error && error->message ? error->message : "(unknown)");
	}
}

static void
remove_connections_read (NMRemoteSettings *settings, gpointer user_data)
{
	RemoveInfo *info = user_data;
	GSList *list, *iter;

	g_source_remove (info->timeout_id);

	g_message ("Removing Bluetooth connections for %s", info->str_bdaddr);

	list = nm_remote_settings_list_connections (settings);
	for (iter = list; iter != NULL; iter = g_slist_next (iter)) {
		NMConnection *connection = iter->data;
		NMSettingBluetooth *s_bt;
		const GByteArray *tmp;

		s_bt = nm_connection_get_setting_bluetooth (connection);
		if (s_bt) {
			tmp = nm_setting_bluetooth_get_bdaddr (s_bt);
			if (tmp && memcmp (tmp->data, info->bdaddr->data, tmp->len) == 0)
				nm_remote_connection_delete (NM_REMOTE_CONNECTION (connection), delete_cb, NULL);
		}
	}
	g_slist_free (list);

	remove_cleanup (info);
}

static gboolean
remove_timeout (gpointer user_data)
{
	RemoveInfo *info = user_data;

	g_message ("Timed out removing Bluetooth connections for %s", info->str_bdaddr);
	remove_cleanup (info);
	return FALSE;
}

static void
device_removed (const char *bdaddr)
{
	GError *error = NULL;
	DBusGConnection *bus;
	RemoveInfo *info;
	struct ether_addr *addr;
	NmaBtDevice *device;

	g_message ("Device '%s' was removed; deleting connections", bdaddr);

	/* Remove any connections associated with the deleted device */

	addr = ether_aton (bdaddr);
	if (!addr) {
		g_warning ("Failed to convert Bluetooth address '%s'", bdaddr);
		return;
	}

	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error || !bus) {
		g_warning ("%s: failed to get a connection to D-Bus! %s", __func__,
		           error ? error->message : "(unknown)");
		g_clear_error (&error);
		return;
	}

	info = g_malloc0 (sizeof (RemoveInfo));
	info->settings = nm_remote_settings_new (bus);

	info->bdaddr = g_byte_array_sized_new (ETH_ALEN);
	g_byte_array_append (info->bdaddr, (const guint8 *) addr->ether_addr_octet, ETH_ALEN);

	info->str_bdaddr = g_strdup (bdaddr);
	info->timeout_id = g_timeout_add_seconds (15, remove_timeout, info);

	g_signal_connect (info->settings,
	                  NM_REMOTE_SETTINGS_CONNECTIONS_READ,
	                  G_CALLBACK (remove_connections_read),
	                  info);

	dbus_g_connection_unref (bus);

	/* Kill the device */
	device = get_device (bdaddr);
	if (device)
		remove_device (device);
}

/**************************************************************/

static GbtPluginInfo plugin_info = {
	"network-manager-applet",
	has_config_widget,
	get_config_widgets,
	device_removed
};

GBT_INIT_PLUGIN(plugin_info)

