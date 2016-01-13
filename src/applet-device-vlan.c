/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Wireless Applet -- Display wireless access points and allow user control
 *
 * Dan Williams <dcbw@redhat.com>
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
 * (C) Copyright 2008 Red Hat, Inc.
 * (C) Copyright 2008 Novell, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <nm-device.h>
#include <nm-setting-connection.h>
#include <nm-setting-wired.h>
#include <nm-setting-vlan.h>
#include <nm-device-ethernet.h>
#include <nm-device-vlan.h>
#include <nm-utils.h>

#include "applet.h"
#include "applet-device-vlan.h"
#include "nm-ui-utils.h"

static NMDevice *
find_device_by_iface (const char *iface, const GPtrArray *devices)
{
	NMDevice *candidate;
	int i;

	for (i = 0; i < devices->len; i++) {
		candidate = devices->pdata[i];

		if (!g_strcmp0 (iface, nm_device_get_iface (candidate)))
			return candidate;
	}
	return NULL;
}

static NMDevice *
find_device_by_mac (const GByteArray *mac, const GPtrArray *devices)
{
	NMDevice *candidate, *device = NULL;
	char *vlan_hw_address, *candidate_hw_address;
	int i;

	vlan_hw_address = nm_utils_hwaddr_ntoa_len (mac->data, mac->len);

	for (i = 0; i < devices->len && device == NULL; i++) {
		candidate = devices->pdata[i];

		if (!g_object_class_find_property (G_OBJECT_GET_CLASS (candidate),
		                                   "hw-address"))
			continue;

		g_object_get (G_OBJECT (candidate),
		              "hw-address", &candidate_hw_address,
		              NULL);
		if (!g_strcmp0 (vlan_hw_address, candidate_hw_address))
			device = candidate;
		g_free (candidate_hw_address);
	}
	g_free (vlan_hw_address);

	return device;
}

static NMDevice *
find_vlan_parent (GSList *connections, NMApplet *applet)
{
	const GPtrArray *devices;
	NMDevice *parent_device;
	GSList *iter;

	devices = nm_client_get_devices (applet->nm_client);
	if (!devices)
		return NULL;

	for (iter = connections; iter; iter = iter->next) {
		NMConnection *connection = iter->data;
		NMSettingVlan *s_vlan;
		const char *parent;

		s_vlan = nm_connection_get_setting_vlan (connection);
		g_return_val_if_fail (s_vlan != NULL, NULL);

		parent = nm_setting_vlan_get_parent (s_vlan);
		if (parent && nm_utils_iface_valid_name (parent)) {
			parent_device = find_device_by_iface (parent, devices);
		} else {
			NMSettingConnection *s_con;
			NMSetting *s_hw;
			const char *type;
			GByteArray *mac;

			s_con = nm_connection_get_setting_connection (connection);
			type = nm_setting_connection_get_connection_type (s_con);
			s_hw = nm_connection_get_setting_by_name (connection, type);
			if (!s_hw) {
				g_warn_if_reached ();
				continue;
			}

			if (!g_object_class_find_property (G_OBJECT_GET_CLASS (s_hw),
			                                   "mac-address"))
				continue;

			g_object_get (G_OBJECT (s_hw),
			              "mac-address", &mac,
			              NULL);
			if (mac) {
				parent_device = find_device_by_mac (mac, devices);
				g_byte_array_unref (mac);
			} else
				parent_device = NULL;
		}
		
		if (parent_device)
			return parent_device;
	}

	return NULL;
}

