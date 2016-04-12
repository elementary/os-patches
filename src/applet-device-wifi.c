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
 * Copyright 2008 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <ctype.h>

#include "applet.h"
#include "applet-device-wifi.h"
#include "ap-menu-item.h"
#include "utils.h"
#include "nma-wifi-dialog.h"
#include "mobile-helpers.h"

#define ACTIVE_AP_TAG "active-ap"

static void wifi_dialog_response_cb (GtkDialog *dialog, gint response, gpointer user_data);

static NMAccessPoint *update_active_ap (NMDevice *device, NMDeviceState state, NMApplet *applet);

static void _do_new_auto_connection (NMApplet *applet,
                                     NMDevice *device,
                                     NMAccessPoint *ap,
                                     AppletNewAutoConnectionCallback callback,
                                     gpointer callback_data);

/*****************************************************************************/

typedef struct {
	NMApplet *applet;
	NMDevice *device;
	NMAccessPoint *ap;
	gulong signal_id;
} ActiveAPData;

static void _active_ap_set (NMApplet *applet, NMDevice *device, NMAccessPoint *ap);
static void _active_ap_set_weakref (gpointer data, GObject *where_the_object_was);

static void
_active_ap_set_notify (NMAccessPoint *ap, GParamSpec *pspec, gpointer user_data)
{
	ActiveAPData *d = user_data;

	g_return_if_fail (NM_IS_ACCESS_POINT (ap));
	g_return_if_fail (d);
	g_return_if_fail (NM_IS_APPLET (d->applet));
	g_return_if_fail (NM_IS_DEVICE (d->device));
	g_return_if_fail (d->ap == ap);
	g_return_if_fail (d->signal_id);

	applet_schedule_update_icon (d->applet);
}

static void
_active_ap_data_free (ActiveAPData *d)
{
	if (d->device)
		g_object_weak_unref ((GObject *) d->device, _active_ap_set_weakref, d);
	if (d->ap) {
		g_object_weak_unref ((GObject *) d->ap, _active_ap_set_weakref, d);
		g_signal_handler_disconnect (d->ap, d->signal_id);
	}
	g_slice_free (ActiveAPData, d);
}

static NMAccessPoint *
_active_ap_get (NMApplet *applet, NMDevice *device)
{
	GSList *list, *iter;

	g_return_val_if_fail (NM_IS_APPLET (applet), NULL);
	g_return_val_if_fail (NM_IS_DEVICE (device), NULL);

	list = g_object_get_data ((GObject *) applet, ACTIVE_AP_TAG);
	for (iter = list; iter; iter = iter->next) {
		ActiveAPData *d = iter->data;

		if (device == d->device && d->ap)
			return d->ap;
	}
	return NULL;
}

static void
_active_ap_set_destroy (gpointer data)
{
	g_slist_free_full (data, (GDestroyNotify) _active_ap_data_free);
}

static void
_active_ap_set_weakref (gpointer data, GObject *where_the_object_was)
{
	ActiveAPData *d = data;

	if ((GObject *) d->ap == where_the_object_was)
		d->ap = NULL;
	else if ((GObject *) d->device == where_the_object_was)
		d->device = NULL;
	else
		g_return_if_reached ();
	_active_ap_set (d->applet, NULL, NULL);

	applet_schedule_update_icon (d->applet);
}

static void
_active_ap_set (NMApplet *applet, NMDevice *device, NMAccessPoint *ap)
{
	GSList *list, *iter, *list0, *pcurrent;
	ActiveAPData *d;

	g_return_if_fail (NM_IS_APPLET (applet));
	g_return_if_fail (!device || NM_IS_DEVICE (device));
	g_return_if_fail (!ap || NM_IS_ACCESS_POINT (ap));

	list0 = g_object_get_data ((GObject *) applet, ACTIVE_AP_TAG);
	list = list0;

remove_empty:
	pcurrent = NULL;
	for (iter = list; iter; iter = iter->next) {
		d = iter->data;
		if (!d->device || !d->ap) {
			_active_ap_data_free (d);
			list = g_slist_delete_link (list, iter);
			goto remove_empty;
		}
		if (device && d->device == device)
			pcurrent = iter;
	}

	if (!device)
		goto out;

	if (!ap) {
		if (pcurrent) {
			_active_ap_data_free (pcurrent->data);
			list = g_slist_delete_link (list, pcurrent);
		}
		goto out;
	}

	if (pcurrent) {
		d = pcurrent->data;

		if (d->ap == ap)
			goto out;
		g_object_weak_unref ((GObject *) d->ap, _active_ap_set_weakref, d);
		g_signal_handler_disconnect (d->ap, d->signal_id);
	} else {
		d = g_slice_new (ActiveAPData);

		d->applet = applet;
		d->device = device;
		g_object_weak_ref ((GObject *) device, _active_ap_set_weakref, d);
		list = g_slist_append (list, d);
	}
	d->ap = ap;
	g_object_weak_ref ((GObject *) ap, _active_ap_set_weakref, d);
	d->signal_id = g_signal_connect (ap,
	                                 "notify::" NM_ACCESS_POINT_STRENGTH,
	                                 G_CALLBACK (_active_ap_set_notify),
	                                 d);

out:
	if (list0 != list) {
		g_object_replace_data ((GObject *) applet, ACTIVE_AP_TAG,
		                       list0, list,
		                       _active_ap_set_destroy, NULL);
	}
}

/*****************************************************************************/

static void
show_ignore_focus_stealing_prevention (GtkWidget *widget)
{
	GdkWindow *window;

	gtk_widget_realize (widget);
	gtk_widget_show (widget);
	window = gtk_widget_get_window (widget);
	gtk_window_present_with_time (GTK_WINDOW (widget), gdk_x11_get_server_time (window));
}

gboolean
applet_wifi_connect_to_hidden_network (NMApplet *applet)
{
	GtkWidget *dialog;

	dialog = nma_wifi_dialog_new_for_hidden (applet->nm_client);
	if (dialog) {
		g_signal_connect (dialog, "response",
		                  G_CALLBACK (wifi_dialog_response_cb),
		                  applet);
		show_ignore_focus_stealing_prevention (dialog);
	}
	return !!dialog;
}

