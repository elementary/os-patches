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

#include "page-vpn.h"
#include "connection-helpers.h"
#include "nm-connection-editor.h"
#include "vpn-helpers.h"

G_DEFINE_TYPE (CEPageVpn, ce_page_vpn, CE_TYPE_PAGE)

#define CE_PAGE_VPN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_VPN, CEPageVpnPrivate))

typedef struct {
	NMSettingVpn *setting;

	char *service_type;

	NMVpnEditorPlugin *plugin;
	NMVpnEditor *editor;
} CEPageVpnPrivate;

static void
vpn_plugin_changed_cb (NMVpnEditorPlugin *plugin, CEPageVpn *self)
{
	ce_page_changed (CE_PAGE (self));
}

static void
finish_setup (CEPageVpn *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPage *parent = CE_PAGE (self);
	CEPageVpnPrivate *priv = CE_PAGE_VPN_GET_PRIVATE (self);
	GError *vpn_error = NULL;

	if (error)
		return;

	g_return_if_fail (priv->plugin != NULL);

	priv->editor = nm_vpn_editor_plugin_get_editor (priv->plugin, parent->connection, &vpn_error);
	if (!priv->editor) {
		g_warning ("Could not load VPN user interface for service '%s': %s.",
		           priv->service_type,
		           (vpn_error && vpn_error->message) ? vpn_error->message : "(unknown)");
		g_error_free (vpn_error);
		return;
	}
	g_signal_connect (priv->editor, "changed", G_CALLBACK (vpn_plugin_changed_cb), self);

	parent->page = GTK_WIDGET (nm_vpn_editor_get_widget (priv->editor));
	if (!parent->page) {
		g_warning ("Could not load VPN user interface for service '%s'.", priv->service_type);
		return;
	}
	g_object_ref_sink (parent->page);
	gtk_widget_show_all (parent->page);
}

