/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
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
 * Copyright 2012 Red Hat, Inc.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <nm-setting-connection.h>
#include <nm-setting-infiniband.h>
#include <nm-device-infiniband.h>
#include <nm-utils.h>

#include <net/if_arp.h>
#include <linux/if_infiniband.h>

#include "page-infiniband.h"

G_DEFINE_TYPE (CEPageInfiniband, ce_page_infiniband, CE_TYPE_PAGE)

#define CE_PAGE_INFINIBAND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_INFINIBAND, CEPageInfinibandPrivate))

typedef struct {
	NMSettingInfiniband *setting;

	GtkComboBoxText *device_combo; /* Device identification (ifname and/or MAC) */

	GtkComboBox *transport_mode;
	GtkSpinButton *mtu;
} CEPageInfinibandPrivate;

#define TRANSPORT_MODE_DATAGRAM  0
#define TRANSPORT_MODE_CONNECTED 1

static void
infiniband_private_init (CEPageInfiniband *self)
{
	CEPageInfinibandPrivate *priv = CE_PAGE_INFINIBAND_GET_PRIVATE (self);
	GtkBuilder *builder;
	GtkWidget *align;
	GtkLabel *label;

	builder = CE_PAGE (self)->builder;

	priv->device_combo = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new_with_entry ());
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->device_combo), 0);
	gtk_widget_set_tooltip_text (GTK_WIDGET (priv->device_combo),
	                             _("This option locks this connection to the network device specified "
	                               "either by its interface name or permanent MAC or both. Examples: "
	                               "\"ib0\", \"80:00:00:48:fe:80:00:00:00:00:00:00:00:02:c9:03:00:00:0f:65\", "
	                               "\"ib0 (80:00:00:48:fe:80:00:00:00:00:00:00:00:02:c9:03:00:00:0f:65)\""));

	align = GTK_WIDGET (gtk_builder_get_object (builder, "infiniband_device_alignment"));
	gtk_container_add (GTK_CONTAINER (align), GTK_WIDGET (priv->device_combo));
	gtk_widget_show_all (GTK_WIDGET (priv->device_combo));

	/* Set mnemonic widget for Device label */
	label = GTK_LABEL (gtk_builder_get_object (builder, "infiniband_device_label"));
	gtk_label_set_mnemonic_widget (label, GTK_WIDGET (priv->device_combo));

	priv->transport_mode = GTK_COMBO_BOX (gtk_builder_get_object (builder, "infiniband_mode"));
	priv->mtu = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "infiniband_mtu"));
}

