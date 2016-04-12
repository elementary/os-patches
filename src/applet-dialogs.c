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
#include <sys/socket.h>
#include <arpa/inet.h>

#include <NetworkManager.h>

#include "applet-dialogs.h"
#include "utils.h"


static void
info_dialog_show_error (const char *err)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new_with_markup (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
			"<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s", _("Error displaying connection information:"), err);
	gtk_window_present (GTK_WINDOW (dialog));
	g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
}

static char *
ip4_address_as_string (guint32 ip)
{
	char *ip_string;
	struct in_addr tmp_addr;

	tmp_addr.s_addr = ip;
	ip_string = g_malloc0 (INET_ADDRSTRLEN + 1);
	if (!inet_ntop (AF_INET, &tmp_addr, ip_string, INET_ADDRSTRLEN))
		strcpy (ip_string, "(none)");
	return ip_string;
}

static char *
get_eap_label (NMSettingWirelessSecurity *sec,
			   NMSetting8021x *s_8021x)
{
	GString *str = NULL;
	char *phase2_str = NULL;

	if (sec) {
		const char *key_mgmt = nm_setting_wireless_security_get_key_mgmt (sec);
		const char *auth_alg = nm_setting_wireless_security_get_auth_alg (sec);

		if (!strcmp (key_mgmt, "ieee8021x")) {
			if (auth_alg && !strcmp (auth_alg, "leap"))
				str = g_string_new (_("LEAP"));
			else
				str = g_string_new (_("Dynamic WEP"));
		} else if (!strcmp (key_mgmt, "wpa-eap"))
			str = g_string_new (_("WPA/WPA2"));
		else
			return NULL;
	} else if (s_8021x)
		str = g_string_new ("802.1x");

	if (!s_8021x)
		goto out;

	if (nm_setting_802_1x_get_num_eap_methods (s_8021x)) {
		char *eap_str = g_ascii_strup (nm_setting_802_1x_get_eap_method (s_8021x, 0), -1);
		g_string_append_printf (str, ", EAP-%s", eap_str);
		g_free (eap_str);
	}

	if (nm_setting_802_1x_get_phase2_auth (s_8021x))
		phase2_str = g_ascii_strup (nm_setting_802_1x_get_phase2_auth (s_8021x), -1);
	else if (nm_setting_802_1x_get_phase2_autheap (s_8021x))
		phase2_str = g_ascii_strup (nm_setting_802_1x_get_phase2_autheap (s_8021x), -1);

	if (phase2_str) {
		g_string_append (str, ", ");
		g_string_append (str, phase2_str);
		g_free (phase2_str);
	}
	
out:
	return g_string_free (str, FALSE);
}

static NMConnection *
get_connection_for_active_path (NMApplet *applet, const char *active_path)
{
	NMActiveConnection *active = NULL;
	const GPtrArray *connections;
	int i;

	if (active_path == NULL)
		return NULL;

	connections = nm_client_get_active_connections (applet->nm_client);
	for (i = 0; connections && (i < connections->len); i++) {
		NMActiveConnection *candidate = g_ptr_array_index (connections, i);
		const char *ac_path = nm_object_get_path (NM_OBJECT (candidate));

		if (g_strcmp0 (ac_path, active_path) == 0) {
			active = candidate;
			break;
		}
	}

	return active ? (NMConnection *) nm_active_connection_get_connection (active) : NULL;
}

static GtkWidget *
create_info_label (const char *text, gboolean selectable)
{
	GtkWidget *label;

	label = gtk_label_new (text ? text : "");
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_label_set_selectable (GTK_LABEL (label), selectable);
	gtk_widget_set_hexpand (label, selectable);
	return label;
}

static GtkWidget *
create_more_addresses_widget (const GPtrArray *addresses)
{
	GtkWidget *expander, *label, *text_view, *child;
	GtkTextBuffer *buffer;
	GtkWidget *scrolled_window;
	int i;

	/* Create the expander */
	expander = gtk_expander_new (_("More addresses"));
	gtk_widget_set_halign (expander, GTK_ALIGN_START);
	label = gtk_expander_get_label_widget (GTK_EXPANDER (expander));
	gtk_widget_set_margin_top (label, 2);

	/* Create the text view widget and add additional addresses to it */
	text_view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text_view), 20);
	gtk_expander_set_spacing (GTK_EXPANDER (expander), 4);
	child = text_view;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
	for (i = 1; addresses && (i < addresses->len); i++) {
		NMIPAddress *addr = (NMIPAddress *) g_ptr_array_index (addresses, i);
		char *addr_text = g_strdup_printf ("%s / %d",
		                                  nm_ip_address_get_address (addr),
		                                  nm_ip_address_get_prefix (addr));
		if (i != 1)
			gtk_text_buffer_insert_at_cursor (buffer, "\n", -1);
		gtk_text_buffer_insert_at_cursor (buffer, addr_text, -1);
		g_free (addr_text);
	}

	if (addresses->len > 5) {
		scrolled_window = gtk_scrolled_window_new (NULL, NULL);
		gtk_widget_set_size_request (scrolled_window, -1, 80);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
		                                GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);
		child = scrolled_window;
	}

	gtk_container_add (GTK_CONTAINER (expander), child);

	return expander;
}

static GtkWidget *
create_info_group_label (const char *text, gboolean selectable)
{
	GtkWidget *label;
	char *markup;

	label = create_info_label (NULL, selectable);
	markup = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>", text);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);

	return label;
}

static GtkWidget *
create_info_label_security (NMConnection *connection)
{
	NMSettingConnection *s_con;
	char *label = NULL;
	GtkWidget *w = NULL;
	const char *connection_type;

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);

	connection_type = nm_setting_connection_get_connection_type (s_con);
	if (!strcmp (connection_type, NM_SETTING_WIRELESS_SETTING_NAME)) {
		NMSettingWirelessSecurity *s_wireless_sec;
		NMSetting8021x *s_8021x;

		s_wireless_sec = nm_connection_get_setting_wireless_security (connection);
		s_8021x = nm_connection_get_setting_802_1x (connection);

		if (s_wireless_sec) {
			const char *key_mgmt = nm_setting_wireless_security_get_key_mgmt (s_wireless_sec);

			if (!strcmp (key_mgmt, "none"))
				label = g_strdup (_("WEP"));
			else if (!strcmp (key_mgmt, "wpa-none"))
				label = g_strdup (_("WPA/WPA2"));
			else if (!strcmp (key_mgmt, "wpa-psk"))
				label = g_strdup (_("WPA/WPA2"));
			else
				label = get_eap_label (s_wireless_sec, s_8021x);
		} else {
			label = g_strdup (C_("Wi-Fi/Ethernet security", "None"));
		}
	} else if (!strcmp (connection_type, NM_SETTING_WIRED_SETTING_NAME)) {
		NMSetting8021x *s_8021x;

		s_8021x = nm_connection_get_setting_802_1x (connection);
		if (s_8021x)
			label = get_eap_label (NULL, s_8021x);
		else
			label = g_strdup (C_("Wi-Fi/Ethernet security", "None"));
	}

	if (label)
		w = create_info_label (label, TRUE);
	g_free (label);

	return w;
}

