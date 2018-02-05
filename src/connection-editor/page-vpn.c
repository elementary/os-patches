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

#include "page-vpn.h"

#include <string.h>

#include "connection-helpers.h"
#include "nm-connection-editor.h"
#include "vpn-helpers.h"
#include "nm-utils/nm-vpn-editor-plugin-call.h"

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
	GError *local = NULL;

	if (error)
		return;

	g_return_if_fail (NM_IS_VPN_EDITOR_PLUGIN (priv->plugin));

	priv->editor = nm_vpn_editor_plugin_get_editor (priv->plugin, CE_PAGE (self)->connection, &local);
	if (!priv->editor) {
		g_warning (_("Could not load editor VPN plugin for “%s” (%s)."),
		           priv->service_type, local ? local->message : _("unknown failure"));
		g_clear_error (&local);
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
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not find VPN plugin for “%s”."), service_type);
		g_object_unref (self);
		return NULL;
	}
	priv->plugin = g_object_ref (priv->plugin);

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	*out_secrets_setting_name = NM_SETTING_VPN_SETTING_NAME;

	return CE_PAGE (self);
}

gboolean
ce_page_vpn_can_export (CEPageVpn *page)
{
	CEPageVpnPrivate *priv = CE_PAGE_VPN_GET_PRIVATE (page);

	return (nm_vpn_editor_plugin_get_capabilities (priv->plugin) & NM_VPN_EDITOR_PLUGIN_CAPABILITY_EXPORT) != 0;
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

	if (priv->editor) {
		g_signal_handlers_disconnect_by_func (priv->editor, G_CALLBACK (vpn_plugin_changed_cb), object);
		g_clear_object (&priv->editor);
	}
	g_clear_pointer (&priv->service_type, g_free);

	g_clear_object (&priv->plugin);

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

typedef void (*VpnImportSuccessCallback) (NMConnection *connection, gpointer user_data);

typedef struct {
	VpnImportSuccessCallback callback;
	gpointer user_data;
} ActionInfo;

static void
complete_vpn_connection (NMConnection *connection, NMClient *client)
{
	ce_page_complete_connection (connection,
	                             _("VPN connection %d"),
	                             NM_SETTING_VPN_SETTING_NAME,
	                             FALSE,
	                             client);
}

#define NEW_VPN_CONNECTION_PRIMARY_LABEL _("Choose a VPN Connection Type")
#define NEW_VPN_CONNECTION_SECONDARY_LABEL _("Select the type of VPN you wish to use for the new connection. If the type of VPN connection you wish to create does not appear in the list, you may not have the correct VPN plugin installed.")

static gboolean
vpn_type_filter_func (FUNC_TAG_NEW_CONNECTION_TYPE_FILTER_IMPL,
                      GType type,
                      gpointer user_data)
{
	return type == NM_TYPE_SETTING_VPN;
}

static void
vpn_type_result_func (FUNC_TAG_NEW_CONNECTION_RESULT_IMPL,
                      NMConnection *connection,
                      gpointer user_data)
{
	NewVpnInfo *info = user_data;

	info->result_func (FUNC_TAG_PAGE_NEW_CONNECTION_RESULT_CALL, connection, connection == NULL, NULL, info->user_data);
	g_slice_free (NewVpnInfo, info);
}

void
vpn_connection_new (FUNC_TAG_PAGE_NEW_CONNECTION_IMPL,
                    GtkWindow *parent,
                    const char *detail,
                    gpointer detail_data,
                    NMConnection *connection,
                    NMClient *client,
                    PageNewConnectionResultFunc result_func,
                    gpointer user_data)
{
	NMSetting *s_vpn;
	const char *service_type;
	gs_free char *service_type_free = NULL;
	gs_free char *add_detail_key_free = NULL;
	gs_free char *add_detail_val_free = NULL;
	const CEPageVpnDetailData *vpn_data = detail_data;
	gssize split_idx, l;
	const char *add_detail_key = NULL;
	const char *add_detail_val = NULL;
	gs_unref_object NMConnection *connection_tmp = NULL;

	if (!detail && !connection) {
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

	connection = _ensure_connection_other (connection, &connection_tmp);
	if (detail) {
		service_type = detail;
		add_detail_key = vpn_data ? vpn_data->add_detail_key : NULL;
		add_detail_val = vpn_data ? vpn_data->add_detail_val : NULL;

		service_type_free = nm_vpn_plugin_info_list_find_service_type (vpn_get_plugin_infos (), detail);
		if (service_type_free)
			service_type = service_type_free;
		else if (!vpn_data) {
			/* when called without @vpn_data, it means that @detail may contain "<SERVICE_TYPE>:<ADD_DETAIL>".
			 * Try to parse them by spliting @detail at the colons and try to interpret the first part as
			 * @service_type and the remainder as add-detail. */
			l = strlen (detail);
			for (split_idx = 1; split_idx < l - 1; split_idx++) {
				if (detail[split_idx] == ':') {
					gs_free char *detail_main = g_strndup (detail, split_idx);
					NMVpnEditorPlugin *plugin;

					service_type_free = nm_vpn_plugin_info_list_find_service_type (vpn_get_plugin_infos (), detail_main);
					if (!service_type_free)
						continue;
					plugin = vpn_get_plugin_by_service (service_type_free);
					if (!plugin) {
						g_clear_pointer (&service_type_free, g_free);
						continue;
					}

					/* we found a @service_type. Try to use the remainder as add-detail. */
					service_type = service_type_free;
					if (nm_vpn_editor_plugin_get_service_add_detail (plugin, service_type, &detail[split_idx + 1],
					                                                 NULL, NULL,
					                                                 &add_detail_key_free, &add_detail_val_free, NULL)
					    && add_detail_key_free && add_detail_key_free[0]
					    && add_detail_val_free && add_detail_val_free[0]) {
						add_detail_key = add_detail_key_free;
						add_detail_val = add_detail_val_free;
					}
					break;
				}
			}
		}
		if (!service_type)
			service_type = detail;

		s_vpn = nm_setting_vpn_new ();
		g_object_set (s_vpn, NM_SETTING_VPN_SERVICE_TYPE, service_type, NULL);

		if (add_detail_key)
			nm_setting_vpn_add_data_item ((NMSettingVpn *) s_vpn, add_detail_key, add_detail_val);

		nm_connection_add_setting (connection, s_vpn);
	}

	complete_vpn_connection (connection, client);

	(*result_func) (FUNC_TAG_PAGE_NEW_CONNECTION_RESULT_CALL, connection, FALSE, NULL, user_data);
}
