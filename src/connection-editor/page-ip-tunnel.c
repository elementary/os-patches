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
 * Copyright 2016 Red Hat, Inc.
 */

#include "nm-default.h"

#include "page-ip-tunnel.h"

#include "nm-connection-editor.h"

G_DEFINE_TYPE (CEPageIPTunnel, ce_page_ip_tunnel, CE_TYPE_PAGE)

#define CE_PAGE_IP_TUNNEL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_IP_TUNNEL, CEPageIPTunnelPrivate))

typedef struct {
	NMSettingIPTunnel *setting;

	GtkEntry *name;
	GtkComboBoxText *parent;
	GtkComboBox *mode;
	GtkEntry *local;
	GtkEntry *remote;
	GtkEntry *input_key;
	GtkEntry *output_key;
	GtkSpinButton *mtu;
} CEPageIPTunnelPrivate;

static void
ip_tunnel_private_init (CEPageIPTunnel *self)
{
	CEPageIPTunnelPrivate *priv = CE_PAGE_IP_TUNNEL_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->name = GTK_ENTRY (gtk_builder_get_object (builder, "ip_tunnel_name"));
	priv->parent = GTK_COMBO_BOX_TEXT (gtk_builder_get_object (builder, "ip_tunnel_parent"));
	priv->mode = GTK_COMBO_BOX (gtk_builder_get_object (builder, "ip_tunnel_mode"));
	priv->local = GTK_ENTRY (gtk_builder_get_object (builder, "ip_tunnel_local"));
	priv->remote = GTK_ENTRY (gtk_builder_get_object (builder, "ip_tunnel_remote"));
	priv->input_key = GTK_ENTRY (gtk_builder_get_object (builder, "ip_tunnel_ikey"));
	priv->output_key = GTK_ENTRY (gtk_builder_get_object (builder, "ip_tunnel_okey"));
	priv->mtu = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "ip_tunnel_mtu"));
}

static void
mode_changed (GtkComboBox *combo, gpointer user_data)
{
	CEPageIPTunnel *self = user_data;
	CEPageIPTunnelPrivate *priv = CE_PAGE_IP_TUNNEL_GET_PRIVATE (self);
	NMIPTunnelMode mode;
	gboolean enable;

	mode = gtk_combo_box_get_active (combo) + 1;

	enable = (mode == NM_IP_TUNNEL_MODE_GRE || mode == NM_IP_TUNNEL_MODE_IP6GRE);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->input_key), enable);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->output_key), enable);

	if (!enable) {
		gtk_entry_set_text (priv->input_key, "");
		gtk_entry_set_text (priv->output_key, "");
	}
}

static void
populate_ui (CEPageIPTunnel *self, NMConnection *connection)
{
	CEPageIPTunnelPrivate *priv = CE_PAGE_IP_TUNNEL_GET_PRIVATE (self);
	NMSettingIPTunnel *setting = priv->setting;
	NMIPTunnelMode mode;
	const char *str;
	int mtu_def;

	str = nm_connection_get_interface_name (CE_PAGE (self)->connection);
	if (str)
		gtk_entry_set_text (priv->name, str);

	str = nm_setting_ip_tunnel_get_parent (setting);
	ce_page_setup_device_combo (CE_PAGE (self), GTK_COMBO_BOX (priv->parent),
	                            G_TYPE_NONE, str,
	                            NULL, NULL);

	mode = nm_setting_ip_tunnel_get_mode (setting);
	if (mode >= NM_IP_TUNNEL_MODE_IPIP && mode <= NM_IP_TUNNEL_MODE_VTI6)
		gtk_combo_box_set_active (priv->mode, mode - 1);

	str = nm_setting_ip_tunnel_get_local (setting);
	if (str)
		gtk_entry_set_text (priv->local, str);

	str = nm_setting_ip_tunnel_get_remote (setting);
	if (str)
		gtk_entry_set_text (priv->remote, str);

	str = nm_setting_ip_tunnel_get_input_key (setting);
	if (str)
		gtk_entry_set_text (priv->input_key, str);

	str = nm_setting_ip_tunnel_get_output_key (setting);
	if (str)
		gtk_entry_set_text (priv->output_key, str);

	mtu_def = ce_get_property_default (NM_SETTING (setting), NM_SETTING_IP_TUNNEL_MTU);
	ce_spin_automatic_val (priv->mtu, mtu_def);

	gtk_spin_button_set_value (priv->mtu, (gdouble) nm_setting_ip_tunnel_get_mtu (setting));

	mode_changed (priv->mode, self);
}