static GtkWidget *
create_info_notebook_label (NMConnection *connection, gboolean is_default)
{
	GtkWidget *label;
	char *str;

	if (is_default) {
		str = g_strdup_printf (_("%s (default)"), nm_connection_get_id (connection));
		label = gtk_label_new (str);
		g_free (str);
	} else
		label = gtk_label_new (nm_connection_get_id (connection));

	return label;
}

typedef struct {
	NMDevice *device;
	GtkWidget *label;
	guint32 id;
} LabelInfo;

static void
device_destroyed (gpointer data, GObject *device_ptr)
{
	LabelInfo *info = data;

	/* Device is destroyed, notify handler won't fire
	 * anymore anyway.  Let the label destroy handler
	 * know it doesn't have to disconnect the callback.
	 */
	info->device = NULL;
	info->id = 0;
}

static void
label_destroyed (gpointer data, GObject *label_ptr)
{
	LabelInfo *info = data;

	/* Remove the notify handler from the device */
	if (info->device) {
		if (info->id)
			g_signal_handler_disconnect (info->device, info->id);
		/* destroy our info data */
		g_object_weak_unref (G_OBJECT (info->device), device_destroyed, info);
		memset (info, 0, sizeof (LabelInfo));
		g_free (info);
	}
}

static void
label_info_new (NMDevice *device,
                GtkWidget *label,
                const char *notify_prop,
                GCallback callback)
{
	LabelInfo *info;

	info = g_malloc0 (sizeof (LabelInfo));
	info->device = device;
	info->label = label;
	info->id = g_signal_connect (device, notify_prop, callback, label);
	g_object_weak_ref (G_OBJECT (label), label_destroyed, info);
	g_object_weak_ref (G_OBJECT (device), device_destroyed, info);
}

static void
bitrate_changed_cb (GObject *device, GParamSpec *pspec, gpointer user_data)
{
	GtkWidget *speed_label = GTK_WIDGET (user_data);
	guint32 bitrate = 0;
	char *str = NULL;

	bitrate = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (device)) / 1000;
	if (bitrate)
		str = g_strdup_printf (_("%u Mb/s"), bitrate);

	gtk_label_set_text (GTK_LABEL (speed_label), str ? str : C_("Speed", "Unknown"));
	g_free (str);
}


static void
display_ip4_info (NMIPAddress *def_addr, const GPtrArray *addresses, GtkGrid *grid, int *row)
{
	GtkWidget *desc_widget, *data_widget = NULL;
	AtkObject *desc_object, *data_object = NULL;
	guint32 hostmask, network, bcast, netmask;
	const char *addr;
	char *str;

	/* Address */
	addr = def_addr ? nm_ip_address_get_address (def_addr) : C_("Address", "Unknown");
	desc_widget = create_info_label (_("IP Address:"), FALSE);
	desc_object = gtk_widget_get_accessible (desc_widget);
	data_widget = create_info_label (addr, TRUE);
	data_object = gtk_widget_get_accessible (data_widget);
	atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

	gtk_grid_attach (grid, desc_widget, 0, *row, 1, 1);
	gtk_grid_attach (grid, data_widget, 1, *row, 1, 1);
	(*row)++;

	/* Broadcast */
	if (def_addr) {
		guint32 addr_bin;

		nm_ip_address_get_address_binary (def_addr, &addr_bin);
		netmask = nm_utils_ip4_prefix_to_netmask (nm_ip_address_get_prefix (def_addr));
		network = addr_bin & netmask;
		hostmask = ~netmask;
		bcast = network | hostmask;
	}

	str = def_addr ? ip4_address_as_string (bcast) : g_strdup (C_("Address", "Unknown"));
	desc_widget = create_info_label (_("Broadcast Address:"), FALSE);
	desc_object = gtk_widget_get_accessible (desc_widget);
	data_widget = create_info_label (str, TRUE);
	data_object = gtk_widget_get_accessible (data_widget);
	atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

	gtk_grid_attach (grid, desc_widget, 0, *row, 1, 1);
	gtk_grid_attach (grid, data_widget, 1, *row, 1, 1);
	g_free (str);
	(*row)++;

	/* Prefix */
	str = def_addr ? ip4_address_as_string (netmask) : g_strdup (C_("Subnet Mask", "Unknown"));
	desc_widget = create_info_label (_("Subnet Mask:"), FALSE);
	desc_object = gtk_widget_get_accessible (desc_widget);
	data_widget = create_info_label (str, TRUE);
	data_object = gtk_widget_get_accessible (data_widget);
	atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

	gtk_grid_attach (grid, desc_widget, 0, *row, 1, 1);
	gtk_grid_attach (grid, data_widget, 1, *row, 1, 1);
	g_free (str);
	(*row)++;

	/* More Addresses */
	if (addresses && addresses->len > 1) {
		data_widget = create_more_addresses_widget (addresses);
		gtk_grid_attach (grid, data_widget, 0, *row, 2, 1);
		(*row)++;
	}
}

