/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
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
 * (C) Copyright 2008 - 2011 Red Hat, Inc.
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <NetworkManager.h>
#include <nm-setting-connection.h>
#include <nm-setting-wireless.h>
#include <nm-setting-wireless-security.h>
#include <nm-setting-8021x.h>
#include <nm-utils.h>

#include "wireless-security.h"
#include "page-wifi.h"
#include "page-wifi-security.h"
#include "nm-connection-editor.h"

G_DEFINE_TYPE (CEPageWifiSecurity, ce_page_wifi_security, CE_TYPE_PAGE)

#define CE_PAGE_WIFI_SECURITY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_WIFI_SECURITY, CEPageWifiSecurityPrivate))

typedef struct {
	GtkSizeGroup *group;
	GtkComboBox *security_combo;
	NM80211Mode mode;
} CEPageWifiSecurityPrivate;

#define S_NAME_COLUMN   0
#define S_SEC_COLUMN    1
#define S_ADHOC_VALID_COLUMN  2
#define S_HOTSPOT_VALID_COLUMN  3

static gboolean
find_proto (NMSettingWirelessSecurity *sec, const char *item)
{
	guint32 i;

	for (i = 0; i < nm_setting_wireless_security_get_num_protos (sec); i++) {
		if (!strcmp (item, nm_setting_wireless_security_get_proto (sec, i)))
			return TRUE;
	}
	return FALSE;
}

static NMUtilsSecurityType
get_default_type_for_security (NMSettingWirelessSecurity *sec)
{
	const char *key_mgmt, *auth_alg;

	g_return_val_if_fail (sec != NULL, NMU_SEC_NONE);

	key_mgmt = nm_setting_wireless_security_get_key_mgmt (sec);
	auth_alg = nm_setting_wireless_security_get_auth_alg (sec);

	/* No IEEE 802.1x */
	if (!strcmp (key_mgmt, "none"))
		return NMU_SEC_STATIC_WEP;

	if (!strcmp (key_mgmt, "ieee8021x")) {
		if (auth_alg && !strcmp (auth_alg, "leap"))
			return NMU_SEC_LEAP;
		return NMU_SEC_DYNAMIC_WEP;
	}

	if (   !strcmp (key_mgmt, "wpa-none")
	    || !strcmp (key_mgmt, "wpa-psk")) {
		if (find_proto (sec, "rsn"))
			return NMU_SEC_WPA2_PSK;
		else if (find_proto (sec, "wpa"))
			return NMU_SEC_WPA_PSK;
		else
			return NMU_SEC_WPA_PSK;
	}

	if (!strcmp (key_mgmt, "wpa-eap")) {
		if (find_proto (sec, "rsn"))
			return NMU_SEC_WPA2_ENTERPRISE;
		else if (find_proto (sec, "wpa"))
			return NMU_SEC_WPA_ENTERPRISE;
		else
			return NMU_SEC_WPA_ENTERPRISE;
	}

	return NMU_SEC_INVALID;
}

