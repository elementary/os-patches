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
#include <nm-setting-bond.h>
#include <nm-device-bond.h>
#include <nm-utils.h>

#include "applet.h"
#include "applet-device-bond.h"
#include "utils.h"
#include "nm-ui-utils.h"

static void
bond_add_menu_item (NMDevice *device,
                    gboolean multiple_devices,
                    GSList *connections,
                    NMConnection *active,
                    GtkWidget *menu,
                    NMApplet *applet)
{
	char *text;
	GtkWidget *item;

	text = nma_utils_get_connection_device_name (connections->data);
	item = applet_menu_item_create_device_item_helper (device, applet, text);
	g_free (text);

	gtk_widget_set_sensitive (item, FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	if (g_slist_length (connections))
		applet_add_connection_items (device, connections, TRUE, active, NMA_ADD_ACTIVE, menu, applet);

	/* Notify user of unmanaged or unavailable device */
	if (device) {
		item = nma_menu_device_get_menu_item (device, applet, NULL);
		if (item) {
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			gtk_widget_show (item);
		}
	}

	if (!device || !nma_menu_device_check_unusable (device)) {
		if ((!active && g_slist_length (connections)) || (active && g_slist_length (connections) > 1))
			applet_menu_item_add_complex_separator_helper (menu, applet, _("Available"));

		if (g_slist_length (connections))
			applet_add_connection_items (device, connections, TRUE, active, NMA_ADD_INACTIVE, menu, applet);
	}
}

static void
bond_notify_connected (NMDevice *device,
                       const char *msg,
                       NMApplet *applet)
{
	applet_do_notify_with_pref (applet,
	                            _("Connection Established"),
	                            msg ? msg : _("You are now connected to the bonded network."),
	                            "nm-device-wired",
	                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
}

static void
bond_get_icon (NMDevice *device,
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
		*tip = g_strdup_printf (_("Preparing bond connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_CONFIG:
		*tip = g_strdup_printf (_("Configuring bond connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_NEED_AUTH:
		*tip = g_strdup_printf (_("User authentication required for bond connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_IP_CONFIG:
		*tip = g_strdup_printf (_("Requesting address for '%s'..."), id);
		break;
	case NM_DEVICE_STATE_ACTIVATED:
		*out_icon_name = "nm-device-wired";
		*tip = g_strdup_printf (_("Bond connection '%s' active"), id);
		break;
	default:
		break;
	}
}

static gboolean
bond_new_auto_connection (NMDevice *device,
                          gpointer dclass_data,
                          AppletNewAutoConnectionCallback callback,
                          gpointer callback_data)
{
	return FALSE;
}


static gboolean
bond_get_secrets (SecretsRequest *req, GError **error)
{
	/* No 802.1x or PPPoE possible yet on bonds */
	return FALSE;
}

NMADeviceClass *
applet_device_bond_get_class (NMApplet *applet)
{
	NMADeviceClass *dclass;

	dclass = g_slice_new0 (NMADeviceClass);
	if (!dclass)
		return NULL;

	dclass->new_auto_connection = bond_new_auto_connection;
	dclass->add_menu_item = bond_add_menu_item;
	dclass->notify_connected = bond_notify_connected;
	dclass->get_icon = bond_get_icon;
	dclass->get_secrets = bond_get_secrets;

	return dclass;
}
