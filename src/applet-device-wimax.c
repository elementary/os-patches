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
 * (C) Copyright 2008 - 2012 Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <nm-device.h>
#include <nm-setting-connection.h>
#include <nm-setting-wimax.h>
#include <nm-device-wimax.h>
#include <nm-utils.h>

#include "applet.h"
#include "applet-device-wimax.h"
#include "applet-dialogs.h"
#include "nma-marshal.h"
#include "mb-menu-item.h"
#include "nm-ui-utils.h"

#define ACTIVE_NSP_TAG "active-nsp"

typedef struct {
	NMApplet *applet;
	NMDevice *device;
	NMConnection *connection;
	NMWimaxNsp *nsp;
} WimaxMenuItemInfo;

static void
wimax_menu_item_info_destroy (gpointer data)
{
	WimaxMenuItemInfo *info = data;

	g_object_unref (G_OBJECT (info->device));
	if (info->connection)
		g_object_unref (info->connection);
	g_object_unref (info->nsp);

	g_slice_free (WimaxMenuItemInfo, data);
}

static gboolean
wimax_new_auto_connection (NMDevice *device,
                           gpointer dclass_data,
                           AppletNewAutoConnectionCallback callback,
                           gpointer callback_data)
{
	WimaxMenuItemInfo *info = dclass_data;
	NMConnection *connection;
	NMSettingWimax *s_wimax = NULL;
	NMSettingConnection *s_con;
	char *uuid;
	const char *nsp_name;

	nsp_name = nm_wimax_nsp_get_name (info->nsp);

	connection = nm_connection_new ();

	s_wimax = NM_SETTING_WIMAX (nm_setting_wimax_new ());
	g_object_set (s_wimax,
	              NM_SETTING_WIMAX_NETWORK_NAME, nsp_name,
	              NULL);
	nm_connection_add_setting (connection, NM_SETTING (s_wimax));

	s_con = NM_SETTING_CONNECTION (nm_setting_connection_new ());
	uuid = nm_utils_uuid_generate ();
	g_object_set (s_con,
				  NM_SETTING_CONNECTION_ID, nsp_name,
				  NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIMAX_SETTING_NAME,
				  NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
				  NM_SETTING_CONNECTION_UUID, uuid,
				  NULL);
	g_free (uuid);

	nm_connection_add_setting (connection, NM_SETTING (s_con));

	(*callback) (connection, TRUE, FALSE, callback_data);
	return TRUE;
}

static void
wimax_menu_item_activate (GtkMenuItem *item, gpointer user_data)
{
	WimaxMenuItemInfo *info = (WimaxMenuItemInfo *) user_data;
	const char *specific_object = NULL;

	if (info->nsp)
		specific_object = nm_object_get_path (NM_OBJECT (info->nsp));
	applet_menu_item_activate_helper (info->device,
	                                  info->connection,
	                                  specific_object,
	                                  info->applet,
	                                  user_data);
}

static guint32
nsp_type_to_mb_state (NMWimaxNspNetworkType nsp_type)
{
	switch (nsp_type) {
	case NM_WIMAX_NSP_NETWORK_TYPE_HOME:
	case NM_WIMAX_NSP_NETWORK_TYPE_PARTNER:
		return MB_STATE_HOME;
	case NM_WIMAX_NSP_NETWORK_TYPE_ROAMING_PARTNER:
		return MB_STATE_ROAMING;
	default:
		break;
	}

	return MB_STATE_UNKNOWN;
}

