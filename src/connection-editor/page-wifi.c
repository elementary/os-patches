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
 * Copyright 2008 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>
#include <math.h>

#include "nm-connection-editor.h"
#include "page-wifi.h"

G_DEFINE_TYPE (CEPageWifi, ce_page_wifi, CE_TYPE_PAGE)

#define CE_PAGE_WIFI_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_WIFI, CEPageWifiPrivate))

typedef struct {
	NMSettingWireless *setting;

	GtkEntry *ssid;
	GtkComboBoxText *bssid;
	GtkComboBoxText *device_combo; /* Device identification (ifname and/or MAC) */
	GtkEntry *cloned_mac;          /* Cloned MAC - used for MAC spoofing */
	GtkComboBox *mode;
	GtkComboBox *band;
	GtkSpinButton *channel;
	GtkSpinButton *rate;
	GtkSpinButton *tx_power;
	GtkSpinButton *mtu;

	GtkSizeGroup *group;

	int last_channel;
} CEPageWifiPrivate;

static void
wifi_private_init (CEPageWifi *self)
{
	CEPageWifiPrivate *priv = CE_PAGE_WIFI_GET_PRIVATE (self);
	GtkBuilder *builder;
	GtkWidget *widget;
	GtkWidget *vbox;
	GtkLabel *label;

	builder = CE_PAGE (self)->builder;

	priv->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	priv->ssid     = GTK_ENTRY (gtk_builder_get_object (builder, "wifi_ssid"));
	priv->cloned_mac = GTK_ENTRY (gtk_builder_get_object (builder, "wifi_cloned_mac"));
	priv->mode     = GTK_COMBO_BOX (gtk_builder_get_object (builder, "wifi_mode"));
	priv->band     = GTK_COMBO_BOX (gtk_builder_get_object (builder, "wifi_band"));
	priv->channel  = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "wifi_channel"));

	/* BSSID */
	priv->bssid = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new_with_entry ());
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->bssid), 0);
	gtk_widget_set_tooltip_text (GTK_WIDGET (priv->bssid),
	                             _("This option locks this connection to the Wi-Fi access point (AP) specified by the BSSID entered here.  Example: 00:11:22:33:44:55"));

	vbox = GTK_WIDGET (gtk_builder_get_object (builder, "wifi_bssid_vbox"));
	gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (priv->bssid));
	gtk_widget_set_halign (GTK_WIDGET (priv->bssid), GTK_ALIGN_FILL);
	gtk_widget_show_all (GTK_WIDGET (priv->bssid));

	/* Device MAC */
	priv->device_combo = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new_with_entry ());
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->device_combo), 0);
	gtk_widget_set_tooltip_text (GTK_WIDGET (priv->device_combo),
	                             _("This option locks this connection to the network device specified "
	                               "either by its interface name or permanent MAC or both. Examples: "
	                               "\"wlan0\", \"3C:97:0E:42:1A:19\", \"wlan0 (3C:97:0E:42:1A:19)\""));

	vbox = GTK_WIDGET (gtk_builder_get_object (builder, "wifi_device_vbox"));
	gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (priv->device_combo));
	gtk_widget_set_halign (GTK_WIDGET (priv->device_combo), GTK_ALIGN_FILL);
	gtk_widget_show_all (GTK_WIDGET (priv->device_combo));

	/* Set mnemonic widget for Device label */
	label = GTK_LABEL (gtk_builder_get_object (builder, "wifi_device_label"));
	gtk_label_set_mnemonic_widget (label, GTK_WIDGET (priv->device_combo));

	priv->rate     = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "wifi_rate"));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "rate_units"));
	gtk_size_group_add_widget (priv->group, widget);

	priv->tx_power = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "wifi_tx_power"));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "tx_power_units"));
	gtk_size_group_add_widget (priv->group, widget);

	priv->mtu      = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "wifi_mtu"));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "mtu_units"));
	gtk_size_group_add_widget (priv->group, widget);
}

static gboolean
band_helper (CEPageWifi *self, gboolean *aband, gboolean *gband)
{
	CEPageWifiPrivate *priv = CE_PAGE_WIFI_GET_PRIVATE (self);

	switch (gtk_combo_box_get_active (priv->band)) {
	case 1: /* A */
		*gband = FALSE;
		return TRUE;
	case 2: /* B/G */
		*aband = FALSE;
		return TRUE;
	default:
		return FALSE;
	}
}

