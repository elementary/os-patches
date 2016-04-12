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
 * Copyright 2012 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <stdlib.h>

#include "page-bridge.h"
#include "nm-connection-editor.h"
#include "connection-helpers.h"

G_DEFINE_TYPE (CEPageBridge, ce_page_bridge, CE_TYPE_PAGE_MASTER)

#define CE_PAGE_BRIDGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_BRIDGE, CEPageBridgePrivate))

typedef struct {
	NMSettingBridge *setting;

	GtkWindow *toplevel;

	GtkSpinButton *ageing_time;
	GtkCheckButton *mcast_snoop;
	GtkCheckButton *stp;
	GtkSpinButton *priority;
	GtkSpinButton *forward_delay;
	GtkSpinButton *hello_time;
	GtkSpinButton *max_age;

} CEPageBridgePrivate;

static void
bridge_private_init (CEPageBridge *self)
{
	CEPageBridgePrivate *priv = CE_PAGE_BRIDGE_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->ageing_time = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bridge_ageing_time"));
	priv->mcast_snoop = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "bridge_mcast_snoop_checkbox"));
	priv->stp = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "bridge_stp_checkbox"));
	priv->priority = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bridge_priority"));
	priv->forward_delay = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bridge_forward_delay"));
	priv->hello_time = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bridge_hello_time"));
	priv->max_age = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bridge_max_age"));

	priv->toplevel = GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (priv->stp),
	                                                      GTK_TYPE_WINDOW));
}

static void
stuff_changed (GtkWidget *w, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
stp_toggled (GtkToggleButton *stp, gpointer user_data)
{
	CEPageBridge *self = user_data;
	CEPageBridgePrivate *priv = CE_PAGE_BRIDGE_GET_PRIVATE (self);

	if (gtk_toggle_button_get_active (stp)) {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->priority), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->forward_delay), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->hello_time), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->max_age), TRUE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->priority), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->forward_delay), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->hello_time), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->max_age), FALSE);
	}
	ce_page_changed (CE_PAGE (user_data));
}

static void
populate_ui (CEPageBridge *self)
{
	CEPageBridgePrivate *priv = CE_PAGE_BRIDGE_GET_PRIVATE (self);
	NMSettingBridge *s_bridge = priv->setting;
	gboolean stp, mcast_snoop;
	int priority, forward_delay, hello_time, max_age;
	int ageing_time;

	/* Ageing time */
	ageing_time = nm_setting_bridge_get_ageing_time (s_bridge);
	gtk_spin_button_set_value (priv->ageing_time, (gdouble) ageing_time);
	g_signal_connect (priv->ageing_time, "value-changed",
	                  G_CALLBACK (stuff_changed),
	                  self);

	/* Multicast snooping */
	mcast_snoop = nm_setting_bridge_get_multicast_snooping (s_bridge);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->mcast_snoop), mcast_snoop);
	g_signal_connect (priv->mcast_snoop, "toggled", G_CALLBACK (stuff_changed), self);

	/* STP */
	g_signal_connect (priv->stp, "toggled",
	                  G_CALLBACK (stp_toggled),
	                  self);
	stp = nm_setting_bridge_get_stp (s_bridge);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->stp), stp);
	stp_toggled (GTK_TOGGLE_BUTTON (priv->stp), self);

	/* Priority */
	priority = nm_setting_bridge_get_priority (s_bridge);
	gtk_spin_button_set_value (priv->priority, (gdouble) priority);
	g_signal_connect (priv->priority, "value-changed",
	                  G_CALLBACK (stuff_changed),
	                  self);

	/* Forward delay */
	forward_delay = nm_setting_bridge_get_forward_delay (s_bridge);
	gtk_spin_button_set_value (priv->forward_delay, (gdouble) forward_delay);
	g_signal_connect (priv->forward_delay, "value-changed",
	                  G_CALLBACK (stuff_changed),
	                  self);

	/* Hello time */
	hello_time = nm_setting_bridge_get_hello_time (s_bridge);
	gtk_spin_button_set_value (priv->hello_time, (gdouble) hello_time);
	g_signal_connect (priv->hello_time, "value-changed",
	                  G_CALLBACK (stuff_changed),
	                  self);

	/* Max age */
	max_age = nm_setting_bridge_get_max_age (s_bridge);
	gtk_spin_button_set_value (priv->max_age, (gdouble) max_age);
	g_signal_connect (priv->max_age, "value-changed",
	                  G_CALLBACK (stuff_changed),
	                  self);
}

static void
create_connection (CEPageMaster *master, NMConnection *connection)
{
	NMSetting *s_port;

	s_port = nm_connection_get_setting (connection, NM_TYPE_SETTING_BRIDGE_PORT);
	if (!s_port) {
		s_port = nm_setting_bridge_port_new ();
		nm_connection_add_setting (connection, s_port);
	}
}

static gboolean
connection_type_filter (GType type, gpointer user_data)
{
	return nm_utils_check_virtual_device_compatibility (NM_TYPE_SETTING_BRIDGE, type);
}