CEPage *
ce_page_vpn_new (NMConnectionEditor *editor,
                 NMConnection *connection,
                 GtkWindow *parent_window,
                 NMClient *client,
                 const char **out_secrets_setting_name,
                 GError **error)
{
	CEPageVpn *self;
	CEPageVpnPrivate *priv;
	const char *service_type;

	self = CE_PAGE_VPN (ce_page_new (CE_TYPE_PAGE_VPN,
	                                 editor,
	                                 connection,
	                                 parent_window,
	                                 client,
	                                 NULL,
	                                 NULL,
	                                 _("VPN")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load VPN user interface."));
		return NULL;
	}

	priv = CE_PAGE_VPN_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_vpn (connection);
	g_assert (priv->setting);

	service_type = nm_setting_vpn_get_service_type (priv->setting);
	g_assert (service_type);
	priv->service_type = g_strdup (service_type);

	priv->plugin = vpn_get_plugin_by_service (service_type);
	if (!priv->plugin) {
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not find VPN plugin service for '%s'."), service_type);
		g_object_unref (self);
		return NULL;
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	*out_secrets_setting_name = NM_SETTING_VPN_SETTING_NAME;

	return CE_PAGE (self);
}

gboolean
ce_page_vpn_can_export (CEPageVpn *page)
{
	CEPageVpnPrivate *priv = CE_PAGE_VPN_GET_PRIVATE (page);

	return 	(nm_vpn_editor_plugin_get_capabilities (priv->plugin) & NM_VPN_EDITOR_PLUGIN_CAPABILITY_EXPORT) != 0;
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageVpn *self = CE_PAGE_VPN (page);
	CEPageVpnPrivate *priv = CE_PAGE_VPN_GET_PRIVATE (self);

	return nm_vpn_editor_update_connection (priv->editor, connection, error);
}

static void
ce_page_vpn_init (CEPageVpn *self)
{
}

static void
dispose (GObject *object)
{
	CEPageVpnPrivate *priv = CE_PAGE_VPN_GET_PRIVATE (object);

	g_clear_object (&priv->editor);
	g_clear_pointer (&priv->service_type, g_free);

	G_OBJECT_CLASS (ce_page_vpn_parent_class)->dispose (object);
}

static void
ce_page_vpn_class_init (CEPageVpnClass *vpn_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (vpn_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (vpn_class);

	g_type_class_add_private (object_class, sizeof (CEPageVpnPrivate));

	/* virtual methods */
	object_class->dispose = dispose;

	parent_class->ce_page_validate_v = ce_page_validate_v;
}

typedef struct {
	NMClient *client;
	PageNewConnectionResultFunc result_func;
	gpointer user_data;
} NewVpnInfo;

static void
import_cb (NMConnection *connection, gpointer user_data)
{
	NewVpnInfo *info = (NewVpnInfo *) user_data;
	NMSettingConnection *s_con;
	NMSettingVpn *s_vpn;
	const char *service_type;
	char *s;
	GError *error = NULL;

	/* Basic sanity checks of the connection */
	s_con = nm_connection_get_setting_connection (connection);
	if (!s_con) {
		s_con = NM_SETTING_CONNECTION (nm_setting_connection_new ());
		nm_connection_add_setting (connection, NM_SETTING (s_con));
	}

	s = (char *) nm_setting_connection_get_id (s_con);
	if (!s) {
		const GPtrArray *connections;

		connections = nm_client_get_connections (info->client);
		s = ce_page_get_next_available_name (connections, _("VPN connection %d"));
		g_object_set (s_con, NM_SETTING_CONNECTION_ID, s, NULL);
		g_free (s);
	}

	s = (char *) nm_setting_connection_get_connection_type (s_con);
	if (!s || strcmp (s, NM_SETTING_VPN_SETTING_NAME))
		g_object_set (s_con, NM_SETTING_CONNECTION_TYPE, NM_SETTING_VPN_SETTING_NAME, NULL);

	s = (char *) nm_setting_connection_get_uuid (s_con);
	if (!s) {
		s = nm_utils_uuid_generate ();
		g_object_set (s_con, NM_SETTING_CONNECTION_UUID, s, NULL);
		g_free (s);
	}

	s_vpn = nm_connection_get_setting_vpn (connection);
	service_type = s_vpn ? nm_setting_vpn_get_service_type (s_vpn) : NULL;

	if (!service_type || !strlen (service_type)) {
		g_object_unref (connection);
		connection = NULL;

		error = g_error_new_literal (NMA_ERROR, NMA_ERROR_GENERIC,
		                             _("The VPN plugin failed to import the VPN connection correctly\n\nError: no VPN service type."));
	}

	info->result_func (connection, FALSE, error, info->user_data);
	g_clear_error (&error);
	g_object_unref (info->client);
	g_slice_free (NewVpnInfo, info);
}

void
vpn_connection_import (GtkWindow *parent,
                       const char *detail,
                       NMClient *client,
                       PageNewConnectionResultFunc result_func,
                       gpointer user_data)
{
	NewVpnInfo *info;

	info = g_slice_new (NewVpnInfo);
	info->result_func = result_func;
	info->client = g_object_ref (client);
	info->user_data = user_data;
	vpn_import (import_cb, info);
}

#define NEW_VPN_CONNECTION_PRIMARY_LABEL _("Choose a VPN Connection Type")
#define NEW_VPN_CONNECTION_SECONDARY_LABEL _("Select the type of VPN you wish to use for the new connection.  If the type of VPN connection you wish to create does not appear in the list, you may not have the correct VPN plugin installed.")

static gboolean
vpn_type_filter_func (GType type, gpointer user_data)
{
	return type == NM_TYPE_SETTING_VPN;
}

static void
vpn_type_result_func (NMConnection *connection, gpointer user_data)
{
	NewVpnInfo *info = user_data;

	info->result_func (connection, connection == NULL, NULL, info->user_data);
	g_slice_free (NewVpnInfo, info);
}

void
vpn_connection_new (GtkWindow *parent,
                    const char *detail,
                    NMClient *client,
                    PageNewConnectionResultFunc result_func,
                    gpointer user_data)
{
	NMConnection *connection;
	NMSetting *s_vpn;

	if (!detail) {
		NewVpnInfo *info;

		/* This will happen if nm-c-e is launched from the command line
		 * with "--create --type vpn". Dump the user back into the
		 * new connection dialog to let them pick a subtype now.
		 */
		info = g_slice_new (NewVpnInfo);
		info->result_func = result_func;
		info->user_data = user_data;
		new_connection_dialog_full (parent, client,
		                            NEW_VPN_CONNECTION_PRIMARY_LABEL,
		                            NEW_VPN_CONNECTION_SECONDARY_LABEL,
		                            vpn_type_filter_func,
		                            vpn_type_result_func, info);
		return;
	}

	connection = ce_page_new_connection (_("VPN connection %d"),
	                                     NM_SETTING_VPN_SETTING_NAME,
	                                     FALSE,
	                                     client,
	                                     user_data);
	s_vpn = nm_setting_vpn_new ();
	g_object_set (s_vpn, NM_SETTING_VPN_SERVICE_TYPE, detail, NULL);
	nm_connection_add_setting (connection, s_vpn);

	(*result_func) (connection, FALSE, NULL, user_data);
}