void
nma_menu_add_hidden_network_item (GtkWidget *menu, NMApplet *applet)
{
	GtkWidget *menu_item;
	GtkWidget *label;

	menu_item = gtk_menu_item_new ();
	label = gtk_label_new_with_mnemonic (_("_Connect to Hidden Wi-Fi Network..."));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_container_add (GTK_CONTAINER (menu_item), label);
	gtk_widget_show_all (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	g_signal_connect_swapped (menu_item, "activate",
	                          G_CALLBACK (applet_wifi_connect_to_hidden_network),
	                          applet);
}

gboolean
applet_wifi_can_create_wifi_network (NMApplet *applet)
{
	gboolean disabled, allowed = FALSE;
	NMClientPermissionResult perm;

	/* FIXME: check WIFI_SHARE_PROTECTED too, and make the wifi dialog
	 * handle the permissions as well so that admins can restrict open network
	 * creation separately from protected network creation.
	 */
	perm = nm_client_get_permission_result (applet->nm_client, NM_CLIENT_PERMISSION_WIFI_SHARE_OPEN);
	if (perm == NM_CLIENT_PERMISSION_RESULT_YES || perm == NM_CLIENT_PERMISSION_RESULT_AUTH) {
		disabled = g_settings_get_boolean (applet->gsettings, PREF_DISABLE_WIFI_CREATE);
		if (!disabled)
			allowed = TRUE;
	}
	return allowed;
}

gboolean
applet_wifi_create_wifi_network (NMApplet *applet)
{
	GtkWidget *dialog;

	dialog = nma_wifi_dialog_new_for_create (applet->nm_client);
	if (dialog) {
		g_signal_connect (dialog, "response",
		                  G_CALLBACK (wifi_dialog_response_cb),
		                  applet);
		show_ignore_focus_stealing_prevention (dialog);
	}
	return !!dialog;
}

void
nma_menu_add_create_network_item (GtkWidget *menu, NMApplet *applet)
{
	GtkWidget *menu_item;
	GtkWidget *label;

	menu_item = gtk_menu_item_new ();
	label = gtk_label_new_with_mnemonic (_("Create _New Wi-Fi Network..."));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_container_add (GTK_CONTAINER (menu_item), label);
	gtk_widget_show_all (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	g_signal_connect_swapped (menu_item, "activate",
	                          G_CALLBACK (applet_wifi_create_wifi_network),
	                          applet);

	if (!applet_wifi_can_create_wifi_network (applet))
		gtk_widget_set_sensitive (GTK_WIDGET (menu_item), FALSE);
}

static void
dbus_8021x_add_and_activate_cb (GObject *client,
                                GAsyncResult *result,
                                gpointer user_data)
{
	GError *error = NULL;
	NMActiveConnection *active;

	active = nm_client_add_and_activate_connection_finish (NM_CLIENT (client), result, &error);
	if (error)
		g_warning ("Failed to add/activate connection: (%d) %s", error->code, error->message);

	g_clear_object (&active);
	g_clear_error (&error);
}

typedef struct {
	NMApplet *applet;
	NMDevice *device;
	NMAccessPoint *ap;
} Dbus8021xInfo;

static void
dbus_connect_8021x_cb (NMConnection *connection,
                       gboolean auto_created,
                       gboolean canceled,
                       gpointer user_data)
{
	Dbus8021xInfo *info = user_data;

	if (canceled == FALSE) {
		g_return_if_fail (connection != NULL);

		/* Ask NM to add the new connection and activate it; NM will fill in the
		 * missing details based on the specific object and the device.
		 */
		nm_client_add_and_activate_connection_async (info->applet->nm_client,
		                                             connection,
			                                     info->device,
			                                     nm_object_get_path (NM_OBJECT (info->ap)),
		                                             NULL,
			                                     dbus_8021x_add_and_activate_cb,
			                                     info->applet);
	}

	g_object_unref (info->device);
	g_object_unref (info->ap);
	memset (info, 0, sizeof (*info));
	g_free (info);
}

gboolean
applet_wifi_connect_to_8021x_network (NMApplet *applet,
                                      NMDevice *device,
                                      NMAccessPoint *ap)
{
	Dbus8021xInfo *info;

	info = g_malloc0 (sizeof (*info));
	info->applet = applet;
	info->device = g_object_ref (device);
	info->ap = g_object_ref (ap);

	_do_new_auto_connection (applet, device, ap, dbus_connect_8021x_cb, info);
	return TRUE;
}


typedef struct {
	NMApplet *applet;
	NMDeviceWifi *device;
	NMAccessPoint *ap;
	NMConnection *connection;
} WifiMenuItemInfo;

static void
wifi_menu_item_info_destroy (gpointer data)
{
	WifiMenuItemInfo *info = (WifiMenuItemInfo *) data;

	g_object_unref (G_OBJECT (info->device));
	g_object_unref (G_OBJECT (info->ap));

	if (info->connection)
		g_object_unref (G_OBJECT (info->connection));

	g_slice_free (WifiMenuItemInfo, data);
}

/*
 * NOTE: this list should *not* contain networks that you would like to
 * automatically roam to like "Starbucks" or "AT&T" or "T-Mobile HotSpot".
 */
static const char *manf_default_ssids[] = {
	"linksys",
	"linksys-a",
	"linksys-g",
	"default",
	"belkin54g",
	"NETGEAR",
	"o2DSL",
	"WLAN",
	"ALICE-WLAN",
	NULL
};

static gboolean
is_ssid_in_list (GBytes *ssid, const char **list)
{
	while (*list) {
		if (g_bytes_get_size (ssid) == strlen (*list)) {
			if (!memcmp (*list, g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid)))
				return TRUE;
		}
		list++;
	}
	return FALSE;
}

static gboolean
is_manufacturer_default_ssid (GBytes *ssid)
{
	return is_ssid_in_list (ssid, manf_default_ssids);
}

static char *
get_ssid_utf8 (NMAccessPoint *ap)
{
	char *ssid_utf8 = NULL;
	GBytes *ssid;

	if (ap) {
		ssid = nm_access_point_get_ssid (ap);
		if (ssid)
			ssid_utf8 = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
	}
	if (!ssid_utf8)
		ssid_utf8 = g_strdup (_("(none)"));

	return ssid_utf8;
}

/* List known trojan networks that should never be shown to the user */
static const char *blacklisted_ssids[] = {
	/* http://www.npr.org/templates/story/story.php?storyId=130451369 */
	"Free Public WiFi",
	NULL
};

static gboolean
is_blacklisted_ssid (GBytes *ssid)
{
	return is_ssid_in_list (ssid, blacklisted_ssids);
}

static void
clamp_ap_to_bssid (NMAccessPoint *ap, NMSettingWireless *s_wifi)
{
	const char *str_bssid;
	struct ether_addr *eth_addr;
	GByteArray *bssid;

	/* For a certain list of known ESSIDs which are commonly preset by ISPs
	 * and manufacturers and often unchanged by users, lock the connection
	 * to the BSSID so that we don't try to auto-connect to your grandma's
	 * neighbor's WiFi.
	 */

	str_bssid = nm_access_point_get_bssid (ap);
	if (str_bssid) {
		eth_addr = ether_aton (str_bssid);
		if (eth_addr) {
			bssid = g_byte_array_sized_new (ETH_ALEN);
			g_byte_array_append (bssid, eth_addr->ether_addr_octet, ETH_ALEN);
			g_object_set (G_OBJECT (s_wifi),
			              NM_SETTING_WIRELESS_BSSID, bssid,
			              NULL);
			g_byte_array_free (bssid, TRUE);
		}
	}
}

typedef struct {
	NMApplet *applet;
	AppletNewAutoConnectionCallback callback;
	gpointer callback_data;
} MoreInfo;

static void
more_info_wifi_dialog_response_cb (GtkDialog *foo,
                                   gint response,
                                   gpointer user_data)
{
	NMAWifiDialog *dialog = NMA_WIFI_DIALOG (foo);
	MoreInfo *info = user_data;
	NMConnection *connection = NULL;
	NMDevice *device = NULL;
	NMAccessPoint *ap = NULL;

	if (response != GTK_RESPONSE_OK) {
		info->callback (NULL, FALSE, TRUE, info->callback_data);
		goto done;
	}

	/* nma_wifi_dialog_get_connection() returns a connection with the
	 * refcount incremented, so the caller must remember to unref it.
	 */
	connection = nma_wifi_dialog_get_connection (dialog, &device, &ap);
	g_assert (connection);
	g_assert (device);

	info->callback (connection, TRUE, FALSE, info->callback_data);

	/* Balance nma_wifi_dialog_get_connection() */
	g_object_unref (connection);

done:
	g_free (info);
	gtk_widget_hide (GTK_WIDGET (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
_do_new_auto_connection (NMApplet *applet,
                         NMDevice *device,
                         NMAccessPoint *ap,
                         AppletNewAutoConnectionCallback callback,
                         gpointer callback_data)
{
	NMConnection *connection = NULL;
	NMSettingConnection *s_con = NULL;
	NMSettingWireless *s_wifi = NULL;
	NMSettingWirelessSecurity *s_wsec = NULL;
	NMSetting8021x *s_8021x = NULL;
	GBytes *ssid;
	NM80211ApSecurityFlags wpa_flags, rsn_flags;
	GtkWidget *dialog;
	MoreInfo *more_info;
	char *uuid;

	g_assert (applet);
	g_assert (device);
	g_assert (ap);

	connection = nm_simple_connection_new ();

	/* Make the new connection available only for the current user */
	s_con = (NMSettingConnection *) nm_setting_connection_new ();
	nm_setting_connection_add_permission (s_con, "user", g_get_user_name (), NULL);
	nm_connection_add_setting (connection, NM_SETTING (s_con));

	ssid = nm_access_point_get_ssid (ap);
	if (   (nm_access_point_get_mode (ap) == NM_802_11_MODE_INFRA)
	    && (is_manufacturer_default_ssid (ssid) == TRUE)) {

		/* Lock connection to this AP if it's a manufacturer-default SSID
		 * so that we don't randomly connect to some other 'linksys'
		 */
		s_wifi = (NMSettingWireless *) nm_setting_wireless_new ();
		clamp_ap_to_bssid (ap, s_wifi);
		nm_connection_add_setting (connection, NM_SETTING (s_wifi));
	}

	/* If the AP is WPA[2]-Enterprise then we need to set up a minimal 802.1x
	 * setting and ask the user for more information.
	 */
	rsn_flags = nm_access_point_get_rsn_flags (ap);
	wpa_flags = nm_access_point_get_wpa_flags (ap);
	if (   (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)
	    || (wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)) {

		/* Need a UUID for the "always ask" stuff in the Dialog of Doom */
		uuid = nm_utils_uuid_generate ();
		g_object_set (s_con, NM_SETTING_CONNECTION_UUID, uuid, NULL);
		g_free (uuid);

		if (!s_wifi) {
			s_wifi = (NMSettingWireless *) nm_setting_wireless_new ();
			nm_connection_add_setting (connection, NM_SETTING (s_wifi));
		}
		g_object_set (s_wifi,
		              NM_SETTING_WIRELESS_SSID, ssid,
		              NULL);

		s_wsec = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
		g_object_set (s_wsec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-eap", NULL);
		nm_connection_add_setting (connection, NM_SETTING (s_wsec));

		s_8021x = (NMSetting8021x *) nm_setting_802_1x_new ();
		nm_setting_802_1x_add_eap_method (s_8021x, "ttls");
		g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, "mschapv2", NULL);
		nm_connection_add_setting (connection, NM_SETTING (s_8021x));
	}

	/* If it's an 802.1x connection, we need more information, so pop up the
	 * Dialog Of Doom.
	 */
	if (s_8021x) {
		more_info = g_malloc0 (sizeof (*more_info));
		more_info->applet = applet;
		more_info->callback = callback;
		more_info->callback_data = callback_data;

		dialog = nma_wifi_dialog_new (applet->nm_client, connection, device, ap, FALSE);
		if (dialog) {
			g_signal_connect (dialog, "response",
				              G_CALLBACK (more_info_wifi_dialog_response_cb),
				              more_info);
			show_ignore_focus_stealing_prevention (dialog);
		}
	} else {
		/* Everything else can just get activated right away */
		callback (connection, TRUE, FALSE, callback_data);
	}
}

static gboolean
wifi_new_auto_connection (NMDevice *device,
                          gpointer dclass_data,
                          AppletNewAutoConnectionCallback callback,
                          gpointer callback_data)
{
	WifiMenuItemInfo *info = (WifiMenuItemInfo *) dclass_data;

	g_return_val_if_fail (device != NULL, FALSE);
	g_return_val_if_fail (info->ap != NULL, FALSE);

	_do_new_auto_connection (info->applet, device, info->ap, callback, callback_data);
	return TRUE;
}


static void
wifi_menu_item_activate (GtkMenuItem *item, gpointer user_data)
{
	WifiMenuItemInfo *info = (WifiMenuItemInfo *) user_data;
	const char *specific_object = NULL;

	if (info->ap)
		specific_object = nm_object_get_path (NM_OBJECT (info->ap));
	applet_menu_item_activate_helper (NM_DEVICE (info->device),
	                                  info->connection,
	                                  specific_object ? specific_object : "/",
	                                  info->applet,
	                                  user_data);
}

struct dup_data {
	NMDevice *device;
	NMNetworkMenuItem *found;
	char *hash;
};

static void
find_duplicate (gpointer d, gpointer user_data)
{
	struct dup_data * data = (struct dup_data *) user_data;
	NMDevice *device;
	const char *hash;
	GtkWidget *widget = GTK_WIDGET (d);

	g_assert (d && widget);
	g_return_if_fail (data);
	g_return_if_fail (data->hash);

	if (data->found || !NM_IS_NETWORK_MENU_ITEM (widget))
		return;

	device = g_object_get_data (G_OBJECT (widget), "device");
	if (NM_DEVICE (device) != data->device)
		return;

	hash = nm_network_menu_item_get_hash (NM_NETWORK_MENU_ITEM (widget));
	if (hash && (strcmp (hash, data->hash) == 0))
		data->found = NM_NETWORK_MENU_ITEM (widget);
}

static NMNetworkMenuItem *
create_new_ap_item (NMDeviceWifi *device,
                    NMAccessPoint *ap,
                    struct dup_data *dup_data,
                    const GPtrArray *connections,
                    NMApplet *applet)
{
	WifiMenuItemInfo *info;
	int i;
	GtkWidget *item;
	GPtrArray *dev_connections;
	GPtrArray *ap_connections;

	dev_connections = nm_device_filter_connections (NM_DEVICE (device), connections);
	ap_connections = nm_access_point_filter_connections (ap, dev_connections);
	g_ptr_array_unref (dev_connections);

	item = nm_network_menu_item_new (ap,
	                                 nm_device_wifi_get_capabilities (device),
	                                 dup_data->hash,
	                                 ap_connections->len != 0,
	                                 applet);
	g_object_set_data (G_OBJECT (item), "device", NM_DEVICE (device));

	/* If there's only one connection, don't show the submenu */
	if (ap_connections->len > 1) {
		GtkWidget *submenu;

		submenu = gtk_menu_new ();

		for (i = 0; i < ap_connections->len; i++) {
			NMConnection *connection = NM_CONNECTION (ap_connections->pdata[i]);
			NMSettingConnection *s_con;
			GtkWidget *subitem;

			s_con = nm_connection_get_setting_connection (connection);
			subitem = gtk_menu_item_new_with_label (nm_setting_connection_get_id (s_con));

			info = g_slice_new0 (WifiMenuItemInfo);
			info->applet = applet;
			info->device = g_object_ref (G_OBJECT (device));
			info->ap = g_object_ref (G_OBJECT (ap));
			info->connection = g_object_ref (G_OBJECT (connection));

			g_signal_connect_data (subitem, "activate",
			                       G_CALLBACK (wifi_menu_item_activate),
			                       info,
			                       (GClosureNotify) wifi_menu_item_info_destroy, 0);

			gtk_menu_shell_append (GTK_MENU_SHELL (submenu), GTK_WIDGET (subitem));
		}

		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
	} else {
		NMConnection *connection;

		info = g_slice_new0 (WifiMenuItemInfo);
		info->applet = applet;
		info->device = g_object_ref (G_OBJECT (device));
		info->ap = g_object_ref (G_OBJECT (ap));

		if (ap_connections->len == 1) {
			connection = NM_CONNECTION (ap_connections->pdata[0]);
			info->connection = g_object_ref (connection);
		}

		g_signal_connect_data (GTK_WIDGET (item),
		                       "activate",
		                       G_CALLBACK (wifi_menu_item_activate),
		                       info,
		                       (GClosureNotify) wifi_menu_item_info_destroy,
		                       0);
	}

	g_ptr_array_unref (ap_connections);
	return NM_NETWORK_MENU_ITEM (item);
}

static NMNetworkMenuItem *
get_menu_item_for_ap (NMDeviceWifi *device,
                      NMAccessPoint *ap,
                      const GPtrArray *connections,
                      GSList *menu_list,
                      NMApplet *applet)
{
	GBytes *ssid;
	struct dup_data dup_data = { NULL, NULL };

	/* Don't add BSSs that hide their SSID or are blacklisted */
	ssid = nm_access_point_get_ssid (ap);
	if (   !ssid
	    || nm_utils_is_empty_ssid (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid))
	    || is_blacklisted_ssid (ssid))
		return NULL;

	/* Find out if this AP is a member of a larger network that all uses the
	 * same SSID and security settings.  If so, we'll already have a menu item
	 * for this SSID, so just update that item's strength and add this AP to
	 * menu item's duplicate list.
	 */
	dup_data.found = NULL;
	dup_data.hash = g_object_get_data (G_OBJECT (ap), "hash");
	g_return_val_if_fail (dup_data.hash != NULL, NULL);

	dup_data.device = NM_DEVICE (device);
	g_slist_foreach (menu_list, find_duplicate, &dup_data);

	if (dup_data.found) {
		nm_network_menu_item_set_strength (dup_data.found, nm_access_point_get_strength (ap), applet);
		nm_network_menu_item_add_dupe (dup_data.found, ap);
		return NULL;
	}

	return create_new_ap_item (device, ap, &dup_data, connections, applet);
}

static gint
sort_by_name (gconstpointer tmpa, gconstpointer tmpb)
{
	NMNetworkMenuItem *a = NM_NETWORK_MENU_ITEM (tmpa);
	NMNetworkMenuItem *b = NM_NETWORK_MENU_ITEM (tmpb);
	const char *a_ssid, *b_ssid;
	gboolean a_adhoc, b_adhoc;
	int i;

	if (a && !b)
		return 1;
	else if (!a && b)
		return -1;
	else if (a == b)
		return 0;

	a_ssid = nm_network_menu_item_get_ssid (a);
	b_ssid = nm_network_menu_item_get_ssid (b);

	if (a_ssid && !b_ssid)
		return 1;
	if (b_ssid && !a_ssid)
		return -1;

	if (a_ssid && b_ssid) {
		i = g_ascii_strcasecmp (a_ssid, b_ssid);
		if (i != 0)
			return i;
	}

	/* If the names are the same, sort infrastructure APs first */
	a_adhoc = nm_network_menu_item_get_is_adhoc (a);
	b_adhoc = nm_network_menu_item_get_is_adhoc (b);
	if (a_adhoc && !b_adhoc)
		return 1;
	else if (!a_adhoc && b_adhoc)
		return -1;

	return 0;
}

/* Sort menu items for the top-level menu:
 * 1) whether there's a saved connection or not
 *    a) sort alphabetically within #1
 * 2) encrypted without a saved connection
 * 3) unencrypted without a saved connection
 */
static gint
sort_toplevel (gconstpointer tmpa, gconstpointer tmpb)
{
	NMNetworkMenuItem *a = NM_NETWORK_MENU_ITEM (tmpa);
	NMNetworkMenuItem *b = NM_NETWORK_MENU_ITEM (tmpb);
	gboolean a_fave, b_fave;

	if (a && !b)
		return 1;
	else if (!a && b)
		return -1;
	else if (a == b)
		return 0;

	a_fave = nm_network_menu_item_get_has_connections (a);
	b_fave = nm_network_menu_item_get_has_connections (b);

	/* Items with a saved connection first */
	if (a_fave && !b_fave)
		return -1;
	else if (!a_fave && b_fave)
		return 1;
	else if (!a_fave && !b_fave) {
		gboolean a_enc = nm_network_menu_item_get_is_encrypted (a);
		gboolean b_enc = nm_network_menu_item_get_is_encrypted (b);

		/* If neither item has a saved connection, sort by encryption */
		if (a_enc && !b_enc)
			return -1;
		else if (!a_enc && b_enc)
			return 1;
	}

	/* For all other cases (both have saved connections, both are encrypted, or
	 * both are unencrypted) just sort by name.
	 */
	return sort_by_name (a, b);
}

static void
wifi_add_menu_item (NMDevice *device,
                    gboolean multiple_devices,
                    const GPtrArray *connections,
                    NMConnection *active,
                    GtkWidget *menu,
                    NMApplet *applet)
{
	NMDeviceWifi *wdev;
	char *text;
	const GPtrArray *aps;
	int i;
	NMAccessPoint *active_ap = NULL;
	GSList *iter;
	gboolean wifi_enabled = TRUE;
	gboolean wifi_hw_enabled = TRUE;
	GSList *menu_items = NULL;  /* All menu items we'll be adding */
	NMNetworkMenuItem *item, *active_item = NULL;
	GtkWidget *widget;

	wdev = NM_DEVICE_WIFI (device);
	aps = nm_device_wifi_get_access_points (wdev);

	if (multiple_devices) {
		const char *desc;

		desc = nm_device_get_description (device);
		if (aps && aps->len > 1)
			text = g_strdup_printf (_("Wi-Fi Networks (%s)"), desc);
		else
			text = g_strdup_printf (_("Wi-Fi Network (%s)"), desc);
	} else
		text = g_strdup (ngettext ("Wi-Fi Network", "Wi-Fi Networks", aps ? aps->len : 0));

	widget = applet_menu_item_create_device_item_helper (device, applet, text);
	g_free (text);

	gtk_widget_set_sensitive (widget, FALSE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), widget);
	gtk_widget_show (widget);

	/* Add the active AP if we're connected to something and the device is available */
	if (!nma_menu_device_check_unusable (device)) {
		active_ap = nm_device_wifi_get_active_access_point (wdev);
		if (active_ap) {
			active_item = item = get_menu_item_for_ap (wdev, active_ap, connections, NULL, applet);
			if (item) {
				nm_network_menu_item_set_active (item, TRUE);
				menu_items = g_slist_append (menu_items, item);

				gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (item));
				gtk_widget_show_all (GTK_WIDGET (item));
			}
		}
	}

	/* Notify user of unmanaged or unavailable device */
	wifi_enabled = nm_client_wireless_get_enabled (applet->nm_client);
	wifi_hw_enabled = nm_client_wireless_hardware_get_enabled (applet->nm_client);
	widget = nma_menu_device_get_menu_item (device, applet,
	                                        wifi_hw_enabled ?
	                                            (wifi_enabled ? NULL : _("Wi-Fi is disabled")) :
	                                            _("Wi-Fi is disabled by hardware switch"));
	if (widget) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), widget);
		gtk_widget_show (widget);
	}

	/* If disabled or rfkilled or whatever, nothing left to do */
	if (nma_menu_device_check_unusable (device))
		goto out;

	/* Create menu items for the rest of the APs */
	for (i = 0; aps && (i < aps->len); i++) {
		NMAccessPoint *ap = g_ptr_array_index (aps, i);

		item = get_menu_item_for_ap (wdev, ap, connections, menu_items, applet);
		if (item)
			menu_items = g_slist_append (menu_items, item);
	}

	/* Now remove the active AP item from the list, as we've already dealt with
	 * it.  (Needed it when creating menu items for the rest of the APs though
	 * to ensure duplicate APs are handled correctly)
	 */
	if (active_item)
		menu_items = g_slist_remove (menu_items, active_item);

	/* Sort all the rest of the menu items for the top-level menu */
	menu_items = g_slist_sort (menu_items, sort_toplevel);

	if (g_slist_length (menu_items)) {
		GSList *submenu_items = NULL;
		GSList *topmenu_items = NULL;
		guint32 num_for_toplevel = 5;

		applet_menu_item_add_complex_separator_helper (menu, applet, _("Available"));

		if (g_slist_length (menu_items) == (num_for_toplevel + 1))
			num_for_toplevel++;

		/* Add the first 5 APs (or 6 if there are only 6 total) from the sorted
		 * toplevel list.
		 */
		for (iter = menu_items; iter && num_for_toplevel; iter = g_slist_next (iter)) {
			topmenu_items = g_slist_append (topmenu_items, iter->data);
			num_for_toplevel--;
			submenu_items = iter->next;
		}
		topmenu_items = g_slist_sort (topmenu_items, sort_by_name);

		for (iter = topmenu_items; iter; iter = g_slist_next (iter)) {
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (iter->data));
			gtk_widget_show_all (GTK_WIDGET (iter->data));
		}
		g_slist_free (topmenu_items);
		topmenu_items = NULL;

		/* If there are any submenu items, make a submenu for those */
		if (submenu_items) {
			GtkWidget *subitem, *submenu;
			GSList *sorted_subitems;

			subitem = gtk_menu_item_new_with_mnemonic (_("More networks"));
			submenu = gtk_menu_new ();
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (subitem), submenu);

			/* Sort the subitems alphabetically */
			sorted_subitems = g_slist_copy (submenu_items);
			sorted_subitems = g_slist_sort (sorted_subitems, sort_by_name);

			/* And add the rest to the submenu */
			for (iter = sorted_subitems; iter; iter = g_slist_next (iter))
				gtk_menu_shell_append (GTK_MENU_SHELL (submenu), GTK_WIDGET (iter->data));
			g_slist_free (sorted_subitems);

			gtk_menu_shell_append (GTK_MENU_SHELL (menu), subitem);
			gtk_widget_show_all (subitem);
		}
	}