static GtkWidget *
new_nsp_menu_item (NMDeviceWimax *device,
                   NMConnection *connection,
                   gboolean active,
                   NMWimaxNsp *nsp,
                   NMApplet *applet)
{
	GtkWidget *item;
	WimaxMenuItemInfo *info;

	g_return_val_if_fail (nsp != NULL, NULL);

	item = nm_mb_menu_item_new (nm_wimax_nsp_get_name (nsp),
		                        nm_wimax_nsp_get_signal_quality (nsp),
		                        NULL,
		                        active,
		                        MB_TECH_WIMAX,
		                        nsp_type_to_mb_state (nm_wimax_nsp_get_network_type (nsp)),
		                        TRUE,
		                        applet);
	gtk_widget_set_sensitive (GTK_WIDGET (item), TRUE);

	info = g_slice_new0 (WimaxMenuItemInfo);
	info->applet = applet;
	info->device = g_object_ref (G_OBJECT (device));
	info->connection = connection ? g_object_ref (connection) : NULL;
	info->nsp = g_object_ref (nsp);

	g_signal_connect_data (item, "activate",
	                       G_CALLBACK (wimax_menu_item_activate),
	                       info,
	                       (GClosureNotify) wimax_menu_item_info_destroy, 0);

	return item;
}

static NMConnection *
get_connection_for_nsp (GSList *connections, NMWimaxNsp *nsp)
{
	GSList *iter;
	const char *nsp_name, *candidate_name;

	nsp_name = nm_wimax_nsp_get_name (nsp);
	for (iter = connections; iter; iter = g_slist_next (iter)) {
		NMConnection *candidate = NM_CONNECTION (iter->data);
		NMSettingWimax *s_wimax;

		s_wimax = nm_connection_get_setting_wimax (candidate);
		if (s_wimax) {
			candidate_name = nm_setting_wimax_get_network_name (s_wimax);
			if (g_strcmp0 (nsp_name, candidate_name) == 0)
				return candidate;
		}
	}
	return NULL;
}

static gint
sort_nsps (gconstpointer a, gconstpointer b)
{
	const char *name_a = NULL, *name_b = NULL;

	if (a)
		name_a = nm_wimax_nsp_get_name (NM_WIMAX_NSP (a));
	if (b)
		name_b = nm_wimax_nsp_get_name (NM_WIMAX_NSP (b));

	return g_strcmp0 (name_a, name_b);
}

static void
wimax_add_menu_item (NMDevice *device,
                     gboolean multiple_devices,
                     GSList *connections,
                     NMConnection *active,
                     GtkWidget *menu,
                     NMApplet *applet)
{
	NMDeviceWimax *wimax = NM_DEVICE_WIMAX (device);
	char *text;
	GtkWidget *item;
	GSList *iter, *sorted = NULL;
	const GPtrArray *nsps;
	NMWimaxNsp *active_nsp = NULL;
	gboolean wimax_enabled, wimax_hw_enabled;
	int i;

	nsps = nm_device_wimax_get_nsps (wimax);

	if (multiple_devices) {
		const char *desc;

		desc = nma_utils_get_device_description (device);
		text = g_strdup_printf (_("WiMAX Mobile Broadband (%s)"), desc);
	} else {
		text = g_strdup (_("WiMAX Mobile Broadband"));
	}

	item = applet_menu_item_create_device_item_helper (device, applet, text);
	gtk_widget_set_sensitive (item, FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
	g_free (text);

	/* Add the active NSP if we're connected to something and the device is available */
	if (!nma_menu_device_check_unusable (device)) {
		active_nsp = nm_device_wimax_get_active_nsp (wimax);
		if (active_nsp) {
			item = new_nsp_menu_item (wimax, active, TRUE, active_nsp, applet);
			if (item) {
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
				gtk_widget_show (item);
			}
		}
	}

	/* Notify user of unmanaged or unavailable device */
	wimax_enabled = nm_client_wimax_get_enabled (applet->nm_client);
	wimax_hw_enabled = nm_client_wimax_hardware_get_enabled (applet->nm_client);
	item = nma_menu_device_get_menu_item (device, applet,
	                                      wimax_hw_enabled ?
	                                          (wimax_enabled ? NULL : _("WiMAX is disabled")) :
	                                          _("WiMAX is disabled by hardware switch"));
	if (item) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
	}

	/* If disabled or rfkilled or whatever, nothing left to do */
	if (nma_menu_device_check_unusable (device))
		return;

	/* Create a sorted list of NSPs */
	for (i = 0; nsps && (i < nsps->len); i++) {
		NMWimaxNsp *nsp = g_ptr_array_index (nsps, i);

		if (nsp != active_nsp)
			sorted = g_slist_insert_sorted (sorted, nsp, sort_nsps);
	}

	if (g_slist_length (sorted)) {
		applet_menu_item_add_complex_separator_helper (menu, applet, _("Available"));

		/* And add menu items for each NSP */
		for (iter = sorted; iter; iter = g_slist_next (iter)) {
			NMWimaxNsp *nsp = NM_WIMAX_NSP (iter->data);
			NMConnection *connection = NULL;

			connection = get_connection_for_nsp (connections, nsp);
			item = new_nsp_menu_item (wimax, connection, FALSE, nsp, applet);
			if (item) {
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
				gtk_widget_show (item);
			}
		}
	}

	g_slist_free (sorted);
}