static void
display_ip6_info (NMIPAddress *def6_addr,
                  const GPtrArray *addresses,
                  const char *method,
                  GtkGrid *grid,
                  int *row)
{
	GtkWidget *desc_widget, *data_widget = NULL;
	AtkObject *desc_object, *data_object = NULL;
	char *str;

	if (!def6_addr)
		return;

	/* Address */
	str = g_strdup_printf ("%s/%d",
	                       nm_ip_address_get_address (def6_addr),
	                       nm_ip_address_get_prefix (def6_addr));

	desc_widget = create_info_label (_("IP Address:"), FALSE);
	desc_object = gtk_widget_get_accessible (desc_widget);
	data_widget = create_info_label (str, TRUE);
	data_object = gtk_widget_get_accessible (data_widget);
	atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

	gtk_grid_attach (grid, desc_widget, 0, *row, 1, 1);
	gtk_grid_attach (grid, data_widget, 1, *row, 1, 1);
	g_free (str);
	(*row)++;

	/* More Addresses */
	if (addresses && addresses->len > 1) {
		data_widget = create_more_addresses_widget (addresses);
		gtk_grid_attach (grid, data_widget, 0, *row, 2, 1);
		(*row)++;
	}
}

static void
display_dns_info (const char * const *dns, GtkGrid *grid, int *row)
{
	GtkWidget *desc_widget, *data_widget = NULL;
	AtkObject *desc_object, *data_object = NULL;
	char *label[] = { "Primary DNS:", "Secondary DNS:", "Ternary DNS:" };
	int i;

	for (i = 0; dns && dns[i] && i < 3; i++) {
		desc_widget = create_info_label (_(label[i]), FALSE);
		desc_object = gtk_widget_get_accessible (desc_widget);
		data_widget = create_info_label (dns[i], TRUE);
		data_object = gtk_widget_get_accessible (data_widget);
		atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

		gtk_grid_attach (grid, desc_widget, 0, *row, 1, 1);
		gtk_grid_attach (grid, data_widget, 1, *row, 1, 1);
		(*row)++;
	}
}

static void
info_dialog_add_page (GtkNotebook *notebook,
                      NMConnection *connection,
                      gboolean is_default,
                      NMDevice *device)
{
	GtkGrid *grid;
	guint32 speed = 0;
	char *str;
	const char *iface, *method = NULL;
	NMIPConfig *ip4_config;
	NMIPConfig *ip6_config;
	const char * const *dns;
	const char * const *dns6;
	NMIPAddress *def_addr = NULL;
	NMIPAddress *def6_addr = NULL;
	const char *gateway;
	NMSettingIPConfig *s_ip6;
	int row = 0;
	GtkWidget* speed_label, *sec_label, *desc_widget, *data_widget = NULL;
	GPtrArray *addresses;
	gboolean show_security = FALSE;
	AtkObject *desc_object, *data_object = NULL;

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (grid, 12);
	gtk_grid_set_row_spacing (grid, 6);
	gtk_container_set_border_width (GTK_CONTAINER (grid), 12);

	/* Interface */
	iface = nm_device_get_iface (device);
	if (NM_IS_DEVICE_ETHERNET (device)) {
		str = g_strdup_printf (_("Ethernet (%s)"), iface);
		show_security = TRUE;
	} else if (NM_IS_DEVICE_WIFI (device)) {
		str = g_strdup_printf (_("802.11 WiFi (%s)"), iface);
		show_security = TRUE;
	} else if (NM_IS_DEVICE_MODEM (device)) {
		NMDeviceModemCapabilities caps;

		caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device));
		if (caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS)
			str = g_strdup_printf (_("GSM (%s)"), iface);
		else if (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO)
			str = g_strdup_printf (_("CDMA (%s)"), iface);
		else
			str = g_strdup_printf (_("Mobile Broadband (%s)"), iface);
	} else
		str = g_strdup (iface);


	/*--- General ---*/
	gtk_grid_attach (grid, create_info_group_label (_("General"), FALSE),
	                 0, row, 1, 1);
	row++;

	desc_widget = create_info_label (_("Interface:"), FALSE);
	desc_object = gtk_widget_get_accessible (desc_widget);
	data_widget = create_info_label (str, TRUE);
	data_object = gtk_widget_get_accessible (data_widget);
	atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

	gtk_grid_attach (grid, desc_widget,
	                 0, row, 1, 1);
	gtk_grid_attach (grid, data_widget,
	                 1, row, 1, 1);
	g_free (str);
	row++;

	/* Hardware address */
	str = NULL;
	if (NM_IS_DEVICE_ETHERNET (device))
		str = g_strdup (nm_device_ethernet_get_hw_address (NM_DEVICE_ETHERNET (device)));
	else if (NM_IS_DEVICE_WIFI (device))
		str = g_strdup (nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (device)));

	desc_widget = create_info_label (_("Hardware Address:"), FALSE);
	desc_object = gtk_widget_get_accessible (desc_widget);
	data_widget = create_info_label (str, TRUE);
	data_object = gtk_widget_get_accessible (data_widget);
	atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

	gtk_grid_attach (grid, desc_widget,
	                 0, row, 1, 1);
	gtk_grid_attach (grid, data_widget,
	                 1, row, 1, 1);
	g_free (str);
	row++;

	/* Driver */
	desc_widget = create_info_label (_("Driver:"), FALSE);
	desc_object = gtk_widget_get_accessible (desc_widget);
	data_widget = create_info_label (nm_device_get_driver (device), TRUE);
	data_object = gtk_widget_get_accessible (data_widget);
	atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

	gtk_grid_attach (grid, desc_widget,
	                 0, row, 1, 1);
	gtk_grid_attach (grid, data_widget,
	                 1, row, 1, 1);
	row++;

	speed_label = create_info_label ("", TRUE);

	/* Speed */
	str = NULL;
	if (NM_IS_DEVICE_ETHERNET (device)) {
		/* Ethernet speed in Mb/s */
		speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (device));
	} else if (NM_IS_DEVICE_WIFI (device)) {
		/* Wi-Fi speed in Kb/s */
		speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (device)) / 1000;

		label_info_new (device,
		                speed_label,
		                "notify::" NM_DEVICE_WIFI_BITRATE,
		                G_CALLBACK (bitrate_changed_cb));
	}

	if (speed)
		str = g_strdup_printf (_("%u Mb/s"), speed);

	gtk_label_set_text (GTK_LABEL(speed_label), str ? str : C_("Speed", "Unknown"));
	g_free (str);

	desc_widget = create_info_label (_("Speed:"), FALSE);
	desc_object = gtk_widget_get_accessible (desc_widget);
	data_object = gtk_widget_get_accessible (speed_label);
	atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

	gtk_grid_attach (grid, desc_widget,
	                 0, row, 1, 1);
	gtk_grid_attach (grid, speed_label,
	                 1, row, 1, 1);
	row++;

	/* Security */
	if (show_security) {
		sec_label = create_info_label_security (connection);
		if (sec_label) {
			desc_widget = create_info_label (_("Security:"), FALSE);
			desc_object = gtk_widget_get_accessible (desc_widget);
			data_object = gtk_widget_get_accessible (sec_label);
			atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

			gtk_grid_attach (grid, desc_widget,
			                 0, row, 1, 1);
			gtk_grid_attach (grid, sec_label,
			                 1, row, 1, 1);
			row++;
		}
	}

	/* Empty line */
	gtk_grid_attach (grid, gtk_label_new (""), 0, row, 2, 1);
	row++;

	/*--- IPv4 ---*/
	gtk_grid_attach (grid, create_info_group_label (_("IPv4"), FALSE),
	                 0, row, 1, 1);
	row++;

	ip4_config = nm_device_get_ip4_config (device);
	if (ip4_config) {
		addresses = nm_ip_config_get_addresses (ip4_config);
		gateway = nm_ip_config_get_gateway (ip4_config);
	} else {
		addresses = NULL;
		gateway = NULL;
	}

	if (addresses && addresses->len > 0)
		def_addr = (NMIPAddress *) g_ptr_array_index (addresses, 0);

	display_ip4_info (def_addr, addresses, grid, &row);

	/* Gateway */
	if (gateway && *gateway) {
		desc_widget = create_info_label (_("Default Route:"), FALSE);
		desc_object = gtk_widget_get_accessible (desc_widget);
		data_widget = create_info_label (gateway, TRUE);
		data_object = gtk_widget_get_accessible (data_widget);
		atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

		gtk_grid_attach (grid, desc_widget, 0, row, 1, 1);
		gtk_grid_attach (grid, data_widget, 1, row, 1, 1);
		row++;
	}

	/* DNS */
	dns = def_addr ? nm_ip_config_get_nameservers (ip4_config) : NULL;
	display_dns_info (dns, grid, &row);

	/* Empty line */
	gtk_grid_attach (grid, gtk_label_new (""), 0, row, 2, 1);
	row++;

	/*--- IPv6 ---*/
	gtk_grid_attach (grid, create_info_group_label (_("IPv6"), FALSE),
	                 0, row, 1, 1);
	row++;

	s_ip6 = nm_connection_get_setting_ip6_config (connection);
	if (s_ip6)
		 method = nm_setting_ip_config_get_method (s_ip6);

	if (!method || !strcmp (method, NM_SETTING_IP6_CONFIG_METHOD_IGNORE)) {
		gtk_grid_attach (grid, create_info_label (_("Ignored"), FALSE),
		                 0, row, 1, 1);
		row++;
	}

	addresses = NULL;
	ip6_config = nm_device_get_ip6_config (device);
	if (ip6_config) {
		addresses = nm_ip_config_get_addresses (ip6_config);
		gateway = nm_ip_config_get_gateway (ip6_config);
	} else {
		addresses = NULL;
		gateway = NULL;
	}

	if (addresses && addresses->len > 0)
		def6_addr = (NMIPAddress *) g_ptr_array_index (addresses, 0);

	display_ip6_info (def6_addr, addresses, method, grid, &row);

	/* Gateway */
	if (gateway && *gateway) {
		desc_widget = create_info_label (_("Default Route:"), FALSE);
		desc_object = gtk_widget_get_accessible (desc_widget);
		data_widget = create_info_label (gateway, TRUE);
		data_object = gtk_widget_get_accessible (data_widget);
		atk_object_add_relationship (desc_object, ATK_RELATION_LABEL_FOR, data_object);

		gtk_grid_attach (grid, desc_widget, 0, row, 1, 1);
		gtk_grid_attach (grid, data_widget, 1, row, 1, 1);
		row++;
	}

	/* DNS */
	dns6 = def6_addr ? nm_ip_config_get_nameservers (ip6_config) : NULL;
	display_dns_info (dns6, grid, &row);

	desc_widget = NULL;
	desc_object = NULL;
	data_widget = NULL;
	data_object = NULL;

	gtk_notebook_append_page (notebook, GTK_WIDGET (grid),
	                          create_info_notebook_label (connection, is_default));

	gtk_widget_show_all (GTK_WIDGET (grid));
}