out:
	g_slist_free (menu_items);
}

static void
notify_active_ap_changed_cb (NMDeviceWifi *device,
                             GParamSpec *pspec,
                             NMApplet *applet)
{
	NMRemoteConnection *connection;
	NMSettingWireless *s_wireless;
	NMAccessPoint *new;
	GBytes *ssid_ap, *ssid;
	NMDeviceState state;

	state = nm_device_get_state (NM_DEVICE (device));

	new = update_active_ap (NM_DEVICE (device), state, applet);
	if (!new || (state != NM_DEVICE_STATE_ACTIVATED))
		return;

	connection = applet_get_exported_connection_for_device (NM_DEVICE (device), applet);
	if (!connection)
		return;

	s_wireless = nm_connection_get_setting_wireless (NM_CONNECTION (connection));
	if (!s_wireless)
		return;

	ssid_ap = nm_access_point_get_ssid (new);
	ssid = nm_setting_wireless_get_ssid (s_wireless);
	if (   !ssid_ap
	    || !ssid
	    || !nm_utils_same_ssid (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid),
	                            g_bytes_get_data (ssid_ap, NULL), g_bytes_get_size (ssid_ap),
	                            TRUE))
		return;

	applet_schedule_update_icon (applet);
}

static void
add_hash_to_ap (NMAccessPoint *ap)
{
	char *hash;

	hash = utils_hash_ap (nm_access_point_get_ssid (ap),
	                      nm_access_point_get_mode (ap),
	                      nm_access_point_get_flags (ap),
	                      nm_access_point_get_wpa_flags (ap),
	                      nm_access_point_get_rsn_flags (ap));
	g_object_set_data_full (G_OBJECT (ap), "hash", hash, (GDestroyNotify) g_free);
}