static void
nsp_quality_changed (NMWimaxNsp *nsp, GParamSpec *pspec, gpointer user_data)
{
	applet_schedule_update_icon (NM_APPLET (user_data));
}

static NMWimaxNsp *
update_active_nsp (NMDevice *device, NMDeviceState state, NMApplet *applet)
{
	NMWimaxNsp *new = NULL, *old;

	if (state == NM_DEVICE_STATE_PREPARE ||
	    state == NM_DEVICE_STATE_CONFIG ||
	    state == NM_DEVICE_STATE_IP_CONFIG ||
	    state == NM_DEVICE_STATE_NEED_AUTH ||
	    state == NM_DEVICE_STATE_ACTIVATED) {
		new = nm_device_wimax_get_active_nsp (NM_DEVICE_WIMAX (device));
	}

	old = g_object_get_data (G_OBJECT (device), ACTIVE_NSP_TAG);
	if (new && (new == old))
		return new;   /* no change */

	if (old) {
		g_signal_handlers_disconnect_by_func (old, G_CALLBACK (nsp_quality_changed), applet);
		g_object_set_data (G_OBJECT (device), ACTIVE_NSP_TAG, NULL);
	}

	if (new) {
		g_object_set_data (G_OBJECT (device), ACTIVE_NSP_TAG, new);

		/* monitor this NSP's signal strength for updating the applet icon */
		g_signal_connect (new,
		                  "notify::" NM_WIMAX_NSP_SIGNAL_QUALITY,
		                  G_CALLBACK (nsp_quality_changed),
		                  applet);
	}

	return new;
}

static void
active_nsp_changed_cb (NMDeviceWimax *device,
                       GParamSpec *pspec,
                       NMApplet *applet)
{
	NMRemoteConnection *connection;
	NMSettingWimax *s_wimax;
	NMWimaxNsp *new;
	NMDeviceState state;

	state = nm_device_get_state (NM_DEVICE (device));

	new = update_active_nsp (NM_DEVICE (device), state, applet);
	if (!new || (state != NM_DEVICE_STATE_ACTIVATED))
		return;

	connection = applet_get_exported_connection_for_device (NM_DEVICE (device), applet);
	if (!connection)
		return;

	s_wimax = nm_connection_get_setting_wimax (NM_CONNECTION (connection));
	if (!s_wimax)
		return;

	if (g_strcmp0 (nm_wimax_nsp_get_name (new), nm_setting_wimax_get_network_name (s_wimax)) != 0)
		applet_schedule_update_icon (applet);
}

static void
nsp_removed_cb (NMDeviceWimax *device,
                NMWimaxNsp *nsp,
                gpointer user_data)
{
	NMApplet *applet = NM_APPLET  (user_data);
	NMWimaxNsp *old;

	/* Clear the ACTIVE_NSP_TAG if the active NSP just got removed */
	old = g_object_get_data (G_OBJECT (device), ACTIVE_NSP_TAG);
	if (old == nsp) {
		g_object_set_data (G_OBJECT (device), ACTIVE_NSP_TAG, NULL);
		applet_schedule_update_icon (applet);
	}
}

