/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
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
 * (C) Copyright 2013 Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <nm-device.h>
#include <nm-setting-connection.h>
#include <nm-setting-infiniband.h>
#include <nm-device-infiniband.h>
#include <nm-utils.h>

#include "applet.h"
#include "applet-device-infiniband.h"
#include "nm-ui-utils.h"

#define DEFAULT_INFINIBAND_NAME _("Auto InfiniBand")

static gboolean
infiniband_new_auto_connection (NMDevice *device,
                                gpointer dclass_data,
                                AppletNewAutoConnectionCallback callback,
                                gpointer callback_data)
{
	NMConnection *connection;
	NMSettingInfiniband *s_infiniband = NULL;
	NMSettingConnection *s_con;
	char *uuid;

	connection = nm_connection_new ();

	s_infiniband = NM_SETTING_INFINIBAND (nm_setting_infiniband_new ());
	nm_connection_add_setting (connection, NM_SETTING (s_infiniband));

	s_con = NM_SETTING_CONNECTION (nm_setting_connection_new ());
	uuid = nm_utils_uuid_generate ();
	g_object_set (s_con,
	              NM_SETTING_CONNECTION_ID, DEFAULT_INFINIBAND_NAME,
	              NM_SETTING_CONNECTION_TYPE, NM_SETTING_INFINIBAND_SETTING_NAME,
	              NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
	              NM_SETTING_CONNECTION_UUID, uuid,
	              NULL);
	g_free (uuid);

	nm_connection_add_setting (connection, NM_SETTING (s_con));

	(*callback) (connection, TRUE, FALSE, callback_data);
	return TRUE;
}

static void
infiniband_add_menu_item (NMDevice *device,
                          gboolean multiple_devices,
                          GSList *connections,
                          NMConnection *active,
                          GtkWidget *menu,
                          NMApplet *applet)
{
	char *text;
	GtkWidget *item;
	gboolean carrier = TRUE;

	if (multiple_devices) {
		const char *desc;

		desc = nma_utils_get_device_description (device);

		if (g_slist_length (connections) > 1)
			text = g_strdup_printf (_("InfiniBand Networks (%s)"), desc);
		else
			text = g_strdup_printf (_("InfiniBand Network (%s)"), desc);
	} else {
		if (g_slist_length (connections) > 1)
			text = g_strdup (_("InfiniBand Networks"));
		else
			text = g_strdup (_("InfiniBand Network"));
	}

	item = applet_menu_item_create_device_item_helper (device, applet, text);
	g_free (text);

	/* Only dim the item if the device supports carrier detection AND
	 * we know it doesn't have a link.
	 */
 	if (nm_device_get_capabilities (device) & NM_DEVICE_CAP_CARRIER_DETECT)
		carrier = nm_device_infiniband_get_carrier (NM_DEVICE_INFINIBAND (device));

	gtk_widget_set_sensitive (item, FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	if (g_slist_length (connections))
		applet_add_connection_items (device, connections, carrier, active, NMA_ADD_ACTIVE, menu, applet);

	/* Notify user of unmanaged or unavailable device */
	item = nma_menu_device_get_menu_item (device, applet, carrier ? NULL : _("disconnected"));
	if (item) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
	}

	if (!nma_menu_device_check_unusable (device)) {
		if ((!active && g_slist_length (connections)) || (active && g_slist_length (connections) > 1))
			applet_menu_item_add_complex_separator_helper (menu, applet, _("Available"));

		if (g_slist_length (connections))
			applet_add_connection_items (device, connections, carrier, active, NMA_ADD_INACTIVE, menu, applet);
		else
			applet_add_default_connection_item (device, DEFAULT_INFINIBAND_NAME, carrier, menu, applet);
	}
}

static void
infiniband_notify_connected (NMDevice *device,
                             const char *msg,
                             NMApplet *applet)
{
	applet_do_notify_with_pref (applet,
	                            _("Connection Established"),
	                            msg ? msg : _("You are now connected to the InfiniBand network."),
	                            "nm-device-wired",
	                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
}

static void
infiniband_get_icon (NMDevice *device,
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
		s_con = nm_connection_get_setting_connection (connection);
		id = nm_setting_connection_get_id (s_con);
	}

	switch (state) {
	case NM_DEVICE_STATE_PREPARE:
		*tip = g_strdup_printf (_("Preparing InfiniBand connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_CONFIG:
		*tip = g_strdup_printf (_("Configuring InfiniBand connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_NEED_AUTH:
		*tip = g_strdup_printf (_("User authentication required for InfiniBand connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_IP_CONFIG:
		*tip = g_strdup_printf (_("Requesting address for '%s'..."), id);
		break;
	case NM_DEVICE_STATE_ACTIVATED:
		*out_icon_name = "nm-device-wired";
		*tip = g_strdup_printf (_("InfiniBand connection '%s' active"), id);
		break;
	default:
		break;
	}
}


static gboolean
infiniband_get_secrets (SecretsRequest *req, GError **error)
{
	/* No 802.1X possible yet on InfiniBand */
	return FALSE;
}

NMADeviceClass *
applet_device_infiniband_get_class (NMApplet *applet)
{
	NMADeviceClass *dclass;

	dclass = g_slice_new0 (NMADeviceClass);
	if (!dclass)
		return NULL;

	dclass->new_auto_connection = infiniband_new_auto_connection;
	dclass->add_menu_item = infiniband_add_menu_item;
	dclass->notify_connected = infiniband_notify_connected;
	dclass->get_icon = infiniband_get_icon;
	dclass->get_secrets = infiniband_get_secrets;

	return dclass;
}