static char *
get_vpn_connection_type (NMConnection *connection)
{
	const char *type, *p;

	/* The service type is in format of "org.freedesktop.NetworkManager.vpnc".
	 * Extract end part after last dot, e.g. "vpnc" */
	type = nm_setting_vpn_get_service_type (nm_connection_get_setting_vpn (connection));
	p = strrchr (type, '.');
	return g_strdup (p ? p + 1 : type);
}

/* VPN parameters can be found at:
 * http://git.gnome.org/browse/network-manager-openvpn/tree/src/nm-openvpn-service.h
 * http://git.gnome.org/browse/network-manager-vpnc/tree/src/nm-vpnc-service.h
 * http://git.gnome.org/browse/network-manager-pptp/tree/src/nm-pptp-service.h
 * http://git.gnome.org/browse/network-manager-openconnect/tree/src/nm-openconnect-service.h
 * http://git.gnome.org/browse/network-manager-openswan/tree/src/nm-openswan-service.h
 * See also 'properties' directory in these plugins.
 */
static const gchar *
find_vpn_gateway_key (const char *vpn_type)
{
	if (g_strcmp0 (vpn_type, "openvpn") == 0)     return "remote";
	if (g_strcmp0 (vpn_type, "vpnc") == 0)        return "IPSec gateway";
	if (g_strcmp0 (vpn_type, "pptp") == 0)        return "gateway";
	if (g_strcmp0 (vpn_type, "openconnect") == 0) return "gateway";
	if (g_strcmp0 (vpn_type, "openswan") == 0)    return "right";
	return "";
}

static const gchar *
find_vpn_username_key (const char *vpn_type)
{
	if (g_strcmp0 (vpn_type, "openvpn") == 0)     return "username";
	if (g_strcmp0 (vpn_type, "vpnc") == 0)        return "Xauth username";
	if (g_strcmp0 (vpn_type, "pptp") == 0)        return "user";
	if (g_strcmp0 (vpn_type, "openconnect") == 0) return "username";
	if (g_strcmp0 (vpn_type, "openswan") == 0)    return "leftxauthusername";
	return "";
}