static gint
channel_spin_input_cb (GtkSpinButton *spin, gdouble *new_val, gpointer user_data)
{
	CEPageWifi *self = CE_PAGE_WIFI (user_data);
	gdouble channel;
	guint32 int_channel = 0;
	gboolean aband = TRUE;
	gboolean gband = TRUE;

	if (!band_helper (self, &aband, &gband))
		return GTK_INPUT_ERROR;

	channel = g_strtod (gtk_entry_get_text (GTK_ENTRY (spin)), NULL);
	if (channel - floor (channel) < ceil (channel) - channel)
		int_channel = floor (channel);
	else
		int_channel = ceil (channel);

	if (nm_utils_wifi_channel_to_freq (int_channel, aband ? "a" : "bg") == -1)
		return GTK_INPUT_ERROR;

	*new_val = channel;
	return 1;
}

static gint
channel_spin_output_cb (GtkSpinButton *spin, gpointer user_data)
{
	CEPageWifi *self = CE_PAGE_WIFI (user_data);
	CEPageWifiPrivate *priv = CE_PAGE_WIFI_GET_PRIVATE (self);
	int channel;
	gchar *buf = NULL;
	guint32 freq;
	gboolean aband = TRUE;
	gboolean gband = TRUE;

	if (!band_helper (self, &aband, &gband))
		buf = g_strdup (_("default"));
	else {
		channel = gtk_spin_button_get_value_as_int (spin);
		if (channel == 0)
			buf = g_strdup (_("default"));
		else {
			int direction = 0;
			freq = nm_utils_wifi_channel_to_freq (channel, aband ? "a" : "bg");
			if (freq == -1) {
				if (priv->last_channel < channel)
					direction = 1;
				else if (priv->last_channel > channel)
					direction = -1;
				channel = nm_utils_wifi_find_next_channel (channel, direction, aband ? "a" : "bg");
				gtk_spin_button_set_value (spin, channel);
				freq = nm_utils_wifi_channel_to_freq (channel, aband ? "a" : "bg");
				if (freq == -1) {
					g_warning ("%s: invalid channel %d!", __func__, channel);
					gtk_spin_button_set_value (spin, 0);
					goto out;
				}

			}
			/* Set spin button to zero to go to "default" from the lowest channel */
			if (direction == -1 && priv->last_channel == channel) {
				buf = g_strdup_printf (_("default"));
				gtk_spin_button_set_value (spin, 0);
				channel = 0;
			} else
				buf = g_strdup_printf (_("%u (%u MHz)"), channel, freq);
		}
		priv->last_channel = channel;
	}

	if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin))))
		gtk_entry_set_text (GTK_ENTRY (spin), buf);

out:
	g_free (buf);
	return 1;
}

static void
band_value_changed_cb (GtkComboBox *box, gpointer user_data)
{
	CEPageWifi *self = CE_PAGE_WIFI (user_data);
	CEPageWifiPrivate *priv = CE_PAGE_WIFI_GET_PRIVATE (self);
	gboolean sensitive;

	priv->last_channel = 0;
	gtk_spin_button_set_value (priv->channel, 0);
 
 	switch (gtk_combo_box_get_active (GTK_COMBO_BOX (box))) {
 	case 1: /* A */
 	case 2: /* B/G */
		sensitive = TRUE;
 		break;
 	default:
		sensitive = FALSE;
 		break;
 	}

	gtk_widget_set_sensitive (GTK_WIDGET (priv->channel), sensitive);

	ce_page_changed (CE_PAGE (self));
}

static void
mode_combo_changed_cb (GtkComboBox *combo,
                       gpointer user_data)
{
	CEPageWifi *self = CE_PAGE_WIFI (user_data);
	CEPageWifiPrivate *priv = CE_PAGE_WIFI_GET_PRIVATE (self);
	CEPage *parent = CE_PAGE (self);
	GtkWidget *widget_band_label, *widget_chan_label, *widget_bssid_label;
	gboolean show_freq = FALSE;
	gboolean show_bssid = TRUE;
	gboolean hotspot = FALSE;

 	switch (gtk_combo_box_get_active (GTK_COMBO_BOX (combo))) {
	case 1: /* hotspot */
		hotspot = TRUE;
		/* fallthrough */
 	case 2: /* adhoc */
		/* BSSID is random and is created by kernel for Ad-Hoc networks
		 * http://lxr.linux.no/linux+v3.7.6/net/mac80211/ibss.c#L685
		 * For AP-mode, the BSSID is the MAC address of the device.
		 */
		show_bssid = FALSE;
		show_freq = TRUE;
 		break;
 	default: /* infrastructure */
		show_freq = FALSE;
 		break;
 	}
	nm_connection_editor_inter_page_set_value (parent->editor,
	                                           INTER_PAGE_CHANGE_WIFI_MODE,
	                                           GUINT_TO_POINTER (hotspot));

	widget_band_label = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wifi_band_label"));
	widget_chan_label = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wifi_channel_label"));
	widget_bssid_label = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wifi_bssid_label"));

	gtk_widget_set_visible (widget_band_label, show_freq);
	gtk_widget_set_sensitive (widget_band_label, show_freq);
	gtk_widget_set_visible (GTK_WIDGET (priv->band), show_freq);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->band), show_freq);
	gtk_widget_set_visible (widget_chan_label, show_freq);
	gtk_widget_set_sensitive (widget_chan_label, show_freq);
	gtk_widget_set_visible (GTK_WIDGET (priv->channel), show_freq);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->channel), show_freq);

	gtk_widget_set_visible (widget_bssid_label, show_bssid);
	gtk_widget_set_sensitive (widget_bssid_label, show_bssid);
	gtk_widget_set_visible (GTK_WIDGET (priv->bssid), show_bssid);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->bssid), show_bssid);

	ce_page_changed (CE_PAGE (self));
}