static void
notify_ap_prop_changed_cb (NMAccessPoint *ap,
                           GParamSpec *pspec,
                           NMApplet *applet)
{
	const char *prop = g_param_spec_get_name (pspec);

	if (   !strcmp (prop, NM_ACCESS_POINT_FLAGS)
	    || !strcmp (prop, NM_ACCESS_POINT_WPA_FLAGS)
	    || !strcmp (prop, NM_ACCESS_POINT_RSN_FLAGS)
	    || !strcmp (prop, NM_ACCESS_POINT_SSID)
	    || !strcmp (prop, NM_ACCESS_POINT_FREQUENCY)
	    || !strcmp (prop, NM_ACCESS_POINT_MODE)) {
		add_hash_to_ap (ap);
	}
}

static void
wifi_available_dont_show_cb (NotifyNotification *notify,
			                 gchar *id,
			                 gpointer user_data)
{
	NMApplet *applet = NM_APPLET (user_data);

	if (!id || strcmp (id, "dont-show"))
		return;

	g_settings_set_boolean (applet->gsettings,
	                        PREF_SUPPRESS_WIFI_NETWORKS_AVAILABLE,
	                        TRUE);
}


struct ap_notification_data 
{
	NMApplet *applet;
	NMDeviceWifi *device;
	guint id;
	gulong last_notification_time;
	guint new_con_id;
};