static void
stuff_changed_cb (WirelessSecurity *sec, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
wsec_size_group_clear (GtkSizeGroup *group)
{
	GSList *children;
	GSList *iter;

	g_return_if_fail (group != NULL);

	children = gtk_size_group_get_widgets (group);
	for (iter = children; iter; iter = g_slist_next (iter))
		gtk_size_group_remove_widget (group, GTK_WIDGET (iter->data));
}

static WirelessSecurity *
wireless_security_combo_get_active (CEPageWifiSecurity *self)
{
	CEPageWifiSecurityPrivate *priv = CE_PAGE_WIFI_SECURITY_GET_PRIVATE (self);
	GtkTreeIter iter;
	GtkTreeModel *model;
	WirelessSecurity *sec = NULL;

	model = gtk_combo_box_get_model (priv->security_combo);
	gtk_combo_box_get_active_iter (priv->security_combo, &iter);
	gtk_tree_model_get (model, &iter, S_SEC_COLUMN, &sec, -1);

	return sec;
}

static void
wireless_security_combo_changed (GtkComboBox *combo,
                                 gpointer user_data)
{
	CEPageWifiSecurity *self = CE_PAGE_WIFI_SECURITY (user_data);
	CEPageWifiSecurityPrivate *priv = CE_PAGE_WIFI_SECURITY_GET_PRIVATE (self);
	GtkWidget *vbox;
	GList *elt, *children;
	WirelessSecurity *sec;

	vbox = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (self)->builder, "wifi_security_vbox"));
	g_assert (vbox);

	wsec_size_group_clear (priv->group);

	/* Remove any previous wifi security widgets */
	children = gtk_container_get_children (GTK_CONTAINER (vbox));
	for (elt = children; elt; elt = g_list_next (elt))
		gtk_container_remove (GTK_CONTAINER (vbox), GTK_WIDGET (elt->data));

	sec = wireless_security_combo_get_active (self);
	if (sec) {
		GtkWidget *sec_widget;
		GtkWidget *widget, *parent;

		sec_widget = wireless_security_get_widget (sec);
		g_assert (sec_widget);
		parent = gtk_widget_get_parent (sec_widget);
		if (parent)
			gtk_container_remove (GTK_CONTAINER (parent), sec_widget);

		widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (self)->builder, "wifi_security_combo_label"));
		gtk_size_group_add_widget (priv->group, widget);
		wireless_security_add_to_size_group (sec, priv->group);

		gtk_container_add (GTK_CONTAINER (vbox), sec_widget);
		wireless_security_unref (sec);
	}

	ce_page_changed (CE_PAGE (self));
}

static void
add_security_item (CEPageWifiSecurity *self,
                   WirelessSecurity *sec,
                   GtkListStore *model,
                   GtkTreeIter *iter,
                   const char *text,
                   gboolean adhoc_valid,
                   gboolean hotspot_valid)
{
	wireless_security_set_changed_notify (sec, stuff_changed_cb, self);
	gtk_list_store_append (model, iter);
	gtk_list_store_set (model, iter,
	                    S_NAME_COLUMN, text,
	                    S_SEC_COLUMN, sec,
	                    S_ADHOC_VALID_COLUMN, adhoc_valid,
	                    S_HOTSPOT_VALID_COLUMN, hotspot_valid,
	                    -1);
	wireless_security_unref (sec);
}

static void
set_sensitive (GtkCellLayout *cell_layout,
               GtkCellRenderer *cell,
               GtkTreeModel *tree_model,
               GtkTreeIter *iter,
               gpointer data)
{
	NM80211Mode *mode = data;
	gboolean sensitive = TRUE;

	if (*mode == NM_802_11_MODE_ADHOC)
		gtk_tree_model_get (tree_model, iter, S_ADHOC_VALID_COLUMN, &sensitive, -1);
	else if (*mode == NM_802_11_MODE_AP)
		gtk_tree_model_get (tree_model, iter, S_HOTSPOT_VALID_COLUMN, &sensitive, -1);

	g_object_set (cell, "sensitive", sensitive, NULL);
}

static gboolean
security_valid (NMUtilsSecurityType sectype, NM80211Mode mode)
{
	guint32 dev_caps = 0;

	/* Fake some device capabilities here since we don't know about the
	 * NMDevice object to get the card's real capabilities.
	 */
	dev_caps =   NM_WIFI_DEVICE_CAP_CIPHER_WEP40
	           | NM_WIFI_DEVICE_CAP_CIPHER_WEP104
	           | NM_WIFI_DEVICE_CAP_CIPHER_TKIP
	           | NM_WIFI_DEVICE_CAP_CIPHER_CCMP
	           | NM_WIFI_DEVICE_CAP_WPA
	           | NM_WIFI_DEVICE_CAP_RSN;

	switch (mode) {
	case NM_802_11_MODE_AP:
		return nm_utils_ap_mode_security_valid (sectype, NM_WIFI_DEVICE_CAP_AP);
	case NM_802_11_MODE_ADHOC:
	case NM_802_11_MODE_INFRA:
	default:
		return nm_utils_security_valid (sectype,
		                                dev_caps,
		                                FALSE,
		                                (mode == NM_802_11_MODE_ADHOC),
		                                0, 0, 0);
	}
	g_assert_not_reached ();
}