static void
stuff_changed (GtkWidget *w, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
populate_ui (CEPageInfiniband *self)
{
	CEPageInfinibandPrivate *priv = CE_PAGE_INFINIBAND_GET_PRIVATE (self);
	NMSettingInfiniband *setting = priv->setting;
	const char *mode;
	int mode_idx = TRANSPORT_MODE_DATAGRAM;
	int mtu_def;
	const GByteArray *s_mac;
	const char *s_ifname;

	/* Port */
	mode = nm_setting_infiniband_get_transport_mode (setting);
	if (mode) {
		if (!strcmp (mode, "datagram"))
			mode_idx = TRANSPORT_MODE_DATAGRAM;
		else if (!strcmp (mode, "connected"))
			mode_idx = TRANSPORT_MODE_CONNECTED;
	}
	gtk_combo_box_set_active (priv->transport_mode, mode_idx);

	/* Device */
	s_ifname = nm_connection_get_interface_name (CE_PAGE (self)->connection);
	s_mac = nm_setting_infiniband_get_mac_address (setting);
	ce_page_setup_device_combo (CE_PAGE (self), GTK_COMBO_BOX (priv->device_combo),
	                            NM_TYPE_DEVICE_INFINIBAND, s_ifname,
	                            s_mac, ARPHRD_INFINIBAND, NM_DEVICE_INFINIBAND_HW_ADDRESS, TRUE);
	g_signal_connect (priv->device_combo, "changed", G_CALLBACK (stuff_changed), self);

	/* MTU */
	mtu_def = ce_get_property_default (NM_SETTING (setting), NM_SETTING_INFINIBAND_MTU);
	g_signal_connect (priv->mtu, "output",
	                  G_CALLBACK (ce_spin_output_with_automatic),
	                  GINT_TO_POINTER (mtu_def));

	gtk_spin_button_set_value (priv->mtu, (gdouble) nm_setting_infiniband_get_mtu (setting));
}

static void
finish_setup (CEPageInfiniband *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPageInfinibandPrivate *priv = CE_PAGE_INFINIBAND_GET_PRIVATE (self);

	if (error)
		return;

	populate_ui (self);

	g_signal_connect (priv->transport_mode, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->mtu, "value-changed", G_CALLBACK (stuff_changed), self);
}

CEPage *
ce_page_infiniband_new (NMConnectionEditor *editor,
                        NMConnection *connection,
                        GtkWindow *parent_window,
                        NMClient *client,
                        NMRemoteSettings *settings,
                        const char **out_secrets_setting_name,
                        GError **error)
{
	CEPageInfiniband *self;
	CEPageInfinibandPrivate *priv;

	self = CE_PAGE_INFINIBAND (ce_page_new (CE_TYPE_PAGE_INFINIBAND,
	                                        editor,
	                                        connection,
	                                        parent_window,
	                                        client,
	                                        settings,
	                                        UIDIR "/ce-page-infiniband.ui",
	                                        "InfinibandPage",
	                                        _("InfiniBand")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC,
		                     _("Could not load InfiniBand user interface."));
		return NULL;
	}

	infiniband_private_init (self);
	priv = CE_PAGE_INFINIBAND_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_infiniband (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_INFINIBAND (nm_setting_infiniband_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageInfiniband *self)
{
	CEPageInfinibandPrivate *priv = CE_PAGE_INFINIBAND_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	const char *mode;
	char *ifname = NULL;
	GByteArray *device_mac = NULL;
	GtkWidget *entry;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	g_return_if_fail (s_con != NULL);

	/* Transport mode */
	if (gtk_combo_box_get_active (priv->transport_mode) == TRANSPORT_MODE_CONNECTED)
		mode = "connected";
	else
		mode = "datagram";

	entry = gtk_bin_get_child (GTK_BIN (priv->device_combo));
	if (entry)
		ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_INFINIBAND, TRUE, &ifname, &device_mac, NULL, NULL);

	g_object_set (s_con,
	              NM_SETTING_CONNECTION_INTERFACE_NAME, ifname,
	              NULL);
	g_object_set (priv->setting,
	              NM_SETTING_INFINIBAND_MAC_ADDRESS, device_mac,
	              NM_SETTING_INFINIBAND_MTU, (guint32) gtk_spin_button_get_value_as_int (priv->mtu),
	              NM_SETTING_INFINIBAND_TRANSPORT_MODE, mode,
	              NULL);

	g_free (ifname);
	if (device_mac)
		g_byte_array_free (device_mac, TRUE);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageInfiniband *self = CE_PAGE_INFINIBAND (page);
	CEPageInfinibandPrivate *priv = CE_PAGE_INFINIBAND_GET_PRIVATE (self);
	GtkWidget *entry;

	entry = gtk_bin_get_child (GTK_BIN (priv->device_combo));
	if (entry) {
		if (!ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_INFINIBAND, TRUE, NULL, NULL, _("infiniband device"), error))
			return FALSE;
	}

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_infiniband_init (CEPageInfiniband *self)
{
}

static void
ce_page_infiniband_class_init (CEPageInfinibandClass *infiniband_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (infiniband_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (infiniband_class);

	g_type_class_add_private (object_class, sizeof (CEPageInfinibandPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
}


void
infiniband_connection_new (GtkWindow *parent,
                           const char *detail,
                           NMRemoteSettings *settings,
                           PageNewConnectionResultFunc result_func,
                           gpointer user_data)
{
	NMConnection *connection;

	connection = ce_page_new_connection (_("InfiniBand connection %d"),
	                                     NM_SETTING_INFINIBAND_SETTING_NAME,
	                                     TRUE,
	                                     settings,
	                                     user_data);
	nm_connection_add_setting (connection, nm_setting_infiniband_new ());

	(*result_func) (connection, FALSE, NULL, user_data);
}