enum VpnDataItem {
	VPN_DATA_ITEM_GATEWAY,
	VPN_DATA_ITEM_USERNAME
};

static const gchar *
get_vpn_data_item (NMConnection *connection, enum VpnDataItem vpn_data_item)
{
	const char *key;
	char *type = get_vpn_connection_type (connection);

	switch (vpn_data_item) {
	case VPN_DATA_ITEM_GATEWAY:
		key = find_vpn_gateway_key (type);
		break;
	case VPN_DATA_ITEM_USERNAME:
		key = find_vpn_username_key (type);
		break;
	default:
		key = "";
		break;
	}
	g_free (type);

	return nm_setting_vpn_get_data_item (nm_connection_get_setting_vpn (connection), key);
}

static void
info_dialog_add_page_for_vpn (GtkNotebook *notebook,
                              NMConnection *connection,
                              NMActiveConnection *active,
                              NMConnection *parent_con)
{
	GtkGrid *grid;
	char *str;
	int row = 0;
	NMIPConfig *ip4_config;
	NMIPConfig *ip6_config;
	const char * const *dns;
	const char * const *dns6;
	NMIPAddress *def_addr = NULL;
	NMIPAddress *def6_addr = NULL;
	GPtrArray *addresses;
	NMSettingIPConfig *s_ip6;
	const char *method = NULL;
	gboolean is_default = nm_active_connection_get_default (active);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (grid, 12);
	gtk_grid_set_row_spacing (grid, 6);
	gtk_container_set_border_width (GTK_CONTAINER (grid), 12);

	/*--- General ---*/
	gtk_grid_attach (grid, create_info_group_label (_("General"), FALSE),
	                 0, row, 1, 1);
	row++;

	str = get_vpn_connection_type (connection);
	gtk_grid_attach (grid, create_info_label (_("VPN Type:"), FALSE),
	                 0, row, 1, 1);
	gtk_grid_attach (grid, create_info_label (str, TRUE),
	                 1, row, 1, 1);
	g_free (str);
	row++;

	gtk_grid_attach (grid, create_info_label (_("VPN Gateway:"), FALSE),
	                 0, row, 1, 1);
	gtk_grid_attach (grid, create_info_label (get_vpn_data_item (connection, VPN_DATA_ITEM_GATEWAY), TRUE),
	                 1, row, 1, 1);
	row++;

	gtk_grid_attach (grid, create_info_label (_("VPN Username:"), FALSE),
	                 0, row, 1, 1);
	gtk_grid_attach (grid, create_info_label (get_vpn_data_item (connection, VPN_DATA_ITEM_USERNAME), TRUE),
	                 1, row, 1, 1);
	row++;

	gtk_grid_attach (grid, create_info_label (_("VPN Banner:"), FALSE),
	                 0, row, 1, 1);
	gtk_grid_attach (grid, create_info_label (nm_vpn_connection_get_banner (NM_VPN_CONNECTION (active)), TRUE),
	                 1, row, 1, 1);
	row++;

	gtk_grid_attach (grid, create_info_label (_("Base Connection:"), FALSE),
	                 0, row, 1, 1);
	gtk_grid_attach (grid, create_info_label (parent_con ? nm_connection_get_id (parent_con) : _("Unknown"), TRUE),
	                 1, row, 1, 1);
	row++;

	/* Empty line */
	gtk_grid_attach (grid, gtk_label_new (""), 0, row, 2, 1);
	row++;

	/*--- IPv4 ---*/
	gtk_grid_attach (grid, create_info_group_label (_("IPv4"), FALSE),
	                 0, row, 1, 1);
	row++;

	ip4_config = nm_active_connection_get_ip4_config (active);
	addresses = nm_ip_config_get_addresses (ip4_config);
	if (addresses && addresses->len > 0)
		def_addr = (NMIPAddress *) g_ptr_array_index (addresses, 0);

	display_ip4_info (def_addr, addresses, grid, &row);

	/* DNS */
	dns = def_addr ? nm_ip_config_get_nameservers (ip4_config) : NULL;
	display_dns_info (dns, grid, &row);

	/* Empty line */
	gtk_grid_attach (grid, gtk_label_new (""), 0, row, 2, 1);
	row++;

	/*--- IPv6 ---*/
	ip6_config = nm_active_connection_get_ip6_config (active);
	if (ip6_config) {
		gtk_grid_attach (grid, create_info_group_label (_("IPv6"), FALSE),
		                 0, row, 1, 1);
		row++;

		s_ip6 = nm_connection_get_setting_ip6_config (connection);
		if (s_ip6)
			 method = nm_setting_ip_config_get_method (s_ip6);

		if (!method || !strcmp (method, NM_SETTING_IP6_CONFIG_METHOD_IGNORE)) {
			gtk_grid_attach (grid, create_info_label (_("Ignored"), FALSE),
			                 0, row, 1, 1);
			row++;
		}

		addresses = nm_ip_config_get_addresses (ip6_config);
		if (addresses && addresses->len > 0)
			def6_addr = (NMIPAddress *) g_ptr_array_index (addresses, 0);

		/* IPv6 Address */
		display_ip6_info (def6_addr, addresses, method, grid, &row);

		/* DNS */
		dns6 = def6_addr ? nm_ip_config_get_nameservers (ip6_config) : NULL;
		display_dns_info (dns6, grid, &row);
	}

	gtk_notebook_append_page (notebook, GTK_WIDGET (grid),
	                          create_info_notebook_label (connection, is_default));

	gtk_widget_show_all (GTK_WIDGET (grid));
}