static void
finish_setup (CEPageWifiSecurity *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPage *parent = CE_PAGE (self);
	CEPageWifiSecurityPrivate *priv = CE_PAGE_WIFI_SECURITY_GET_PRIVATE (self);
	NMSettingWireless *s_wireless;
	NMSettingWirelessSecurity *s_wireless_sec;
	NMConnection *connection = parent->connection;
	NM80211Mode mode = NM_802_11_MODE_INFRA;
	GtkListStore *sec_model;
	GtkTreeIter iter;
	NMUtilsSecurityType default_type = NMU_SEC_NONE;
	int active = -1;
	int item = 0;
	GtkComboBox *combo;
	GtkCellRenderer *renderer;

	if (error)
		return;

	s_wireless = nm_connection_get_setting_wireless (connection);
	g_assert (s_wireless);

	if (!g_strcmp0 (nm_setting_wireless_get_mode (s_wireless), "adhoc"))
		mode = NM_802_11_MODE_ADHOC;
	else if (!g_strcmp0 (nm_setting_wireless_get_mode (s_wireless), "ap"))
		mode = NM_802_11_MODE_AP;

	s_wireless_sec = nm_connection_get_setting_wireless_security (connection);

	if (s_wireless_sec)
		default_type = get_default_type_for_security (s_wireless_sec);

	sec_model = gtk_list_store_new (4, G_TYPE_STRING, wireless_security_get_type (), G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

	if (security_valid (NMU_SEC_NONE, mode)) {
		gtk_list_store_append (sec_model, &iter);
		gtk_list_store_set (sec_model, &iter,
		                    S_NAME_COLUMN, C_("Wi-Fi/Ethernet security", "None"),
		                    S_ADHOC_VALID_COLUMN, TRUE,
		                    S_HOTSPOT_VALID_COLUMN, TRUE,
		                    -1);
		if (default_type == NMU_SEC_NONE)
			active = item;
		item++;
	}

	if (security_valid (NMU_SEC_STATIC_WEP, mode)) {
		WirelessSecurityWEPKey *ws_wep;
		NMWepKeyType wep_type = NM_WEP_KEY_TYPE_KEY;

		if (default_type == NMU_SEC_STATIC_WEP) {
			NMSettingWirelessSecurity *s_wsec;

			s_wsec = nm_connection_get_setting_wireless_security (connection);
			if (s_wsec)
				wep_type = nm_setting_wireless_security_get_wep_key_type (s_wsec);
			if (wep_type == NM_WEP_KEY_TYPE_UNKNOWN)
				wep_type = NM_WEP_KEY_TYPE_KEY;
		}

		ws_wep = ws_wep_key_new (connection, NM_WEP_KEY_TYPE_KEY, FALSE, FALSE);
		if (ws_wep) {
			add_security_item (self, WIRELESS_SECURITY (ws_wep), sec_model,
			                   &iter, _("WEP 40/128-bit Key (Hex or ASCII)"),
			                   TRUE, TRUE);
			if ((active < 0) && (default_type == NMU_SEC_STATIC_WEP) && (wep_type == NM_WEP_KEY_TYPE_KEY))
				active = item;
			item++;
		}

		ws_wep = ws_wep_key_new (connection, NM_WEP_KEY_TYPE_PASSPHRASE, FALSE, FALSE);
		if (ws_wep) {
			add_security_item (self, WIRELESS_SECURITY (ws_wep), sec_model,
			                   &iter, _("WEP 128-bit Passphrase"), TRUE, TRUE);
			if ((active < 0) && (default_type == NMU_SEC_STATIC_WEP) && (wep_type == NM_WEP_KEY_TYPE_PASSPHRASE))
				active = item;
			item++;
		}
	}

	if (security_valid (NMU_SEC_LEAP, mode)) {
		WirelessSecurityLEAP *ws_leap;

		ws_leap = ws_leap_new (connection, FALSE);
		if (ws_leap) {
			add_security_item (self, WIRELESS_SECURITY (ws_leap), sec_model,
			                   &iter, _("LEAP"), FALSE, FALSE);
			if ((active < 0) && (default_type == NMU_SEC_LEAP))
				active = item;
			item++;
		}
	}

	if (security_valid (NMU_SEC_DYNAMIC_WEP, mode)) {
		WirelessSecurityDynamicWEP *ws_dynamic_wep;

		ws_dynamic_wep = ws_dynamic_wep_new (connection, TRUE, FALSE);
		if (ws_dynamic_wep) {
			add_security_item (self, WIRELESS_SECURITY (ws_dynamic_wep), sec_model,
			                   &iter, _("Dynamic WEP (802.1x)"), FALSE, FALSE);
			if ((active < 0) && (default_type == NMU_SEC_DYNAMIC_WEP))
				active = item;
			item++;
		}
	}

	if (security_valid (NMU_SEC_WPA_PSK, mode) || security_valid (NMU_SEC_WPA2_PSK, mode)) {
		WirelessSecurityWPAPSK *ws_wpa_psk;

		ws_wpa_psk = ws_wpa_psk_new (connection, FALSE);
		if (ws_wpa_psk) {
			add_security_item (self, WIRELESS_SECURITY (ws_wpa_psk), sec_model,
			                   &iter, _("WPA & WPA2 Personal"), FALSE, TRUE);
			if ((active < 0) && ((default_type == NMU_SEC_WPA_PSK) || (default_type == NMU_SEC_WPA2_PSK)))
				active = item;
			item++;
		}
	}

	if (security_valid (NMU_SEC_WPA_ENTERPRISE, mode) || security_valid (NMU_SEC_WPA2_ENTERPRISE, mode)) {
		WirelessSecurityWPAEAP *ws_wpa_eap;

		ws_wpa_eap = ws_wpa_eap_new (connection, TRUE, FALSE);
		if (ws_wpa_eap) {
			add_security_item (self, WIRELESS_SECURITY (ws_wpa_eap), sec_model,
			                   &iter, _("WPA & WPA2 Enterprise"), FALSE, FALSE);
			if ((active < 0) && ((default_type == NMU_SEC_WPA_ENTERPRISE) || (default_type == NMU_SEC_WPA2_ENTERPRISE)))
				active = item;
			item++;
		}
	}

	combo = GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "wifi_security_combo"));
	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (sec_model));
	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "text", S_NAME_COLUMN, NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), renderer, set_sensitive, &priv->mode, NULL);

	gtk_combo_box_set_active (combo, active < 0 ? 0 : (guint32) active);
	g_object_unref (G_OBJECT (sec_model));

	priv->security_combo = combo;

	wireless_security_combo_changed (combo, self);
	g_signal_connect (combo, "changed",
	                  G_CALLBACK (wireless_security_combo_changed),
	                  self);
}