/* Scan the list of access points, looking for the case where we have no
 * known (i.e. autoconnect) access points, but we do have unknown ones.
 * 
 * If we find one, notify the user.
 */
static gboolean
idle_check_avail_access_point_notification (gpointer datap)
{	
	struct ap_notification_data *data = datap;
	NMApplet *applet = data->applet;
	NMDeviceWifi *device = data->device;
	int i;
	const GPtrArray *aps;
	GPtrArray *all_connections;
	GPtrArray *connections;
	GTimeVal timeval;
	gboolean have_unused_access_point = FALSE;
	gboolean have_no_autoconnect_points = TRUE;

	data->id = 0;

	if (nm_client_get_state (data->applet->nm_client) != NM_STATE_DISCONNECTED)
		return FALSE;

	if (nm_device_get_state (NM_DEVICE (device)) != NM_DEVICE_STATE_DISCONNECTED)
		return FALSE;

	g_get_current_time (&timeval);
	if ((timeval.tv_sec - data->last_notification_time) < 60*60) /* Notify at most once an hour */
		return FALSE;	

	all_connections = applet_get_all_connections (applet);
	connections = nm_device_filter_connections (NM_DEVICE (device), all_connections);
	g_ptr_array_unref (all_connections);	

	aps = nm_device_wifi_get_access_points (device);
	for (i = 0; i < aps->len; i++) {
		NMAccessPoint *ap = aps->pdata[i];
		GPtrArray *ap_connections = nm_access_point_filter_connections (ap, connections);
		int a;
		gboolean is_autoconnect = FALSE;

		for (a = 0; a < ap_connections->len; a++) {
			NMConnection *connection = NM_CONNECTION (ap_connections->pdata[a]);
			NMSettingConnection *s_con;

			s_con = nm_connection_get_setting_connection (connection);
			if (nm_setting_connection_get_autoconnect (s_con))  {
				is_autoconnect = TRUE;
				break;
			}
		}
		g_ptr_array_unref (ap_connections);

		if (!is_autoconnect)
			have_unused_access_point = TRUE;
		else
			have_no_autoconnect_points = FALSE;
	}
	g_ptr_array_unref (connections);

	if (!(have_unused_access_point && have_no_autoconnect_points))
		return FALSE;

	/* Avoid notifying too often */
	g_get_current_time (&timeval);
	data->last_notification_time = timeval.tv_sec;

	applet_do_notify (applet,
	                  NOTIFY_URGENCY_LOW,
	                  _("Wi-Fi Networks Available"),
	                  _("Use the network menu to connect to a Wi-Fi network"),
	                  "nm-device-wireless",
	                  "dont-show",
	                  _("Don't show this message again"),
	                  wifi_available_dont_show_cb,
	                  applet);
	return FALSE;
}

