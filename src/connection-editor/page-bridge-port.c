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

#include "page-bridge-port.h"

G_DEFINE_TYPE (CEPageBridgePort, ce_page_bridge_port, CE_TYPE_PAGE)

#define CE_PAGE_BRIDGE_PORT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_BRIDGE_PORT, CEPageBridgePortPrivate))

typedef struct {
	NMSettingBridgePort *setting;

	GtkSpinButton *priority;
	GtkSpinButton *path_cost;
	GtkToggleButton *hairpin_mode;

} CEPageBridgePortPrivate;

static void
bridge_port_private_init (CEPageBridgePort *self)
{
	CEPageBridgePortPrivate *priv = CE_PAGE_BRIDGE_PORT_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->priority = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bridge_port_priority"));
	priv->path_cost = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "bridge_port_path_cost"));
	priv->hairpin_mode = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "bridge_port_hairpin_mode"));
}

static void
stuff_changed (GtkWidget *w, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
populate_ui (CEPageBridgePort *self)
{
	CEPageBridgePortPrivate *priv = CE_PAGE_BRIDGE_PORT_GET_PRIVATE (self);
	NMSettingBridgePort *s_port = priv->setting;

	gtk_spin_button_set_value (priv->priority, (gdouble) nm_setting_bridge_port_get_priority (s_port));
	gtk_spin_button_set_value (priv->path_cost, (gdouble) nm_setting_bridge_port_get_path_cost (s_port));
	gtk_toggle_button_set_active (priv->hairpin_mode, nm_setting_bridge_port_get_hairpin_mode (s_port));
}

static void
finish_setup (CEPageBridgePort *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPageBridgePortPrivate *priv = CE_PAGE_BRIDGE_PORT_GET_PRIVATE (self);

	if (error)
		return;

	populate_ui (self);

	g_signal_connect (priv->priority, "value-changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->path_cost, "value-changed", G_CALLBACK (stuff_changed), self);
	g_signal_connect (priv->hairpin_mode, "toggled", G_CALLBACK (stuff_changed), self);
}

CEPage *
ce_page_bridge_port_new (NMConnectionEditor *editor,
                         NMConnection *connection,
                         GtkWindow *parent_window,
                         NMClient *client,
                         const char **out_secrets_setting_name,
                         GError **error)
{
	CEPageBridgePort *self;
	CEPageBridgePortPrivate *priv;

	self = CE_PAGE_BRIDGE_PORT (ce_page_new (CE_TYPE_PAGE_BRIDGE_PORT,
	                                         editor,
	                                         connection,
	                                         parent_window,
	                                         client,
	                                         UIDIR "/ce-page-bridge-port.ui",
	                                         "BridgePortPage",
	                                         /* Translators: a "Bridge Port" is a network
	                                          * device that is part of a bridge.
	                                          */
	                                         _("Bridge Port")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load bridge port user interface."));
		return NULL;
	}

	bridge_port_private_init (self);
	priv = CE_PAGE_BRIDGE_PORT_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_bridge_port (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_BRIDGE_PORT (nm_setting_bridge_port_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageBridgePort *self)
{
	CEPageBridgePortPrivate *priv = CE_PAGE_BRIDGE_PORT_GET_PRIVATE (self);

	g_object_set (priv->setting,
	              NM_SETTING_BRIDGE_PORT_HAIRPIN_MODE, gtk_toggle_button_get_active (priv->hairpin_mode),
	              NM_SETTING_BRIDGE_PORT_PRIORITY, gtk_spin_button_get_value_as_int (priv->priority),
	              NM_SETTING_BRIDGE_PORT_PATH_COST, gtk_spin_button_get_value_as_int (priv->path_cost),
	              NULL);
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageBridgePort *self = CE_PAGE_BRIDGE_PORT (page);
	CEPageBridgePortPrivate *priv = CE_PAGE_BRIDGE_PORT_GET_PRIVATE (self);

	ui_to_setting (self);
	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_bridge_port_init (CEPageBridgePort *self)
{
}

static void
ce_page_bridge_port_class_init (CEPageBridgePortClass *bridge_port_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (bridge_port_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (bridge_port_class);

	g_type_class_add_private (object_class, sizeof (CEPageBridgePortPrivate));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
}