static void
add_slave (CEPageMaster *master, NewConnectionResultFunc result_func)
{
	CEPageBridge *self = CE_PAGE_BRIDGE (master);
	CEPageBridgePrivate *priv = CE_PAGE_BRIDGE_GET_PRIVATE (self);

	new_connection_dialog (priv->toplevel,
	                       CE_PAGE (self)->client,
	                       connection_type_filter,
	                       result_func,
	                       master);
}

static void
finish_setup (CEPageBridge *self, gpointer unused, GError *error, gpointer user_data)
{
	if (error)
		return;

	populate_ui (self);
}

CEPage *
ce_page_bridge_new (NMConnectionEditor *editor,
                    NMConnection *connection,
                    GtkWindow *parent_window,
                    NMClient *client,
                    const char **out_secrets_setting_name,
                    GError **error)
{
	CEPageBridge *self;
	CEPageBridgePrivate *priv;

	self = CE_PAGE_BRIDGE (ce_page_new (CE_TYPE_PAGE_BRIDGE,
	                                  editor,
	                                  connection,
	                                  parent_window,
	                                  client,
	                                  UIDIR "/ce-page-bridge.ui",
	                                  "BridgePage",
	                                  _("Bridge")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC,
		                     _("Could not load bridge user interface."));
		return NULL;
	}

	bridge_private_init (self);
	priv = CE_PAGE_BRIDGE_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_bridge (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_BRIDGE (nm_setting_bridge_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageBridge *self)
{
	CEPageBridgePrivate *priv = CE_PAGE_BRIDGE_GET_PRIVATE (self);
	int ageing_time, priority, forward_delay, hello_time, max_age;
	gboolean stp, mcast_snoop;

	ageing_time = gtk_spin_button_get_value_as_int (priv->ageing_time);
	mcast_snoop = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->mcast_snoop));
	stp = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->stp));
	g_object_set (G_OBJECT (priv->setting),
	              NM_SETTING_BRIDGE_AGEING_TIME, ageing_time,
	              NM_SETTING_BRIDGE_MULTICAST_SNOOPING, mcast_snoop,
	              NM_SETTING_BRIDGE_STP, stp,
	              NULL);

	if (stp) {
		priority = gtk_spin_button_get_value_as_int (priv->priority);
		forward_delay = gtk_spin_button_get_value_as_int (priv->forward_delay);
		hello_time = gtk_spin_button_get_value_as_int (priv->hello_time);
		max_age = gtk_spin_button_get_value_as_int (priv->max_age);

		g_object_set (G_OBJECT (priv->setting),
		              NM_SETTING_BRIDGE_PRIORITY, priority,
		              NM_SETTING_BRIDGE_FORWARD_DELAY, forward_delay,
		              NM_SETTING_BRIDGE_HELLO_TIME, hello_time,
		              NM_SETTING_BRIDGE_MAX_AGE, max_age,
		              NULL);
	}
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageBridge *self = CE_PAGE_BRIDGE (page);
	CEPageBridgePrivate *priv = CE_PAGE_BRIDGE_GET_PRIVATE (self);

	if (!CE_PAGE_CLASS (ce_page_bridge_parent_class)->ce_page_validate_v (page, connection, error))
		return FALSE;

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), connection, error);
}

static void
ce_page_bridge_init (CEPageBridge *self)
{
}

static void
ce_page_bridge_class_init (CEPageBridgeClass *bridge_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (bridge_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (bridge_class);
	CEPageMasterClass *master_class = CE_PAGE_MASTER_CLASS (bridge_class);

	g_type_class_add_private (object_class, sizeof (CEPageBridgePrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
	master_class->create_connection = create_connection;
	master_class->add_slave = add_slave;
}

void
bridge_connection_new (GtkWindow *parent,
                       const char *detail,
                       NMClient *client,
                       PageNewConnectionResultFunc result_func,
                       gpointer user_data)
{
	NMConnection *connection;
	NMSettingConnection *s_con;
	int bridge_num = 0, num, i;
	const GPtrArray *connections;
	NMConnection *conn2;
	const char *iface;
	char *my_iface;

	connection = ce_page_new_connection (_("Bridge connection %d"),
	                                     NM_SETTING_BRIDGE_SETTING_NAME,
	                                     TRUE,
	                                     client,
	                                     user_data);
	nm_connection_add_setting (connection, nm_setting_bridge_new ());

	/* Find an available interface name */
	connections = nm_client_get_connections (client);
	for (i = 0; i < connections->len; i++) {
		conn2 = connections->pdata[i];

		if (!nm_connection_is_type (conn2, NM_SETTING_BRIDGE_SETTING_NAME))
			continue;
		iface = nm_connection_get_interface_name (conn2);
		if (!iface || strncmp (iface, "bridge", 6) != 0 || !g_ascii_isdigit (iface[6]))
			continue;

		num = atoi (iface + 6);
		if (bridge_num <= num)
			bridge_num = num + 1;
	}

	s_con = nm_connection_get_setting_connection (connection);
	my_iface = g_strdup_printf ("bridge%d", bridge_num);
	g_object_set (G_OBJECT (s_con),
	              NM_SETTING_CONNECTION_INTERFACE_NAME, my_iface,
	              NULL);
	g_free (my_iface);

	(*result_func) (connection, FALSE, NULL, user_data);
}