static GtkWidget *
info_dialog_update (NMApplet *applet)
{
	GtkNotebook *notebook;
	const GPtrArray *connections;
	int i;
	int pages = 0;

	notebook = GTK_NOTEBOOK (GTK_WIDGET (gtk_builder_get_object (applet->info_dialog_ui, "info_notebook")));

	/* Remove old pages */
	for (i = gtk_notebook_get_n_pages (notebook); i > 0; i--)
		gtk_notebook_remove_page (notebook, -1);

	/* Add new pages */
	connections = nm_client_get_active_connections (applet->nm_client);
	for (i = 0; connections && (i < connections->len); i++) {
		NMActiveConnection *active_connection = g_ptr_array_index (connections, i);
		NMConnection *connection;
		const GPtrArray *devices;

		if (nm_active_connection_get_state (active_connection) != NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
			continue;

		connection = (NMConnection *) nm_active_connection_get_connection (active_connection);
		if (!connection) {
			g_warning ("%s: couldn't find the default active connection's NMConnection!", __func__);
			continue;
		}

		devices = nm_active_connection_get_devices (active_connection);
		if (NM_IS_VPN_CONNECTION (active_connection)) {
			const char *spec_object = nm_active_connection_get_specific_object_path (active_connection);
			NMConnection *parent_con = get_connection_for_active_path (applet, spec_object);

			info_dialog_add_page_for_vpn (notebook, connection, active_connection, parent_con);
		} else if (devices && devices->len > 0) {
				info_dialog_add_page (notebook,
				                      connection,
				                      nm_active_connection_get_default (active_connection),
				                      g_ptr_array_index (devices, 0));
		} else {
			g_warning ("Active connection %s had no devices and was not a VPN!",
			           nm_object_get_path (NM_OBJECT (active_connection)));
			continue;
		}

		pages++;
	}

	if (pages == 0) {
		/* Shouldn't really happen but ... */
		info_dialog_show_error (_("No valid active connections found!"));
		return NULL;
	}

	return GTK_WIDGET (gtk_builder_get_object (applet->info_dialog_ui, "info_dialog"));
}

void
applet_info_dialog_show (NMApplet *applet)
{
	GtkWidget *dialog;

	dialog = info_dialog_update (applet);
	if (!dialog)
		return;

	g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), dialog);
	g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_hide), dialog);
	gtk_widget_realize (dialog);
	gtk_window_present_with_time (GTK_WINDOW (dialog),
		gdk_x11_get_server_time (gtk_widget_get_window (dialog)));
}

void
applet_about_dialog_show (NMApplet *applet)
{
	gtk_show_about_dialog (NULL,
	                       "version", VERSION,
	                       "copyright", _("Copyright \xc2\xa9 2004-2014 Red Hat, Inc.\n"
	                                      "Copyright \xc2\xa9 2005-2008 Novell, Inc.\n"
	                                      "and many other community contributors and translators"),
	                       "comments", _("Notification area applet for managing your network devices and connections."),
	                       "website", "http://www.gnome.org/projects/NetworkManager/",
	                       "website-label", _("NetworkManager Website"),
	                       "logo-icon-name", GTK_STOCK_NETWORK,
	                       NULL);
}

GtkWidget *
applet_warning_dialog_show (const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, message, NULL);

	/* Bash focus-stealing prevention in the face */
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_default_icon_name (GTK_STOCK_DIALOG_ERROR);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Missing resources"));
	gtk_widget_realize (dialog);
	gtk_widget_show (dialog);
	gtk_window_present_with_time (GTK_WINDOW (dialog),
		gdk_x11_get_server_time (gtk_widget_get_window (dialog)));

	g_signal_connect_swapped (dialog, "response",
	                          G_CALLBACK (gtk_widget_destroy),
	                          dialog);
	return dialog;
}

GtkWidget *
applet_mobile_password_dialog_new (NMConnection *connection,
                                   GtkEntry **out_secret_entry)
{
	GtkDialog *dialog;
	GtkWidget *w;
	GtkBox *box = NULL, *vbox = NULL;
	NMSettingConnection *s_con;
	char *tmp;
	const char *id;

	dialog = GTK_DIALOG (gtk_dialog_new ());
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Mobile broadband network password"));

	gtk_dialog_add_button (dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
	w = gtk_dialog_add_button (dialog, GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_window_set_default (GTK_WINDOW (dialog), w);

	s_con = nm_connection_get_setting_connection (connection);
	id = nm_setting_connection_get_id (s_con);
	g_assert (id);
	tmp = g_strdup_printf (_("A password is required to connect to '%s'."), id);
	w = gtk_label_new (tmp);
	g_free (tmp);

	vbox = GTK_BOX (gtk_dialog_get_content_area (dialog));

	gtk_box_pack_start (vbox, w, TRUE, TRUE, 0);

	w = gtk_alignment_new (0.5, 0.5, 0, 1.0);
	gtk_box_pack_start (vbox, w, TRUE, TRUE, 0);

	box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));
	gtk_container_set_border_width (GTK_CONTAINER (box), 6);
	gtk_container_add (GTK_CONTAINER (w), GTK_WIDGET (box));

	gtk_box_pack_start (box, gtk_label_new (_("Password:")), FALSE, FALSE, 0);

	w = gtk_entry_new ();
	*out_secret_entry = GTK_ENTRY (w);
	gtk_entry_set_activates_default (GTK_ENTRY (w), TRUE);
	gtk_box_pack_start (box, w, FALSE, FALSE, 0);

	gtk_widget_show_all (GTK_WIDGET (vbox));
	return GTK_WIDGET (dialog);
}

/**********************************************************************/

static void
mpd_entry_changed (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *dialog = GTK_WIDGET (user_data);
	GtkBuilder *builder = g_object_get_data (G_OBJECT (dialog), "builder");
	GtkWidget *entry;
	guint32 minlen;
	gboolean valid = FALSE;
	const char *text, *text2 = NULL, *text3 = NULL;
	gboolean match23;

	g_return_if_fail (builder != NULL);

	entry = GTK_WIDGET (gtk_builder_get_object (builder, "code1_entry"));
	if (g_object_get_data (G_OBJECT (entry), "active")) {
		minlen = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (entry), "minlen"));
		text = gtk_entry_get_text (GTK_ENTRY (entry));
		if (text && (strlen (text) < minlen))
			goto done;
	}

	entry = GTK_WIDGET (gtk_builder_get_object (builder, "code2_entry"));
	if (g_object_get_data (G_OBJECT (entry), "active")) {
		minlen = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (entry), "minlen"));
		text2 = gtk_entry_get_text (GTK_ENTRY (entry));
		if (text2 && (strlen (text2) < minlen))
			goto done;
	}

	entry = GTK_WIDGET (gtk_builder_get_object (builder, "code3_entry"));
	if (g_object_get_data (G_OBJECT (entry), "active")) {
		minlen = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (entry), "minlen"));
		text3 = gtk_entry_get_text (GTK_ENTRY (entry));
		if (text3 && (strlen (text3) < minlen))
			goto done;
	}

	/* Validate 2 & 3 if they are supposed to be the same */
	match23 = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (dialog), "match23"));
	if (match23) {
		if (!text2 || !text3 || strcmp (text2, text3))
			goto done;
	}

	valid = TRUE;