static void
wimax_device_added (NMDevice *device, NMApplet *applet)
{
	g_signal_connect (device,
	                  "notify::" NM_DEVICE_WIMAX_ACTIVE_NSP,
	                  G_CALLBACK (active_nsp_changed_cb),
	                  applet);

	g_signal_connect (device,
	                  "nsp-removed",
	                  G_CALLBACK (nsp_removed_cb),
	                  applet);
}

static void
wimax_notify_connected (NMDevice *device,
                        const char *msg,
                        NMApplet *applet)
{
	applet_do_notify_with_pref (applet,
	                            _("Connection Established"),
	                            msg ? msg : _("You are now connected to the WiMAX network."),
	                            "nm-device-wwan",
	                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
}

static void
wimax_get_icon (NMDevice *device,
                NMDeviceState state,
                NMConnection *connection,
                GdkPixbuf **out_pixbuf,
                const char **out_icon_name,
                char **tip,
                NMApplet *applet)
{
	NMSettingConnection *s_con;
	const char *id;
	NMWimaxNsp *nsp;
	guint32 quality = 0;
	NMWimaxNspNetworkType nsp_type = NM_WIMAX_NSP_NETWORK_TYPE_UNKNOWN;
	gboolean roaming;

	id = nm_device_get_iface (NM_DEVICE (device));
	if (connection) {
		s_con = nm_connection_get_setting_connection (connection);
		id = nm_setting_connection_get_id (s_con);
	}

	nsp = nm_device_wimax_get_active_nsp (NM_DEVICE_WIMAX (device));
	if (nsp) {
		quality = nm_wimax_nsp_get_signal_quality (nsp);
		nsp_type = nm_wimax_nsp_get_network_type (nsp);
	}

	switch (state) {
	case NM_DEVICE_STATE_PREPARE:
		*tip = g_strdup_printf (_("Preparing mobile broadband connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_CONFIG:
		*tip = g_strdup_printf (_("Configuring mobile broadband connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_NEED_AUTH:
		*tip = g_strdup_printf (_("User authentication required for mobile broadband connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_IP_CONFIG:
		*tip = g_strdup_printf (_("Requesting a network address for '%s'..."), id);
		break;
	case NM_DEVICE_STATE_ACTIVATED:
		roaming = (nsp_type == NM_WIMAX_NSP_NETWORK_TYPE_ROAMING_PARTNER);
		*out_pixbuf = mobile_helper_get_status_pixbuf (quality,
		                                          TRUE,
		                                          nsp_type_to_mb_state (nsp_type),
		                                          MB_TECH_WIMAX,
		                                          applet);
		*tip = g_strdup_printf (_("Mobile broadband connection '%s' active: (%d%%%s%s)"),
		                        id, quality,
		                        roaming ? ", " : "",
		                        roaming ? _("roaming") : "");
		break;
	default:
		break;
	}
}

static gboolean
wimax_get_secrets (SecretsRequest *req, GError **error)
{
	g_set_error (error,
	             NM_SECRET_AGENT_ERROR,
	             NM_SECRET_AGENT_ERROR_NO_SECRETS,
	             "%s.%d (%s): no WiMAX secrets available.",
	             __FILE__, __LINE__, __func__);
	return FALSE;
}

NMADeviceClass *
applet_device_wimax_get_class (NMApplet *applet)
{
	NMADeviceClass *dclass;

	dclass = g_slice_new0 (NMADeviceClass);
	if (!dclass)
		return NULL;

	dclass->new_auto_connection = wimax_new_auto_connection;
	dclass->add_menu_item = wimax_add_menu_item;
	dclass->device_added = wimax_device_added;
	dclass->notify_connected = wimax_notify_connected;
	dclass->get_icon = wimax_get_icon;
	dclass->get_secrets = wimax_get_secrets;

	return dclass;
}