static void
queue_avail_access_point_notification (NMDevice *device)
{
	struct ap_notification_data *data;

	data = g_object_get_data (G_OBJECT (device), "notify-wifi-avail-data");	
	if (data->id != 0)
		return;

	if (g_settings_get_boolean (data->applet->gsettings,
	                            PREF_SUPPRESS_WIFI_NETWORKS_AVAILABLE))
		return;

	data->id = g_timeout_add_seconds (3, idle_check_avail_access_point_notification, data);
}

static void
access_point_added_cb (NMDeviceWifi *device,
                       NMAccessPoint *ap,
                       gpointer user_data)
{
	NMApplet *applet = NM_APPLET  (user_data);

	add_hash_to_ap (ap);
	g_signal_connect (G_OBJECT (ap),
	                  "notify",
	                  G_CALLBACK (notify_ap_prop_changed_cb),
	                  applet);

	queue_avail_access_point_notification (NM_DEVICE (device));
	applet_schedule_update_menu (applet);
}

static void
access_point_removed_cb (NMDeviceWifi *device,
                         NMAccessPoint *ap,
                         gpointer user_data)
{
	NMApplet *applet = NM_APPLET  (user_data);
	NMAccessPoint *old;

	/* If this AP was the active AP, make sure ACTIVE_AP_TAG gets cleared from
	 * its device.
	 */
	old = _active_ap_get (applet, (NMDevice *) device);
	if (old == ap) {
		_active_ap_set (applet, (NMDevice *) device, NULL);
		applet_schedule_update_icon (applet);
		applet_schedule_update_menu (applet);
	}
}

static void
on_new_connection (NMClient *client,
                   NMRemoteConnection *connection,
                   gpointer datap)
{
	struct ap_notification_data *data = datap;
	queue_avail_access_point_notification (NM_DEVICE (data->device));
}

static void
free_ap_notification_data (gpointer user_data)
{
	struct ap_notification_data *data = user_data;
	NMClient *client = data->applet->nm_client;

	nm_clear_g_source (&data->id);

	if (client)
		g_signal_handler_disconnect (client, data->new_con_id);
	memset (data, 0, sizeof (*data));
	g_free (data);
}

static void
wifi_device_added (NMDevice *device, NMApplet *applet)
{
	NMDeviceWifi *wdev = NM_DEVICE_WIFI (device);
	const GPtrArray *aps;
	int i;
	struct ap_notification_data *data;
	guint id;

	g_signal_connect (wdev,
	                  "notify::" NM_DEVICE_WIFI_ACTIVE_ACCESS_POINT,
	                  G_CALLBACK (notify_active_ap_changed_cb),
	                  applet);

	g_signal_connect (wdev,
	                  "access-point-added",
	                  G_CALLBACK (access_point_added_cb),
	                  applet);

	g_signal_connect (wdev,
	                  "access-point-removed",
	                  G_CALLBACK (access_point_removed_cb),
	                  applet);

	/* Now create the per-device hooks for watching for available wifi
	 * connections.
	 */
	data = g_new0 (struct ap_notification_data, 1);
	data->applet = applet;
	data->device = wdev;
	/* We also need to hook up to the client to find out when we have new connections
	 * that might be candididates.  Keep the ID around so we can disconnect
	 * when the device is destroyed.
	 */ 
	id = g_signal_connect (applet->nm_client,
	                       NM_CLIENT_CONNECTION_ADDED,
	                       G_CALLBACK (on_new_connection),
	                       data);
	data->new_con_id = id;
	g_object_set_data_full (G_OBJECT (wdev), "notify-wifi-avail-data",
	                        data, free_ap_notification_data);

	queue_avail_access_point_notification (device);

	/* Hash all APs this device knows about */
	aps = nm_device_wifi_get_access_points (wdev);
	for (i = 0; aps && (i < aps->len); i++)
		add_hash_to_ap (g_ptr_array_index (aps, i));
}

static NMAccessPoint *
update_active_ap (NMDevice *device, NMDeviceState state, NMApplet *applet)
{
	NMAccessPoint *new = NULL;

	if (state == NM_DEVICE_STATE_PREPARE ||
	    state == NM_DEVICE_STATE_CONFIG ||
	    state == NM_DEVICE_STATE_IP_CONFIG ||
	    state == NM_DEVICE_STATE_NEED_AUTH ||
	    state == NM_DEVICE_STATE_ACTIVATED) {
		new = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (device));
	}

	_active_ap_set (applet, device, new);
	return new;
}

