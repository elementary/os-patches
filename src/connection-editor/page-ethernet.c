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
#include <net/ethernet.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <nm-setting-connection.h>
#include <nm-setting-wired.h>
#include <nm-device-ethernet.h>
#include <nm-utils.h>

#include "page-ethernet.h"

G_DEFINE_TYPE (CEPageEthernet, ce_page_ethernet, CE_TYPE_PAGE)

#define CE_PAGE_ETHERNET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_ETHERNET, CEPageEthernetPrivate))

typedef struct {
	NMSettingWired *setting;

	GtkComboBoxText *device_combo; /* Device identification (ifname and/or MAC) */
	GtkEntry *cloned_mac;          /* Cloned MAC - used for MAC spoofing */
	GtkComboBox *port;
	GtkComboBox *speed;
	GtkToggleButton *duplex;
	GtkToggleButton *autonegotiate;
	GtkSpinButton *mtu;
} CEPageEthernetPrivate;

#define PORT_DEFAULT  0
#define PORT_TP       1
#define PORT_AUI      2
#define PORT_BNC      3
#define PORT_MII      4

#define SPEED_DEFAULT 0
#define SPEED_10      1
#define SPEED_100     2
#define SPEED_1000    3
#define SPEED_10000   4

static void
ethernet_private_init (CEPageEthernet *self)
{
	CEPageEthernetPrivate *priv = CE_PAGE_ETHERNET_GET_PRIVATE (self);
	GtkBuilder *builder;
	GtkWidget *align;
	GtkLabel *label;

	builder = CE_PAGE (self)->builder;

	priv->device_combo = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new_with_entry ());
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->device_combo), 0);
	gtk_widget_set_tooltip_text (GTK_WIDGET (priv->device_combo),
	                             _("This option locks this connection to the network device specified "
	                               "either by its interface name or permanent MAC or both. Examples: "
	                               "\"em1\", \"3C:97:0E:42:1A:19\", \"em1 (3C:97:0E:42:1A:19)\""));

	align = GTK_WIDGET (gtk_builder_get_object (builder, "ethernet_device_alignment"));
	gtk_container_add (GTK_CONTAINER (align), GTK_WIDGET (priv->device_combo));
	gtk_widget_show_all (GTK_WIDGET (priv->device_combo));

	/* Set mnemonic widget for Device label */
	label = GTK_LABEL (gtk_builder_get_object (builder, "ethernet_device_label"));
	gtk_label_set_mnemonic_widget (label, GTK_WIDGET (priv->device_combo));

	priv->cloned_mac = GTK_ENTRY (gtk_builder_get_object (builder, "ethernet_cloned_mac"));
	priv->port = GTK_COMBO_BOX (gtk_builder_get_object (builder, "ethernet_port"));
	priv->speed = GTK_COMBO_BOX (gtk_builder_get_object (builder, "ethernet_speed"));
	priv->duplex = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "ethernet_duplex"));
	priv->autonegotiate = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "ethernet_autonegotiate"));
	priv->mtu = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "ethernet_mtu"));
}