done:
	/* Clear any error text in the progress label now that the user has changed something */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "progress_label"));
	gtk_label_set_text (GTK_LABEL (widget), "");

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "unlock_button"));
	g_warn_if_fail (widget != NULL);
	gtk_widget_set_sensitive (widget, valid);
	if (valid)
		gtk_widget_grab_default (widget);
}

static void
mpd_cancel_dialog (GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
}

static void
show_toggled_cb (GtkWidget *button, gpointer user_data)
{
	GtkWidget *dialog = GTK_WIDGET (user_data);
	gboolean show;
	GtkWidget *widget;
	GtkBuilder *builder;

	builder = g_object_get_data (G_OBJECT (dialog), "builder");
	g_return_if_fail (builder != NULL);

	show = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code1_entry"));
	gtk_entry_set_visibility (GTK_ENTRY (widget), show);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code2_entry"));
	gtk_entry_set_visibility (GTK_ENTRY (widget), show);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code3_entry"));
	gtk_entry_set_visibility (GTK_ENTRY (widget), show);
}

static void
mpd_entry_filter (GtkEditable *editable,
                  gchar *text,
                  gint length,
                  gint *position,
                  gpointer user_data)
{
	utils_filter_editable_on_insert_text (editable,
	                                      text, length, position, user_data,
	                                      utils_char_is_ascii_digit,
	                                      mpd_entry_filter);
}

const char *
applet_mobile_pin_dialog_get_entry1 (GtkWidget *dialog)
{
	GtkBuilder *builder;
	GtkWidget *widget;

	g_return_val_if_fail (dialog != NULL, NULL);
	builder = g_object_get_data (G_OBJECT (dialog), "builder");
	g_return_val_if_fail (builder != NULL, NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code1_entry"));
	return gtk_entry_get_text (GTK_ENTRY (widget));
}

const char *
applet_mobile_pin_dialog_get_entry2 (GtkWidget *dialog)
{
	GtkBuilder *builder;
	GtkWidget *widget;

	g_return_val_if_fail (dialog != NULL, NULL);
	builder = g_object_get_data (G_OBJECT (dialog), "builder");
	g_return_val_if_fail (builder != NULL, NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code2_entry"));
	return gtk_entry_get_text (GTK_ENTRY (widget));
}

gboolean
applet_mobile_pin_dialog_get_auto_unlock (GtkWidget *dialog)
{
	GtkBuilder *builder;
	GtkWidget *widget;

	g_return_val_if_fail (dialog != NULL, FALSE);
	builder = g_object_get_data (G_OBJECT (dialog), "builder");
	g_return_val_if_fail (builder != NULL, FALSE);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "save_checkbutton"));
	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

void
applet_mobile_pin_dialog_start_spinner (GtkWidget *dialog, const char *text)
{
	GtkBuilder *builder;
	GtkWidget *spinner, *widget, *hbox, *vbox;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (text != NULL);

	builder = g_object_get_data (G_OBJECT (dialog), "builder");
	g_return_if_fail (builder != NULL);

	spinner = gtk_spinner_new ();
	g_return_if_fail (spinner != NULL);
	g_object_set_data (G_OBJECT (dialog), "spinner", spinner);

	vbox = GTK_WIDGET (gtk_builder_get_object (builder, "spinner_vbox"));
	gtk_container_add (GTK_CONTAINER (vbox), spinner);
	gtk_widget_set_halign (spinner, GTK_ALIGN_FILL);
	gtk_spinner_start (GTK_SPINNER (spinner));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "progress_label"));
	gtk_label_set_text (GTK_LABEL (widget), text);
	gtk_widget_show (widget);

	hbox = GTK_WIDGET (gtk_builder_get_object (builder, "progress_hbox"));
	gtk_widget_show_all (hbox);

	/* Desensitize everything while spinning */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code1_entry"));
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code2_entry"));
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code3_entry"));
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "unlock_button"));
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "unlock_cancel_button"));
	gtk_widget_set_sensitive (widget, FALSE);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "show_password_checkbutton"));
	gtk_widget_set_sensitive (widget, FALSE);
}

void
applet_mobile_pin_dialog_stop_spinner (GtkWidget *dialog, const char *text)
{
	GtkBuilder *builder;
	GtkWidget *spinner, *widget, *vbox;

	g_return_if_fail (dialog != NULL);

	builder = g_object_get_data (G_OBJECT (dialog), "builder");
	g_return_if_fail (builder != NULL);

	spinner = g_object_get_data (G_OBJECT (dialog), "spinner");
	g_return_if_fail (spinner != NULL);
	gtk_spinner_stop (GTK_SPINNER (spinner));
	g_object_set_data (G_OBJECT (dialog), "spinner", NULL);

	/* Remove it from the vbox */
	vbox = GTK_WIDGET (gtk_builder_get_object (builder, "spinner_vbox"));
	gtk_container_remove (GTK_CONTAINER (vbox), spinner);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "progress_label"));
	if (text) {
		gtk_label_set_text (GTK_LABEL (widget), text);
		gtk_widget_show (widget);
	} else
		gtk_widget_hide (widget);

	/* Resensitize stuff */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code1_entry"));
	gtk_widget_set_sensitive (widget, TRUE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code2_entry"));
	gtk_widget_set_sensitive (widget, TRUE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code3_entry"));
	gtk_widget_set_sensitive (widget, TRUE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "unlock_button"));
	gtk_widget_set_sensitive (widget, TRUE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "unlock_cancel_button"));
	gtk_widget_set_sensitive (widget, TRUE);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "show_password_checkbutton"));
	gtk_widget_set_sensitive (widget, TRUE);
}