CEPage *
ce_page_wifi_security_new (NMConnectionEditor *editor,
                           NMConnection *connection,
                           GtkWindow *parent_window,
                           NMClient *client,
                           NMRemoteSettings *settings,
                           const char **out_secrets_setting_name,
                           GError **error)
{
	CEPageWifiSecurity *self;
	NMSettingWireless *s_wireless;
	NMSettingWirelessSecurity *s_wsec = NULL;
	NMUtilsSecurityType default_type = NMU_SEC_NONE;

	s_wireless = nm_connection_get_setting_wireless (connection);
	if (!s_wireless) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load Wi-Fi security user interface; missing Wi-Fi setting."));
		return NULL;
	}

	self = CE_PAGE_WIFI_SECURITY (ce_page_new (CE_TYPE_PAGE_WIFI_SECURITY,
	                                           editor,
	                                           connection,
	                                           parent_window,
	                                           client,
	                                           settings,
	                                           UIDIR "/ce-page-wifi-security.ui",
	                                           "WifiSecurityPage",
	                                           _("Wi-Fi Security")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load Wi-Fi security user interface."));
		return NULL;
	}

	CE_PAGE_WIFI_SECURITY_GET_PRIVATE (self)->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	s_wsec = nm_connection_get_setting_wireless_security (connection);

	if (s_wsec)
		default_type = get_default_type_for_security (s_wsec);

	/* Get secrets if the connection is not 802.1x enabled */
	if (   default_type == NMU_SEC_STATIC_WEP
	    || default_type == NMU_SEC_LEAP
	    || default_type == NMU_SEC_WPA_PSK
	    || default_type == NMU_SEC_WPA2_PSK) {
		*out_secrets_setting_name = NM_SETTING_WIRELESS_SECURITY_SETTING_NAME;
	}

	/* Or if it is 802.1x enabled */
	if (   default_type == NMU_SEC_DYNAMIC_WEP
	    || default_type == NMU_SEC_WPA_ENTERPRISE
	    || default_type == NMU_SEC_WPA2_ENTERPRISE) {
		*out_secrets_setting_name = NM_SETTING_802_1X_SETTING_NAME;
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ce_page_wifi_security_init (CEPageWifiSecurity *self)
{
}

static void
dispose (GObject *object)
{
	CEPageWifiSecurityPrivate *priv = CE_PAGE_WIFI_SECURITY_GET_PRIVATE (object);

	g_clear_object (&priv->group);

	G_OBJECT_CLASS (ce_page_wifi_security_parent_class)->dispose (object);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageWifiSecurity *self = CE_PAGE_WIFI_SECURITY (page);
	CEPageWifiSecurityPrivate *priv = CE_PAGE_WIFI_SECURITY_GET_PRIVATE (self);
	NMSettingWireless *s_wireless;
	WirelessSecurity *sec;
	gboolean valid = FALSE;
	const char *mode;

	s_wireless = nm_connection_get_setting_wireless (connection);
	g_assert (s_wireless);

	/* Kernel Ad-Hoc WPA support is busted; it creates open networks.  Disable
	 * WPA when Ad-Hoc is selected.  set_sensitive() will pick up priv->mode
	 * and do the right thing.
	 */
	mode = nm_setting_wireless_get_mode (s_wireless);
	if (g_strcmp0 (mode, NM_SETTING_WIRELESS_MODE_ADHOC) == 0)
		priv->mode = NM_802_11_MODE_ADHOC;
	else if (g_strcmp0 (mode, NM_SETTING_WIRELESS_MODE_AP) == 0)
		priv->mode = NM_802_11_MODE_AP;
	else
		priv->mode = NM_802_11_MODE_INFRA;

	sec = wireless_security_combo_get_active (self);
	if (sec) {
		const GByteArray *ssid = nm_setting_wireless_get_ssid (s_wireless);

		if (ssid) {
			valid = wireless_security_validate (sec, error);
			if (valid)
				wireless_security_fill_connection (sec, connection);
		} else {
			g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing SSID"));
			valid = FALSE;
		}

		if (priv->mode == NM_802_11_MODE_ADHOC) {
			if (!wireless_security_adhoc_compatible (sec)) {
				g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Security not compatible with Ad-Hoc mode"));
				valid = FALSE;
			}
		}

		wireless_security_unref (sec);
	} else {
		/* No security, unencrypted */
		nm_connection_remove_setting (connection, NM_TYPE_SETTING_WIRELESS_SECURITY);
		nm_connection_remove_setting (connection, NM_TYPE_SETTING_802_1X);
		valid = TRUE;
	}

	return valid;
}

static void
ce_page_wifi_security_class_init (CEPageWifiSecurityClass *wireless_security_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (wireless_security_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (wireless_security_class);

	g_type_class_add_private (object_class, sizeof (CEPageWifiSecurityPrivate));

	/* virtual methods */
	object_class->dispose = dispose;

	parent_class->ce_page_validate_v = ce_page_validate_v;
}