static void
stuff_changed (GtkWidget *w, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
populate_ui (CEPageEthernet *self)
{
	CEPageEthernetPrivate *priv = CE_PAGE_ETHERNET_GET_PRIVATE (self);
	NMSettingWired *setting = priv->setting;
	const char *port;
	const char *duplex;
	int port_idx = PORT_DEFAULT;
	int speed_idx;
	int mtu_def;
	const GByteArray *s_mac;
	const char *s_ifname;

	/* Port */
	port = nm_setting_wired_get_port (setting);
	if (port) {
		if (!strcmp (port, "tp"))
			port_idx = PORT_TP;
		else if (!strcmp (port, "aui"))
			port_idx = PORT_AUI;
		else if (!strcmp (port, "bnc"))
			port_idx = PORT_BNC;
		else if (!strcmp (port, "mii"))
			port_idx = PORT_MII;
	}
	gtk_combo_box_set_active (priv->port, port_idx);

	/* Speed */
	switch (nm_setting_wired_get_speed (setting)) {
	case 10:
		speed_idx = SPEED_10;
		break;
	case 100:
		speed_idx = SPEED_100;
		break;
	case 1000:
		speed_idx = SPEED_1000;
		break;
	case 10000:
		speed_idx = SPEED_10000;
		break;
	default:
		speed_idx = SPEED_DEFAULT;
		break;
	}
	gtk_combo_box_set_active (priv->speed, speed_idx);

	/* Duplex */
	duplex = nm_setting_wired_get_duplex (setting);
	if (duplex && !strcmp (duplex, "half"))
		gtk_toggle_button_set_active (priv->duplex, FALSE);
	else
		gtk_toggle_button_set_active (priv->duplex, TRUE);

	/* Autonegotiate */
	gtk_toggle_button_set_active (priv->autonegotiate, 
	                              nm_setting_wired_get_auto_negotiate (setting));

	/* Device ifname/MAC */
	s_ifname = nm_connection_get_interface_name (CE_PAGE (self)->connection);
	s_mac = nm_setting_wired_get_mac_address (setting);
	ce_page_setup_device_combo (CE_PAGE (self), GTK_COMBO_BOX (priv->device_combo),
	                            NM_TYPE_DEVICE_ETHERNET, s_ifname,
	                            s_mac, ARPHRD_ETHER, NM_DEVICE_ETHERNET_PERMANENT_HW_ADDRESS, TRUE);
	g_signal_connect (priv->device_combo, "changed", G_CALLBACK (stuff_changed), self);

	/* Cloned MAC address */
	s_mac = nm_setting_wired_get_cloned_mac_address (setting);
	ce_page_mac_to_entry (s_mac, ARPHRD_ETHER, priv->cloned_mac);
	g_signal_connect (priv->cloned_mac, "changed", G_CALLBACK (stuff_changed), self);

	/* MTU */
	mtu_def = ce_get_property_default (NM_SETTING (setting), NM_SETTING_WIRED_MTU);
	g_signal_connect (priv->mtu, "output",
	                  G_CALLBACK (ce_spin_output_with_automatic),
	                  GINT_TO_POINTER (mtu_def));

	gtk_spin_button_set_value (priv->mtu, (gdouble) nm_setting_wired_get_mtu (setting));
}

static void
finish_setup (CEPageEthernet *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPage *parent = CE_PAGE (self);
	CEPageEthernetPrivate *priv = CE_PAGE_ETHERNET_GET_PRIVATE (self);
	GtkWidget *widget;

	if (error)
		return;

	populate_ui (self);

	g_signal_connect (priv->port, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->speed, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->duplex, "toggled", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->autonegotiate, "toggled", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->mtu, "value-changed", G_CALLBACK (stuff_changed), self);

	/* Hide widgets we don't yet support */
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "ethernet_port_label"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "ethernet_port"));
	gtk_widget_hide (widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "ethernet_speed_label"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "ethernet_speed"));
	gtk_widget_hide (widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "ethernet_duplex"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "ethernet_autonegotiate"));
	gtk_widget_hide (widget);
}