static void
wifi_device_state_changed (NMDevice *device,
                           NMDeviceState new_state,
                           NMDeviceState old_state,
                           NMDeviceStateReason reason,
                           NMApplet *applet)
{
	update_active_ap (device, new_state, applet);

	if (new_state == NM_DEVICE_STATE_DISCONNECTED)
		queue_avail_access_point_notification (device);
}

static void
wifi_notify_connected (NMDevice *device,
                       const char *msg,
                       NMApplet *applet)
{
	NMAccessPoint *ap;
	char *esc_ssid;
	char *ssid_msg;
	const char *signal_strength_icon;

	ap = _active_ap_get (applet, device);

	esc_ssid = get_ssid_utf8 (ap);

	if (!ap)
		signal_strength_icon = "nm-device-wireless";
	else
		signal_strength_icon = mobile_helper_get_quality_icon_name (nm_access_point_get_strength (ap));

	ssid_msg = g_strdup_printf (_("You are now connected to the Wi-Fi network '%s'."), esc_ssid);
	applet_do_notify_with_pref (applet, _("Connection Established"),
	                            ssid_msg, signal_strength_icon,
	                            PREF_DISABLE_CONNECTED_NOTIFICATIONS);
	g_free (ssid_msg);
	g_free (esc_ssid);
}