static void
populate_ui (CEPageWifi *self)
{
	CEPageWifiPrivate *priv = CE_PAGE_WIFI_GET_PRIVATE (self);
	NMSettingWireless *setting = priv->setting;
	GBytes *ssid;
	const char *mode;
	const char *band;
	int band_idx = 0;
	int rate_def;
	int tx_power_def;
	int mtu_def;
	char *utf8_ssid;
	const char *s_ifname, *s_mac, *s_bssid;
	GPtrArray *bssid_array;
	char **bssid_list;
	guint32 idx;

	rate_def = ce_get_property_default (NM_SETTING (setting), NM_SETTING_WIRELESS_RATE);
	g_signal_connect (priv->rate, "output",
	                  G_CALLBACK (ce_spin_output_with_automatic),
	                  GINT_TO_POINTER (rate_def));
	g_signal_connect_swapped (priv->rate, "value-changed", G_CALLBACK (ce_page_changed), self);

	tx_power_def = ce_get_property_default (NM_SETTING (setting), NM_SETTING_WIRELESS_TX_POWER);
	g_signal_connect (priv->tx_power, "output",
	                  G_CALLBACK (ce_spin_output_with_automatic),
	                  GINT_TO_POINTER (tx_power_def));
	g_signal_connect_swapped (priv->tx_power, "value-changed", G_CALLBACK (ce_page_changed), self);

	mtu_def = ce_get_property_default (NM_SETTING (setting), NM_SETTING_WIRELESS_MTU);
	g_signal_connect (priv->mtu, "output",
	                  G_CALLBACK (ce_spin_output_with_automatic),
	                  GINT_TO_POINTER (mtu_def));
	g_signal_connect_swapped (priv->mtu, "value-changed", G_CALLBACK (ce_page_changed), self);

	ssid = nm_setting_wireless_get_ssid (setting);
	mode = nm_setting_wireless_get_mode (setting);
	band = nm_setting_wireless_get_band (setting);

	if (ssid)
		utf8_ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL),
		                                   g_bytes_get_size (ssid));
	else
		utf8_ssid = g_strdup ("");
	gtk_entry_set_text (priv->ssid, utf8_ssid);
	g_signal_connect_swapped (priv->ssid, "changed", G_CALLBACK (ce_page_changed), self);
	g_free (utf8_ssid);

	/* Default to Infrastructure */
	gtk_combo_box_set_active (priv->mode, 0);
	if (!g_strcmp0 (mode, "ap"))
		gtk_combo_box_set_active (priv->mode, 1);
	if (!g_strcmp0 (mode, "adhoc"))
		gtk_combo_box_set_active (priv->mode, 2);
	mode_combo_changed_cb (priv->mode, self);
	g_signal_connect (priv->mode, "changed", G_CALLBACK (mode_combo_changed_cb), self);

	g_signal_connect (priv->channel, "output",
	                  G_CALLBACK (channel_spin_output_cb),
	                  self);
	g_signal_connect (priv->channel, "input",
	                  G_CALLBACK (channel_spin_input_cb),
	                  self);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->channel), FALSE);
	if (band) {
		if (!strcmp (band, "a")) {
			band_idx = 1;
			gtk_widget_set_sensitive (GTK_WIDGET (priv->channel), TRUE);
		} else if (!strcmp (band, "bg")) {
			band_idx = 2;
			gtk_widget_set_sensitive (GTK_WIDGET (priv->channel), TRUE);
		}
	}

	gtk_combo_box_set_active (priv->band, band_idx);
	g_signal_connect (priv->band, "changed",
	                  G_CALLBACK (band_value_changed_cb),
	                  self);

	/* Update the channel _after_ the band has been set so that it gets
	 * the right values */
	priv->last_channel = nm_setting_wireless_get_channel (setting);
	gtk_spin_button_set_value (priv->channel, (gdouble) priv->last_channel);
	g_signal_connect_swapped (priv->channel, "value-changed", G_CALLBACK (ce_page_changed), self);

	/* BSSID */
	bssid_array = g_ptr_array_new ();
	for (idx = 0; idx < nm_setting_wireless_get_num_seen_bssids (setting); idx++)
		g_ptr_array_add (bssid_array, g_strdup (nm_setting_wireless_get_seen_bssid (setting, idx)));
	g_ptr_array_add (bssid_array, NULL);
	bssid_list = (char **) g_ptr_array_free (bssid_array, FALSE);
	s_bssid = nm_setting_wireless_get_bssid (setting);
	ce_page_setup_mac_combo (CE_PAGE (self), GTK_COMBO_BOX (priv->bssid),
	                         s_bssid, bssid_list);
	g_strfreev (bssid_list);
	g_signal_connect_swapped (priv->bssid, "changed", G_CALLBACK (ce_page_changed), self);

	/* Device MAC address */
        s_ifname = nm_connection_get_interface_name (CE_PAGE (self)->connection);
	s_mac = nm_setting_wireless_get_mac_address (setting);
	ce_page_setup_device_combo (CE_PAGE (self), GTK_COMBO_BOX (priv->device_combo),
	                            NM_TYPE_DEVICE_WIFI, s_ifname,
	                            s_mac, NM_DEVICE_WIFI_PERMANENT_HW_ADDRESS, TRUE);
	g_signal_connect_swapped (priv->device_combo, "changed", G_CALLBACK (ce_page_changed), self);

	/* Cloned MAC address */
	s_mac = nm_setting_wireless_get_cloned_mac_address (setting);
	if (s_mac)
		gtk_entry_set_text (priv->cloned_mac, s_mac);
	g_signal_connect_swapped (priv->cloned_mac, "changed", G_CALLBACK (ce_page_changed), self);

	gtk_spin_button_set_value (priv->rate, (gdouble) nm_setting_wireless_get_rate (setting));
	gtk_spin_button_set_value (priv->tx_power, (gdouble) nm_setting_wireless_get_tx_power (setting));
	gtk_spin_button_set_value (priv->mtu, (gdouble) nm_setting_wireless_get_mtu (setting));
}