CEPage *
ce_page_ethernet_new (NMConnectionEditor *editor,
                      NMConnection *connection,
                      GtkWindow *parent_window,
                      NMClient *client,
                      NMRemoteSettings *settings,
                      const char **out_secrets_setting_name,
                      GError **error)
{
	CEPageEthernet *self;
	CEPageEthernetPrivate *priv;

	self = CE_PAGE_ETHERNET (ce_page_new (CE_TYPE_PAGE_ETHERNET,
	                                      editor,
	                                      connection,
	                                      parent_window,
	                                      client,
	                                      settings,
	                                      UIDIR "/ce-page-ethernet.ui",
	                                      "EthernetPage",
	                                      _("Ethernet")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load ethernet user interface."));
		return NULL;
	}

	ethernet_private_init (self);
	priv = CE_PAGE_ETHERNET_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_wired (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_WIRED (nm_setting_wired_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageEthernet *self)
{
	CEPageEthernetPrivate *priv = CE_PAGE_ETHERNET_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	const char *port;
	guint32 speed;
	char *ifname = NULL;
	GByteArray *device_mac = NULL;
	GByteArray *cloned_mac = NULL;
	GtkWidget *entry;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	g_return_if_fail (s_con != NULL);

	/* Port */
	switch (gtk_combo_box_get_active (priv->port)) {
	case PORT_TP:
		port = "tp";
		break;
	case PORT_AUI:
		port = "aui";
		break;
	case PORT_BNC:
		port = "bnc";
		break;
	case PORT_MII:
		port = "mii";
		break;
	default:
		port = NULL;
		break;
	}

	/* Speed */
	switch (gtk_combo_box_get_active (priv->speed)) {
	case SPEED_10:
		speed = 10;
		break;
	case SPEED_100:
		speed = 100;
		break;
	case SPEED_1000:
		speed = 1000;
		break;
	case SPEED_10000:
		speed = 10000;
		break;
	default:
		speed = 0;
		break;
	}

	entry = gtk_bin_get_child (GTK_BIN (priv->device_combo));
	if (entry)
		ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, TRUE, &ifname, &device_mac, NULL, NULL);
	cloned_mac = ce_page_entry_to_mac (priv->cloned_mac, ARPHRD_ETHER, NULL);

	g_object_set (s_con,
	              NM_SETTING_CONNECTION_INTERFACE_NAME, ifname,
	              NULL);
	g_object_set (priv->setting,
	              NM_SETTING_WIRED_MAC_ADDRESS, device_mac,
	              NM_SETTING_WIRED_CLONED_MAC_ADDRESS, cloned_mac,
	              NM_SETTING_WIRED_PORT, port,
	              NM_SETTING_WIRED_SPEED, speed,
	              NM_SETTING_WIRED_DUPLEX, gtk_toggle_button_get_active (priv->duplex) ? "full" : "half",
	              NM_SETTING_WIRED_AUTO_NEGOTIATE, gtk_toggle_button_get_active (priv->autonegotiate),
	              NM_SETTING_WIRED_MTU, (guint32) gtk_spin_button_get_value_as_int (priv->mtu),
	              NULL);

	g_free (ifname);
	if (device_mac)
		g_byte_array_free (device_mac, TRUE);
	if (cloned_mac)
		g_byte_array_free (cloned_mac, TRUE);

}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageEthernet *self = CE_PAGE_ETHERNET (page);
	CEPageEthernetPrivate *priv = CE_PAGE_ETHERNET_GET_PRIVATE (self);
	GtkWidget *entry;

	entry = gtk_bin_get_child (GTK_BIN (priv->device_combo));
	if (entry) {
		if (!ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, TRUE, NULL, NULL, _("Ethernet device"), error))
			return FALSE;
	}

	if (!ce_page_mac_entry_valid (priv->cloned_mac, ARPHRD_ETHER, _("cloned MAC"), error))
		return FALSE;

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_ethernet_init (CEPageEthernet *self)
{
}

static void
ce_page_ethernet_class_init (CEPageEthernetClass *ethernet_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (ethernet_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (ethernet_class);

	g_type_class_add_private (object_class, sizeof (CEPageEthernetPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
}


void
ethernet_connection_new (GtkWindow *parent,
                         const char *detail,
                         NMRemoteSettings *settings,
                         PageNewConnectionResultFunc result_func,
                         gpointer user_data)
{
	NMConnection *connection;

	connection = ce_page_new_connection (_("Ethernet connection %d"),
	                                     NM_SETTING_WIRED_SETTING_NAME,
	                                     TRUE,
	                                     settings,
	                                     user_data);
	nm_connection_add_setting (connection, nm_setting_wired_new ());

	(*result_func) (connection, FALSE, NULL, user_data);
}