GtkWidget *
applet_mobile_pin_dialog_new (const char *unlock_required,
                              const char *device_description)
{
	char *str;
	GtkWidget *dialog;
	GtkWidget *widget;
	GtkWidget *label;
	GError *error = NULL;
	GtkBuilder *builder;
	const char *header = NULL;
	const char *title = NULL;
	const char *show_password_label = NULL;
	char *desc = NULL;
	const char *label1 = NULL, *label2 = NULL, *label3 = NULL;
	gboolean match23 = FALSE;
	guint32 label1_min = 0, label2_min = 0, label3_min = 0;
	guint32 label1_max = 0, label2_max = 0, label3_max = 0;
	gboolean puk = FALSE;

	g_return_val_if_fail (unlock_required != NULL, NULL);
	g_return_val_if_fail (!strcmp (unlock_required, "sim-pin") || !strcmp (unlock_required, "sim-puk"), NULL);

	builder = gtk_builder_new ();

	if (!gtk_builder_add_from_file (builder, UIDIR "/gsm-unlock.ui", &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		g_object_unref (builder);
		return NULL;
	}

	dialog = GTK_WIDGET (gtk_builder_get_object (builder, "unlock_dialog"));
	if (!dialog) {
		g_object_unref (builder);
		g_return_val_if_fail (dialog != NULL, NULL);
	}

	g_object_set_data_full (G_OBJECT (dialog), "builder", builder, (GDestroyNotify) g_object_unref);

	/* Figure out the dialog text based on the required unlock code */
	if (!strcmp (unlock_required, "sim-pin")) {
		title = _("SIM PIN unlock required");
		header = _("SIM PIN Unlock Required");
		/* FIXME: some warning about # of times you can enter incorrect PIN */
		desc = g_strdup_printf (_("The mobile broadband device '%s' requires a SIM PIN code before it can be used."), device_description);
		/* Translators: PIN code entry label */
		label1 = _("PIN code:");
		label1_min = 4;
		label1_max = 8;
		/* Translators: Show/obscure PIN checkbox label */
		show_password_label = _("Show PIN code");
	} else if (!strcmp (unlock_required, "sim-puk")) {
		title = _("SIM PUK unlock required");
		header = _("SIM PUK Unlock Required");
		/* FIXME: some warning about # of times you can enter incorrect PUK */
		desc = g_strdup_printf (_("The mobile broadband device '%s' requires a SIM PUK code before it can be used."), device_description);
		/* Translators: PUK code entry label */
		label1 = _("PUK code:");
		label1_min = label1_max = 8;
		/* Translators: New PIN entry label */
		label2 = _("New PIN code:");
		/* Translators: New PIN verification entry label */
		label3 = _("Re-enter new PIN code:");
		label2_min = label3_min = 4;
		label2_max = label3_max = 8;
		match23 = TRUE;
		/* Translators: Show/obscure PIN/PUK checkbox label */
		show_password_label = _("Show PIN/PUK codes");
		puk = TRUE;
	} else
		g_assert_not_reached ();

	gtk_window_set_title (GTK_WINDOW (dialog), title);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "header_label"));
	str = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>", header);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
	gtk_label_set_markup (GTK_LABEL (widget), str);
	g_free (str);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "desc_label"));
	gtk_label_set_text (GTK_LABEL (widget), desc);
	g_free (desc);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "show_password_checkbutton"));
	gtk_button_set_label (GTK_BUTTON (widget), show_password_label);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
	g_signal_connect (widget, "toggled", G_CALLBACK (show_toggled_cb), dialog);
	show_toggled_cb (widget, dialog);

	g_signal_connect (dialog, "delete-event", G_CALLBACK (mpd_cancel_dialog), NULL);

	gtk_widget_show_all (dialog);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "save_checkbutton"));
	if (!puk)
		g_object_set_data (G_OBJECT (widget), "active", GUINT_TO_POINTER (TRUE));
	else
		gtk_widget_hide (widget);

	/* Set contents */
	g_object_set_data (G_OBJECT (dialog), "match23", GUINT_TO_POINTER (match23));

	/* code1_entry */
	label = GTK_WIDGET (gtk_builder_get_object (builder, "code1_label"));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code1_entry"));
	gtk_label_set_text (GTK_LABEL (label), label1);
	g_signal_connect (widget, "changed", G_CALLBACK (mpd_entry_changed), dialog);
	g_signal_connect (widget, "insert-text", G_CALLBACK (mpd_entry_filter), NULL);
	if (label1_max)
		gtk_entry_set_max_length (GTK_ENTRY (widget), label1_max);
	g_object_set_data (G_OBJECT (widget), "minlen", GUINT_TO_POINTER (label1_min));
	g_object_set_data (G_OBJECT (widget), "active", GUINT_TO_POINTER (1));

	/* code2_entry */
	label = GTK_WIDGET (gtk_builder_get_object (builder, "code2_label"));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code2_entry"));
	if (label2) {
		gtk_label_set_text (GTK_LABEL (label), label2);
		g_signal_connect (widget, "changed", G_CALLBACK (mpd_entry_changed), dialog);
		g_signal_connect (widget, "insert-text", G_CALLBACK (mpd_entry_filter), NULL);
		if (label2_max)
			gtk_entry_set_max_length (GTK_ENTRY (widget), label2_max);
		g_object_set_data (G_OBJECT (widget), "minlen", GUINT_TO_POINTER (label2_min));
		g_object_set_data (G_OBJECT (widget), "active", GUINT_TO_POINTER (1));
	} else {
		gtk_widget_hide (label);
		gtk_widget_hide (widget);
	}

	/* code3_entry */
	label = GTK_WIDGET (gtk_builder_get_object (builder, "code3_label"));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "code3_entry"));
	if (label3) {
		gtk_label_set_text (GTK_LABEL (label), label3);
		g_signal_connect (widget, "changed", G_CALLBACK (mpd_entry_changed), dialog);
		g_signal_connect (widget, "insert-text", G_CALLBACK (mpd_entry_filter), NULL);
		if (label3_max)
			gtk_entry_set_max_length (GTK_ENTRY (widget), label3_max);
		g_object_set_data (G_OBJECT (widget), "minlen", GUINT_TO_POINTER (label3_min));
		g_object_set_data (G_OBJECT (widget), "active", GUINT_TO_POINTER (1));
	} else {
		gtk_widget_hide (label);
		gtk_widget_hide (widget);
	}

	/* Make a single-entry dialog look better */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "grid14"));
	if (label2 || label3)
		gtk_grid_set_row_spacing (GTK_GRID (widget), 6);
	else
		gtk_grid_set_row_spacing (GTK_GRID (widget), 0);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "progress_hbox"));
	gtk_widget_hide (widget);

	mpd_entry_changed (NULL, dialog);

	return dialog;
}