static void
wifi_get_icon (NMDevice *device,
               NMDeviceState state,
               NMConnection *connection,
               GdkPixbuf **out_pixbuf,
               const char **out_icon_name,
               char **tip,
               NMApplet *applet)
{
	NMSettingConnection *s_con;
	NMAccessPoint *ap;
	const char *id;
	guint8 strength;

	g_return_if_fail (out_icon_name && !*out_icon_name);
	g_return_if_fail (tip && !*tip);

	ap = _active_ap_get (applet, device);

	id = nm_device_get_iface (device);
	if (connection) {
		s_con = nm_connection_get_setting_connection (connection);
		id = nm_setting_connection_get_id (s_con);
	}

	switch (state) {
	case NM_DEVICE_STATE_PREPARE:
		*tip = g_strdup_printf (_("Preparing Wi-Fi network connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_CONFIG:
		*tip = g_strdup_printf (_("Configuring Wi-Fi network connection '%s'..."), id);
		break;
	case NM_DEVICE_STATE_NEED_AUTH:
		*tip = g_strdup_printf (_("User authentication required for Wi-Fi network '%s'..."), id);
		break;
	case NM_DEVICE_STATE_IP_CONFIG:
		*tip = g_strdup_printf (_("Requesting a Wi-Fi network address for '%s'..."), id);
		break;
	case NM_DEVICE_STATE_ACTIVATED:
		strength = ap ? nm_access_point_get_strength (ap) : 0;
		strength = MIN (strength, 100);

		*out_icon_name = mobile_helper_get_quality_icon_name (strength);

		if (ap) {
			char *ssid = get_ssid_utf8 (ap);

			*tip = g_strdup_printf (_("Wi-Fi network connection '%s' active: %s (%d%%)"),
			                        id, ssid, strength);
			g_free (ssid);
		} else
			*tip = g_strdup_printf (_("Wi-Fi network connection '%s' active"), id);
		break;
	default:
		break;
	}
}


static void
activate_existing_cb (GObject *client,
                      GAsyncResult *result,
                      gpointer user_data)
{
	GError *error = NULL;
	NMActiveConnection *active;

	active = nm_client_activate_connection_finish (NM_CLIENT (client), result, &error);
	g_clear_object (&active);
	if (error) {
		const char *text = _("Failed to activate connection");
		char *err_text = g_strdup_printf ("(%d) %s", error->code,
		                                  error->message ? error->message : _("Unknown error"));

		g_warning ("%s: %s", text, err_text);
		utils_show_error_dialog (_("Connection failure"), text, err_text, FALSE, NULL);
		g_free (err_text);
		g_error_free (error);
	}
	applet_schedule_update_icon (NM_APPLET (user_data));
}

static void
activate_new_cb (GObject *client,
                 GAsyncResult *result,
                 gpointer user_data)
{
	GError *error = NULL;
	NMActiveConnection *active;

	active = nm_client_add_and_activate_connection_finish (NM_CLIENT (client), result, &error);
	g_clear_object (&active);
	if (error) {
		const char *text = _("Failed to add new connection");
		char *err_text = g_strdup_printf ("(%d) %s", error->code,
		                                  error->message ? error->message : _("Unknown error"));

		g_warning ("%s: %s", text, err_text);
		utils_show_error_dialog (_("Connection failure"), text, err_text, FALSE, NULL);
		g_free (err_text);
		g_error_free (error);
	}
	applet_schedule_update_icon (NM_APPLET (user_data));
}

static void
wifi_dialog_response_cb (GtkDialog *foo,
                         gint response,
                         gpointer user_data)
{
	NMAWifiDialog *dialog = NMA_WIFI_DIALOG (foo);
	NMApplet *applet = NM_APPLET (user_data);
	NMConnection *connection = NULL, *fuzzy_match = NULL;
	NMDevice *device = NULL;
	NMAccessPoint *ap = NULL;
	GPtrArray *all;
	int i;

	if (response != GTK_RESPONSE_OK)
		goto done;

	/* nma_wifi_dialog_get_connection() returns a connection with the
	 * refcount incremented, so the caller must remember to unref it.
	 */
	connection = nma_wifi_dialog_get_connection (dialog, &device, &ap);
	g_assert (connection);
	g_assert (device);

	/* Find a similar connection and use that instead */
	all = applet_get_all_connections (applet);
	for (i = 0; i < all->len; i++) {
		if (nm_connection_compare (connection,
		                           NM_CONNECTION (all->pdata[i]),
		                           (NM_SETTING_COMPARE_FLAG_FUZZY | NM_SETTING_COMPARE_FLAG_IGNORE_ID))) {
			fuzzy_match = NM_CONNECTION (all->pdata[i]);
			break;
		}
	}
	g_ptr_array_unref (all);

	if (fuzzy_match) {
		nm_client_activate_connection_async (applet->nm_client,
		                                     fuzzy_match,
		                                     device,
		                                     ap ? nm_object_get_path (NM_OBJECT (ap)) : NULL,
		                                     NULL,
		                                     activate_existing_cb,
		                                     applet);
	} else {
		NMSetting *s_con;
		NMSettingWireless *s_wifi = NULL;
		const char *mode = NULL;

		/* Entirely new connection */

		/* Don't autoconnect adhoc networks by default for now */
		s_wifi = nm_connection_get_setting_wireless (connection);
		if (s_wifi)
			mode = nm_setting_wireless_get_mode (s_wifi);
		if (g_strcmp0 (mode, "adhoc") == 0 || g_strcmp0 (mode, "ap") == 0) {
			s_con = nm_connection_get_setting (connection, NM_TYPE_SETTING_CONNECTION);
			if (!s_con) {
				s_con = nm_setting_connection_new ();
				nm_connection_add_setting (connection, s_con);
			}
			g_object_set (G_OBJECT (s_con), NM_SETTING_CONNECTION_AUTOCONNECT, FALSE, NULL);
		}

		nm_client_add_and_activate_connection_async (applet->nm_client,
		                                             connection,
		                                             device,
		                                             ap ? nm_object_get_path (NM_OBJECT (ap)) : NULL,
		                                             NULL,
		                                             activate_new_cb,
		                                             applet);
	}

	/* Balance nma_wifi_dialog_get_connection() */
	g_object_unref (connection);

done:
	gtk_widget_hide (GTK_WIDGET (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static GVariant *
remove_unwanted_secrets (GVariant *secrets, gboolean keep_8021X)
{
	GVariant *copy, *setting_dict;
	const char *setting_name;
	GVariantBuilder conn_builder;
	GVariantIter conn_iter;

	g_variant_builder_init (&conn_builder, NM_VARIANT_TYPE_CONNECTION);
	g_variant_iter_init (&conn_iter, secrets);
	while (g_variant_iter_next (&conn_iter, "{&s@a{sv}}", &setting_name, &setting_dict)) {
		if (   !strcmp (setting_name, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME)
		    || (!strcmp (setting_name, NM_SETTING_802_1X_SETTING_NAME) && keep_8021X))
			g_variant_builder_add (&conn_builder, "{s@a{sv}}", setting_name, setting_dict);

		g_variant_unref (setting_dict);
	}
	copy = g_variant_builder_end (&conn_builder);
	g_variant_unref (secrets);

	return copy;
}
	

typedef struct {
	SecretsRequest req;

	GtkWidget *dialog;
} NMWifiInfo;

static void
free_wifi_info (SecretsRequest *req)
{
	NMWifiInfo *info = (NMWifiInfo *) req;

	if (info->dialog) {
		gtk_widget_hide (info->dialog);
		gtk_widget_destroy (info->dialog);
		info->dialog = NULL;
	}
}

static void
get_secrets_dialog_response_cb (GtkDialog *foo,
                                gint response,
                                gpointer user_data)
{
	SecretsRequest *req = user_data;
	NMWifiInfo *info = (NMWifiInfo *) req;
	NMAWifiDialog *dialog = NMA_WIFI_DIALOG (info->dialog);
	NMConnection *connection = NULL;
	NMSettingWirelessSecurity *s_wireless_sec;
	GVariant *secrets = NULL;
	const char *key_mgmt, *auth_alg;
	gboolean keep_8021X = FALSE;
	GError *error = NULL;

	if (response != GTK_RESPONSE_OK) {
		g_set_error (&error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_USER_CANCELED,
		             "%s.%d (%s): canceled",
		             __FILE__, __LINE__, __func__);
		goto done;
	}

	connection = nma_wifi_dialog_get_connection (dialog, NULL, NULL);
	if (!connection) {
		g_set_error (&error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_FAILED,
		             "%s.%d (%s): couldn't get connection from Wi-Fi dialog.",
		             __FILE__, __LINE__, __func__);
		goto done;
	}

	/* Second-guess which setting NM wants secrets for. */
	s_wireless_sec = nm_connection_get_setting_wireless_security (connection);
	if (!s_wireless_sec) {
		g_set_error (&error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_INVALID_CONNECTION,
		             "%s.%d (%s): requested setting '802-11-wireless-security'"
		             " didn't exist in the connection.",
		             __FILE__, __LINE__, __func__);
		goto done;  /* Unencrypted */
	}

	secrets = nm_connection_to_dbus (connection, NM_CONNECTION_SERIALIZE_ONLY_SECRETS);
	if (!secrets) {
		g_set_error (&error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_FAILED,
		             "%s.%d (%s): failed to hash connection '%s'.",
		             __FILE__, __LINE__, __func__, nm_connection_get_id (connection));
		goto done;
	}
	/* If the user chose an 802.1x-based auth method, return 802.1x secrets,
	 * not wireless secrets.  Can happen with Dynamic WEP, because NM doesn't
	 * know the capabilities of the AP (since Dynamic WEP APs don't broadcast
	 * beacons), and therefore defaults to requesting WEP secrets from the
	 * wireless-security setting, not the 802.1x setting.
	 */
	key_mgmt = nm_setting_wireless_security_get_key_mgmt (s_wireless_sec);
	if (!strcmp (key_mgmt, "ieee8021x") || !strcmp (key_mgmt, "wpa-eap")) {
		/* LEAP secrets aren't in the 802.1x setting */
		auth_alg = nm_setting_wireless_security_get_auth_alg (s_wireless_sec);
		if (!auth_alg || strcmp (auth_alg, "leap")) {
			NMSetting8021x *s_8021x;

			s_8021x = nm_connection_get_setting_802_1x (connection);
			if (!s_8021x) {
				g_set_error (&error,
				             NM_SECRET_AGENT_ERROR,
				             NM_SECRET_AGENT_ERROR_INVALID_CONNECTION,
				             "%s.%d (%s): requested setting '802-1x' didn't"
				             " exist in the connection.",
				             __FILE__, __LINE__, __func__);
				goto done;
			}
			keep_8021X = TRUE;
		}
	}

	/* Remove all not-relevant secrets (inner dicts) */
	secrets = remove_unwanted_secrets (secrets, keep_8021X);
	g_variant_take_ref (secrets);

done:
	applet_secrets_request_complete (req, secrets, error);
	applet_secrets_request_free (req);

	if (secrets)
		g_variant_unref (secrets);
	if (connection)
		nm_connection_clear_secrets (connection);
}

static gboolean
wifi_get_secrets (SecretsRequest *req, GError **error)
{
	NMWifiInfo *info = (NMWifiInfo *) req;

	g_return_val_if_fail (!info->dialog, FALSE);

	info->dialog = nma_wifi_dialog_new (req->applet->nm_client, req->connection, NULL, NULL, TRUE);
	if (info->dialog) {
		applet_secrets_request_set_free_func (req, free_wifi_info);
		g_signal_connect (info->dialog, "response",
		                  G_CALLBACK (get_secrets_dialog_response_cb),
		                  info);
		show_ignore_focus_stealing_prevention (info->dialog);
	} else {
		g_set_error (error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_FAILED,
		             "%s.%d (%s): couldn't display secrets UI",
		             __FILE__, __LINE__, __func__);
	}
	return !!info->dialog;
}

NMADeviceClass *
applet_device_wifi_get_class (NMApplet *applet)
{
	NMADeviceClass *dclass;

	dclass = g_slice_new0 (NMADeviceClass);
	if (!dclass)
		return NULL;

	dclass->new_auto_connection = wifi_new_auto_connection;
	dclass->add_menu_item = wifi_add_menu_item;
	dclass->device_added = wifi_device_added;
	dclass->device_state_changed = wifi_device_state_changed;
	dclass->notify_connected = wifi_notify_connected;
	dclass->get_icon = wifi_get_icon;
	dclass->get_secrets = wifi_get_secrets;
	dclass->secrets_request_size = sizeof (NMWifiInfo);

	return dclass;
}