static void
finish_setup (CEPageWifi *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPage *parent = CE_PAGE (self);
	GtkWidget *widget;

	if (error)
		return;

	populate_ui (self);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wifi_tx_power_label"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wifi_tx_power_hbox"));
	gtk_widget_hide (widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wifi_rate_label"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wifi_rate_hbox"));
	gtk_widget_hide (widget);
}

CEPage *
ce_page_wifi_new (NMConnectionEditor *editor,
                  NMConnection *connection,
                  GtkWindow *parent_window,
                  NMClient *client,
                  const char **out_secrets_setting_name,
                  GError **error)
{
	CEPageWifi *self;
	CEPageWifiPrivate *priv;

	g_return_val_if_fail (NM_IS_CONNECTION (connection), NULL);

	self = CE_PAGE_WIFI (ce_page_new (CE_TYPE_PAGE_WIFI,
	                                  editor,
	                                  connection,
	                                  parent_window,
	                                  client,
	                                  UIDIR "/ce-page-wifi.ui",
	                                  "WifiPage",
	                                  _("Wi-Fi")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load Wi-Fi user interface."));
		return NULL;
	}

	wifi_private_init (self);
	priv = CE_PAGE_WIFI_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_wireless (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_WIRELESS (nm_setting_wireless_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

GBytes *
ce_page_wifi_get_ssid (CEPageWifi *self)
{
	CEPageWifiPrivate *priv;
	const char *txt_ssid;
	GBytes *ssid;

	g_return_val_if_fail (CE_IS_PAGE_WIFI (self), NULL);

	priv = CE_PAGE_WIFI_GET_PRIVATE (self);
	txt_ssid = gtk_entry_get_text (priv->ssid);
	if (!txt_ssid || !strlen (txt_ssid))
		return NULL;

	ssid = g_bytes_new (txt_ssid, strlen (txt_ssid));

	return ssid;
}

static void
ui_to_setting (CEPageWifi *self)
{
	CEPageWifiPrivate *priv = CE_PAGE_WIFI_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	GBytes *ssid;
	const char *bssid = NULL;
	char *ifname = NULL;
	char *device_mac = NULL;
	const char *cloned_mac;
	const char *mode;
	const char *band;
	GtkWidget *entry;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	g_return_if_fail (s_con != NULL);

	ssid = ce_page_wifi_get_ssid (self);

	switch (gtk_combo_box_get_active (priv->mode)) {
	case 1:
		mode = "ap";
		break;
	case 2:
		mode = "adhoc";
		break;
	default:
		mode = "infrastructure";
		break;
	}

	switch (gtk_combo_box_get_active (priv->band)) {
	case 1:
		band = "a";
		break;
	case 2:
		band = "bg";
		break;
	case 0:
	default:
		band = NULL;
		break;
	}

	entry = gtk_bin_get_child (GTK_BIN (priv->bssid));
	/* BSSID is only valid for infrastructure */
	if (entry && mode && strcmp (mode, "infrastructure") == 0)
		bssid = gtk_entry_get_text (GTK_ENTRY (entry));
	entry = gtk_bin_get_child (GTK_BIN (priv->device_combo));
	if (entry)
		ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, TRUE, &ifname, &device_mac, NULL, NULL);
	cloned_mac = gtk_entry_get_text (priv->cloned_mac);

	g_object_set (s_con,
	              NM_SETTING_CONNECTION_INTERFACE_NAME, ifname,
	              NULL);
	g_object_set (priv->setting,
	              NM_SETTING_WIRELESS_SSID, ssid,
	              NM_SETTING_WIRELESS_BSSID, bssid && *bssid ? bssid : NULL,
	              NM_SETTING_WIRELESS_MAC_ADDRESS, device_mac,
	              NM_SETTING_WIRELESS_CLONED_MAC_ADDRESS, cloned_mac && *cloned_mac ? cloned_mac : NULL,
	              NM_SETTING_WIRELESS_MODE, mode,
	              NM_SETTING_WIRELESS_BAND, band,
	              NM_SETTING_WIRELESS_CHANNEL, gtk_spin_button_get_value_as_int (priv->channel),
	              NM_SETTING_WIRELESS_RATE, gtk_spin_button_get_value_as_int (priv->rate),
	              NM_SETTING_WIRELESS_TX_POWER, gtk_spin_button_get_value_as_int (priv->tx_power),
	              NM_SETTING_WIRELESS_MTU, gtk_spin_button_get_value_as_int (priv->mtu),
	              NULL);

	g_bytes_unref (ssid);
	g_free (ifname);
	g_free (device_mac);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageWifi *self = CE_PAGE_WIFI (page);
	CEPageWifiPrivate *priv = CE_PAGE_WIFI_GET_PRIVATE (self);
	gboolean success;
	GtkWidget *entry;

	entry = gtk_bin_get_child (GTK_BIN (priv->bssid));
	if (entry) {
		if (!ce_page_mac_entry_valid (GTK_ENTRY (entry), ARPHRD_ETHER, _("bssid"), error))
			return FALSE;
	}

	entry = gtk_bin_get_child (GTK_BIN (priv->device_combo));
	if (entry) {
		if (!ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, TRUE, NULL, NULL, _("Wi-Fi device"), error))
			return FALSE;
	}

	if (!ce_page_mac_entry_valid (priv->cloned_mac, ARPHRD_ETHER, _("cloned MAC"), error))
		return FALSE;

	ui_to_setting (self);

	success = nm_setting_verify (NM_SETTING (priv->setting), NULL, error);

	return success;
}

static void
ce_page_wifi_init (CEPageWifi *self)
{
}

static void
ce_page_wifi_class_init (CEPageWifiClass *wifi_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (wifi_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (wifi_class);

	g_type_class_add_private (object_class, sizeof (CEPageWifiPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
}


void
wifi_connection_new (GtkWindow *parent,
                     const char *detail,
                     NMClient *client,
                     PageNewConnectionResultFunc result_func,
                     gpointer user_data)
{
	NMConnection *connection;
	NMSetting *s_wifi;

	connection = ce_page_new_connection (_("Wi-Fi connection %d"),
	                                     NM_SETTING_WIRELESS_SETTING_NAME,
	                                     TRUE,
	                                     client,
	                                     user_data);
	s_wifi = nm_setting_wireless_new ();
	g_object_set (s_wifi, NM_SETTING_WIRELESS_MODE, "infrastructure", NULL);
	nm_connection_add_setting (connection, s_wifi);

	(*result_func) (connection, FALSE, NULL, user_data);
}