static void
stuff_changed (GtkEditable *editable, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
finish_setup (CEPageIPTunnel *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPage *parent = CE_PAGE (self);
	CEPageIPTunnelPrivate *priv = CE_PAGE_IP_TUNNEL_GET_PRIVATE (self);

	if (error)
		return;

	populate_ui (self, parent->connection);

	g_signal_connect (priv->name, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->parent, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->mode, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->local, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->remote, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->input_key, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->output_key, "changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->mtu, "value-changed", G_CALLBACK (stuff_changed), self);

	g_signal_connect (priv->mode, "changed", G_CALLBACK (mode_changed), self);
}

CEPage *
ce_page_ip_tunnel_new (NMConnectionEditor *editor,
                       NMConnection *connection,
                       GtkWindow *parent_window,
                       NMClient *client,
                       const char **out_secrets_setting_name,
                       GError **error)
{
	CEPageIPTunnel *self;
	CEPageIPTunnelPrivate *priv;

	self = CE_PAGE_IP_TUNNEL (ce_page_new (CE_TYPE_PAGE_IP_TUNNEL,
	                                       editor,
	                                       connection,
	                                       parent_window,
	                                       client,
	                                       "/org/freedesktop/network-manager-applet/ce-page-ip-tunnel.ui",
	                                       "IPTunnelPage",
	                                       _("IP tunnel")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load IP tunnel user interface."));
		return NULL;
	}

	ip_tunnel_private_init (self);
	priv = CE_PAGE_IP_TUNNEL_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_ip_tunnel (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_IP_TUNNEL (nm_setting_ip_tunnel_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageIPTunnel *self)
{
	CEPageIPTunnelPrivate *priv = CE_PAGE_IP_TUNNEL_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	const char *parent = NULL;
	const char *local;
	const char *remote;
	const char *input_key;
	const char *output_key;
	NMIPTunnelMode mode;
	GtkWidget *entry;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	g_return_if_fail (s_con != NULL);
	g_object_set (s_con,
	              NM_SETTING_CONNECTION_INTERFACE_NAME, gtk_entry_get_text (priv->name),
	              NULL);

	entry = gtk_bin_get_child (GTK_BIN (priv->parent));
	if (entry) {
		ce_page_device_entry_get (GTK_ENTRY (entry), ARPHRD_ETHER, TRUE,
		                          (char **) &parent, NULL, NULL, NULL);
	}

	mode = gtk_combo_box_get_active (priv->mode) + 1;

	local = gtk_entry_get_text (priv->local);
	if (local && !local[0])
		local = NULL;

	remote = gtk_entry_get_text (priv->remote);
	if (remote && !remote[0])
		remote = NULL;

	input_key = gtk_entry_get_text (priv->input_key);
	if (input_key && !input_key[0])
		input_key = NULL;


	output_key = gtk_entry_get_text (priv->output_key);
	if (output_key && !output_key[0])
		output_key = NULL;

	g_object_set (priv->setting,
	              NM_SETTING_IP_TUNNEL_PARENT, parent,
	              NM_SETTING_IP_TUNNEL_MODE, mode,
	              NM_SETTING_IP_TUNNEL_LOCAL, local,
	              NM_SETTING_IP_TUNNEL_REMOTE, remote,
	              NM_SETTING_IP_TUNNEL_INPUT_KEY, input_key,
	              NM_SETTING_IP_TUNNEL_OUTPUT_KEY, output_key,
	              NM_SETTING_IP_TUNNEL_MTU, gtk_spin_button_get_value_as_int (priv->mtu),
	              NULL);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageIPTunnel *self = CE_PAGE_IP_TUNNEL (page);
	CEPageIPTunnelPrivate *priv = CE_PAGE_IP_TUNNEL_GET_PRIVATE (self);

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), connection, error);
}

static void
ce_page_ip_tunnel_init (CEPageIPTunnel *self)
{
}

static void
ce_page_ip_tunnel_class_init (CEPageIPTunnelClass *ip_tunnel_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (ip_tunnel_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (ip_tunnel_class);

	g_type_class_add_private (object_class, sizeof (CEPageIPTunnelPrivate));

	parent_class->ce_page_validate_v = ce_page_validate_v;
}

void
ip_tunnel_connection_new (FUNC_TAG_PAGE_NEW_CONNECTION_IMPL,
                          GtkWindow *parent,
                          const char *detail,
                          gpointer detail_data,
                          NMConnection *connection,
                          NMClient *client,
                          PageNewConnectionResultFunc result_func,
                          gpointer user_data)
{
	NMSetting *s_ip_tunnel;
	gs_unref_object NMConnection *connection_tmp = NULL;

	connection = _ensure_connection_other (connection, &connection_tmp);
	ce_page_complete_connection (connection,
	                             _("IP tunnel connection %d"),
	                             NM_SETTING_IP_TUNNEL_SETTING_NAME,
	                             FALSE,
	                             client);

	s_ip_tunnel = nm_setting_ip_tunnel_new ();
	g_object_set (s_ip_tunnel, NM_SETTING_IP_TUNNEL_MODE, NM_IP_TUNNEL_MODE_IPIP, NULL);
	nm_connection_add_setting (connection, s_ip_tunnel);

	(*result_func) (FUNC_TAG_PAGE_NEW_CONNECTION_RESULT_CALL, connection, FALSE, NULL, user_data);
}