static void
vlan_add_menu_item (NMDevice *device,
                    gboolean multiple_devices,
                    GSList *connections,
                    NMConnection *active,
                    GtkWidget *menu,
                    NMApplet *applet)
{
	char *text;
	GtkWidget *item;
	gboolean carrier = TRUE;

	text = nma_utils_get_connection_device_name (connections->data);
	item = applet_menu_item_create_device_item_helper (device, applet, text);
	g_free (text);

	/* If the VLAN device exists, check its carrier */
	if (device && nm_device_get_capabilities (device) & NM_DEVICE_CAP_CARRIER_DETECT)
		carrier = nm_device_vlan_get_carrier (NM_DEVICE_VLAN (device));
	else {
		NMDevice *parent;

		/* If we can find its parent, check the parent's carrier */
		parent = find_vlan_parent (connections, applet);

		if (parent && nm_device_get_capabilities (parent) & NM_DEVICE_CAP_CARRIER_DETECT)
			g_object_get (G_OBJECT (parent), "carrier", &carrier, NULL);
	} /* else fall back to assuming carrier is present */		

	gtk_widget_set_sensitive (item, FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	if (g_slist_length (connections))
		applet_add_connection_items (device, connections, carrier, active, NMA_ADD_ACTIVE, menu, applet);

	/* Notify user of unmanaged or unavailable device */
	if (device) {
		item = nma_menu_device_get_menu_item (device, applet, carrier ? NULL : _("disconnected"));
		if (item) {
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			gtk_widget_show (item);
		}
	}

	if (!device || !nma_menu_device_check_unusable (device)) {
		if ((!active && g_slist_length (connections)) || (active && g_slist_length (connections) > 1))
			applet_menu_item_add_complex_separator_helper (menu, applet, _("Available"));

		if (g_slist_length (connections))
			applet_add_connection_items (device, connections, carrier, active, NMA_ADD_INACTIVE, menu, applet);
	}
}

static void
vlan_notify_connected (NMDevice *device,
                       const char *msg,
                       NMApplet *applet)
{
	applet_do_notify_with_pref (applet,
	                            _("Connection Established"),
	                            msg ? msg : _("You are now connected to the VLAN."),
	                            "nm-device-wired",
	                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
}

static void
vlan_get_icon (NMDevice *device,
               NMDeviceState state,
               NMConnection *connection,
               GdkPixbuf **out_pixbuf,
               const char **out_icon_name,
               char **tip,
               NMApplet *applet)
{
	NMSettingConnection *s_con;
	const char *id;

	id = nm_device_get_iface (NM_DEVICE (device));
	if (connection) {
		s_con = NM_SETTING_CONNECTION (nm_connection_get_setting (connection, NM_TYPE_SETTING_CONNECTION));
		id = nm_setting_connection_get_id (s_con);
	}

	switch (state) {
	case NM_DEVICE_STATE_PREPARE:
		*tip = g_strdup_printf (_("Preparing VLAN connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_CONFIG:
		*tip = g_strdup_printf (_("Configuring VLAN connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_NEED_AUTH:
		*tip = g_strdup_printf (_("User authentication required for VLAN connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_IP_CONFIG:
		*tip = g_strdup_printf (_("Requesting address for '%s'..."), id);
		break;
	case NM_DEVICE_STATE_ACTIVATED:
		*out_icon_name = "nm-device-wired";
		*tip = g_strdup_printf (_("VLAN connection '%s' active"), id);
		break;
	default:
		break;
	}
}

static gboolean
vlan_new_auto_connection (NMDevice *device,
                          gpointer dclass_data,
                          AppletNewAutoConnectionCallback callback,
                          gpointer callback_data)
{
	return FALSE;
}

static gboolean
vlan_get_secrets (SecretsRequest *req, GError **error)
{
	/* No 802.1x or PPPoE possible yet on VLANs */
	return FALSE;
}

NMADeviceClass *
applet_device_vlan_get_class (NMApplet *applet)
{
	NMADeviceClass *dclass;

	dclass = g_slice_new0 (NMADeviceClass);
	if (!dclass)
		return NULL;

	dclass->new_auto_connection = vlan_new_auto_connection;
	dclass->add_menu_item = vlan_add_menu_item;
	dclass->notify_connected = vlan_notify_connected;
	dclass->get_icon = vlan_get_icon;
	dclass->get_secrets = vlan_get_secrets;

	return dclass;
}
