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
 * Copyright 2017 Red Hat, Inc.
 */

#include "nm-default.h"

#include "connection-helpers.h"

#include "nm-connection-list.h"
#include "nm-connection-editor.h"
#include "page-ethernet.h"
#include "page-wifi.h"
#include "page-mobile.h"
#include "page-bluetooth.h"
#include "page-dsl.h"
#include "page-infiniband.h"
#include "page-ip-tunnel.h"
#include "page-macsec.h"
#include "page-bond.h"
#include "page-team.h"
#include "page-bridge.h"
#include "page-vlan.h"
#include "page-vpn.h"
#include "vpn-helpers.h"
#include "nm-utils/nm-vpn-editor-plugin-call.h"

#define COL_MARKUP     0
#define COL_SENSITIVE  1
#define COL_NEW_FUNC   2
#define COL_DESCRIPTION 3
#define COL_VPN_PLUGIN 4
#define COL_VPN_SERVICE_TYPE 5
#define COL_VPN_ADD_DETAIL_KEY 6
#define COL_VPN_ADD_DETAIL_VAL 7

static gint
sort_types (gconstpointer a, gconstpointer b)
{
	ConnectionTypeData *typea = (ConnectionTypeData *)a;
	ConnectionTypeData *typeb = (ConnectionTypeData *)b;

	if (typea->virtual && !typeb->virtual)
		return 1;
	else if (typeb->virtual && !typea->virtual)
		return -1;

	if (typea->setting_types[0] == NM_TYPE_SETTING_VPN &&
	    typeb->setting_types[0] != NM_TYPE_SETTING_VPN)
		return 1;
	else if (typeb->setting_types[0] == NM_TYPE_SETTING_VPN &&
	         typea->setting_types[0] != NM_TYPE_SETTING_VPN)
		return -1;

	return g_utf8_collate (typea->name, typeb->name);
}

#define add_type_data_full(a, n, new_func, type0, type1, type2, v) \
{ \
	ConnectionTypeData data; \
 \
	memset (&data, 0, sizeof (data)); \
	data.name = n; \
	data.new_connection_func = new_func; \
	data.setting_types[0] = type0; \
	data.setting_types[1] = type1; \
	data.setting_types[2] = type2; \
	data.setting_types[3] = G_TYPE_INVALID; \
	data.virtual = v; \
	g_array_append_val (a, data); \
}

#define add_type_data_real(a, n, new_func, type0) \
	add_type_data_full(a, n, new_func, type0, G_TYPE_INVALID, G_TYPE_INVALID, FALSE)

#define add_type_data_virtual(a, n, new_func, type0) \
	add_type_data_full(a, n, new_func, type0, G_TYPE_INVALID, G_TYPE_INVALID, TRUE)

ConnectionTypeData *
get_connection_type_list (void)
{
	GArray *array;
	static ConnectionTypeData *list;

	if (list)
		return list;

	array = g_array_new (TRUE, FALSE, sizeof (ConnectionTypeData));

	add_type_data_real (array, _("Ethernet"), ethernet_connection_new, NM_TYPE_SETTING_WIRED);
	add_type_data_real (array, _("Wi-Fi"), wifi_connection_new, NM_TYPE_SETTING_WIRELESS);
	add_type_data_full (array,
	                    _("Mobile Broadband"),
	                    mobile_connection_new,
	                    NM_TYPE_SETTING_GSM,
	                    NM_TYPE_SETTING_CDMA,
	                    NM_TYPE_SETTING_BLUETOOTH,
	                    FALSE);
	add_type_data_real (array, _("Bluetooth"), bluetooth_connection_new, NM_TYPE_SETTING_BLUETOOTH);
	add_type_data_real (array, _("DSL/PPPoE"), dsl_connection_new, NM_TYPE_SETTING_PPPOE);
	add_type_data_real (array, _("InfiniBand"), infiniband_connection_new, NM_TYPE_SETTING_INFINIBAND);
	add_type_data_virtual (array, _("Bond"), bond_connection_new, NM_TYPE_SETTING_BOND);
	add_type_data_virtual (array, _("Team"), team_connection_new, NM_TYPE_SETTING_TEAM);
	add_type_data_virtual (array, _("Bridge"), bridge_connection_new, NM_TYPE_SETTING_BRIDGE);
	add_type_data_virtual (array, _("VLAN"), vlan_connection_new, NM_TYPE_SETTING_VLAN);
	add_type_data_virtual (array, _("IP tunnel"), ip_tunnel_connection_new, NM_TYPE_SETTING_IP_TUNNEL);
	add_type_data_virtual (array, _("MACsec"), macsec_connection_new, NM_TYPE_SETTING_MACSEC);

	add_type_data_virtual (array, _("VPN"), vpn_connection_new, NM_TYPE_SETTING_VPN);

	g_array_sort (array, sort_types);

	return (ConnectionTypeData *)g_array_free (array, FALSE);
}

static gboolean
combo_row_separator_func (GtkTreeModel *model,
                          GtkTreeIter  *iter,
                          gpointer      data)
{
	char *label;

	gtk_tree_model_get (model, iter,
	                    COL_MARKUP, &label,
	                    -1);
	if (label) {
		g_free (label);
		return FALSE;
	} else
		return TRUE;
}

static void
combo_changed_cb (GtkComboBox *combo, gpointer user_data)
{
	GtkLabel *label = GTK_LABEL (user_data);
	GtkTreeModel *model;
	GtkTreeIter iter;
	gs_free char *description = NULL;

	if (!gtk_combo_box_get_active_iter (combo, &iter))
		goto no_description;
	model = gtk_combo_box_get_model (combo);
	if (!model)
		goto no_description;

	gtk_tree_model_get (model, &iter,
	                    COL_DESCRIPTION, &description,
	                    -1);
	if (description) {
		gs_free char *markup = NULL;

		markup = g_markup_printf_escaped ("<i>%s</i>", description);
		gtk_label_set_markup (label, markup);
		return;
	}

no_description:
	gtk_label_set_text (label, "");
}

NMConnection *
vpn_connection_from_file (const char *filename)
{
	NMConnection *connection = NULL;
	GError *error = NULL;
	GSList *iter;

	for (iter = vpn_get_plugin_infos (); !connection && iter; iter = iter->next) {
		NMVpnEditorPlugin *plugin;

		plugin = nm_vpn_plugin_info_get_editor_plugin (iter->data);
		g_clear_error (&error);
		connection = nm_vpn_editor_plugin_import (plugin, filename, &error);
		if (connection)
			break;
	}

	if (connection) {
		NMSettingVpn *s_vpn;
		const char *service_type;

		s_vpn = nm_connection_get_setting_vpn (connection);
		service_type = s_vpn ? nm_setting_vpn_get_service_type (s_vpn) : NULL;

		/* Check connection sanity. */
		if (!service_type || !strlen (service_type)) {
			g_object_unref (connection);
			connection = NULL;

			error = g_error_new_literal (NMA_ERROR, NMA_ERROR_GENERIC,
			                             _("The VPN plugin failed to import the VPN connection correctly\n\nError: no VPN service type."));
		}
	}

	if (!connection) {
		GtkWidget *err_dialog;
		char *bname = g_path_get_basename (filename);

		err_dialog = gtk_message_dialog_new (NULL,
		                                     GTK_DIALOG_DESTROY_WITH_PARENT,
		                                     GTK_MESSAGE_ERROR,
		                                     GTK_BUTTONS_OK,
		                                     _("Cannot import VPN connection"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (err_dialog),
		                                 _("The file “%s” could not be read or does not contain recognized VPN connection information\n\nError: %s."),
		                                 bname, error ? error->message : _("unknown error"));
		g_free (bname);
		g_signal_connect (err_dialog, "delete-event", G_CALLBACK (gtk_widget_destroy), NULL);
		g_signal_connect (err_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_widget_show_all (err_dialog);
		gtk_window_present (GTK_WINDOW (err_dialog));
	}

	g_clear_error (&error);

	return connection;
}

typedef struct {
	GtkWindow *parent;
	NMClient *client;
	PageNewConnectionResultFunc result_func;
	gpointer user_data;
} ImportVpnInfo;

static void
import_vpn_from_file_cb (GtkWidget *dialog, gint response, gpointer user_data)
{
	char *filename = NULL;
	ImportVpnInfo *info = (ImportVpnInfo *) user_data;
	NMConnection *connection = NULL;

	if (response != GTK_RESPONSE_ACCEPT)
		goto out;

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	if (!filename) {
		g_warning ("%s: didn't get a filename back from the chooser!", __func__);
		goto out;
	}

	connection = vpn_connection_from_file (filename);
	if (connection) {
		/* Wrap around the actual new function so that the page can complete
		 * the missing parts, such as UUID or make up the connection name. */
		vpn_connection_new (FUNC_TAG_PAGE_NEW_CONNECTION_CALL,
		                    info->parent,
		                    NULL,
		                    NULL,
		                    connection,
		                    info->client,
		                    info->result_func,
		                    info->user_data);
	}

	g_free (filename);
out:
	gtk_widget_hide (dialog);
	gtk_widget_destroy (dialog);
	g_object_unref (info->parent);
	g_object_unref (info->client);
	g_slice_free (ImportVpnInfo, info);
}

static void
vpn_connection_import (FUNC_TAG_PAGE_NEW_CONNECTION_IMPL,
                       GtkWindow *parent,
                       const char *detail,
                       gpointer detail_data,
                       NMConnection *connection,
                       NMClient *client,
                       PageNewConnectionResultFunc result_func,
                       gpointer user_data)
{
	ImportVpnInfo *info;
	GtkWidget *dialog;
	const char *home_folder;

	/* The import function decides about the type. */
	g_return_if_fail (!detail);
	g_warn_if_fail (!connection);

	info = g_slice_new (ImportVpnInfo);
	info->parent = g_object_ref (parent);
	info->result_func = result_func;
	info->client = g_object_ref (client);
	info->user_data = user_data;

	dialog = gtk_file_chooser_dialog_new (_("Select file to import"),
	                                      NULL,
	                                      GTK_FILE_CHOOSER_ACTION_OPEN,
	                                      _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                      _("_Open"), GTK_RESPONSE_ACCEPT,
	                                      NULL);
	home_folder = g_get_home_dir ();
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), home_folder);

	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (import_vpn_from_file_cb), info);
	gtk_widget_show_all (dialog);
	gtk_window_present (GTK_WINDOW (dialog));
}

static void
set_up_connection_type_combo (GtkComboBox *combo,
                              GtkLabel *description_label,
                              NewConnectionTypeFilterFunc type_filter_func,
                              gpointer user_data)
{
	GtkListStore *model = GTK_LIST_STORE (gtk_combo_box_get_model (combo));
	ConnectionTypeData *list = get_connection_type_list ();
	GtkTreeIter iter;
	GSList *p;
	int i, vpn_index = -1, active = 0, added = 0;
	gboolean import_supported = FALSE;
	gboolean added_virtual_header = FALSE;
	gboolean show_headers = (type_filter_func == NULL);
	char *markup;
	GSList *vpn_plugins;

	gtk_combo_box_set_row_separator_func (combo, combo_row_separator_func, NULL, NULL);
	g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (combo_changed_cb), description_label);

	if (show_headers) {
		markup = g_strdup_printf ("<b><big>%s</big></b>", _("Hardware"));
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
		                    COL_MARKUP, markup,
		                    COL_SENSITIVE, FALSE,
		                    -1);
		g_free (markup);
	}

	for (i = 0; list[i].name; i++) {
		if (type_filter_func) {
			if (   (   list[i].setting_types[0] == G_TYPE_INVALID
			        || !type_filter_func (FUNC_TAG_NEW_CONNECTION_TYPE_FILTER_CALL, list[i].setting_types[0], user_data))
			    && (   list[i].setting_types[1] == G_TYPE_INVALID
			        || !type_filter_func (FUNC_TAG_NEW_CONNECTION_TYPE_FILTER_CALL, list[i].setting_types[1], user_data))
			    && (   list[i].setting_types[2] == G_TYPE_INVALID
			        || !type_filter_func (FUNC_TAG_NEW_CONNECTION_TYPE_FILTER_CALL, list[i].setting_types[2], user_data)))
				continue;
		}

		if (list[i].setting_types[0] == NM_TYPE_SETTING_VPN) {
			vpn_index = i;
			continue;
		} else if (list[i].setting_types[0] == NM_TYPE_SETTING_WIRED)
			active = added;

		if (list[i].virtual && !added_virtual_header && show_headers) {
			markup = g_strdup_printf ("<b><big>%s</big></b>", _("Virtual"));
			gtk_list_store_append (model, &iter);
			gtk_list_store_set (model, &iter,
			                    COL_MARKUP, markup,
			                    COL_SENSITIVE, FALSE,
			                    -1);
			g_free (markup);
			added_virtual_header = TRUE;
		}

		if (show_headers)
			markup = g_markup_printf_escaped ("    %s", list[i].name);
		else
			markup = g_markup_escape_text (list[i].name, -1);
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
		                    COL_MARKUP, markup,
		                    COL_SENSITIVE, TRUE,
		                    COL_NEW_FUNC, list[i].new_connection_func,
		                    -1);
		g_free (markup);
		added++;
	}

	vpn_plugins = vpn_get_plugin_infos ();
	if (!vpn_plugins || vpn_index == -1) {
		gtk_combo_box_set_active (combo, show_headers ? active + 1 : active);
		return;
	}

	if (show_headers) {
		markup = g_strdup_printf ("<b><big>%s</big></b>", _("VPN"));
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
		                    COL_MARKUP, markup,
		                    COL_SENSITIVE, FALSE,
		                    -1);
		g_free (markup);
	}

	for (p = vpn_plugins; p; p = p->next) {
		NMVpnPluginInfo *plugin_info = p->data;
		NMVpnEditorPlugin *plugin;
		const char *const*aliases;
		const char *service_type;
		gboolean is_alias = FALSE;

		plugin = nm_vpn_plugin_info_get_editor_plugin (plugin_info);
		if (!plugin)
			continue;

		service_type = nm_vpn_plugin_info_get_service (plugin_info);
		aliases = nm_vpn_plugin_info_get_aliases (plugin_info);

		for (;;) {
			gs_free char *pretty_name = NULL;
			gs_free char *description = NULL;
			NMVpnEditorPluginServiceFlags flags;
			gs_strfreev char **add_details_free = NULL;
			char **add_details;
			const char *i_add_detail;

			if (!nm_vpn_editor_plugin_get_service_info (plugin, service_type, NULL, &pretty_name, &description, &flags)) {
				if (is_alias)
					goto next;
				g_object_get (plugin,
				              NM_VPN_EDITOR_PLUGIN_NAME, &pretty_name,
				              NM_VPN_EDITOR_PLUGIN_DESCRIPTION, &description,
				              NULL);
				flags = NM_VPN_EDITOR_PLUGIN_SERVICE_FLAGS_CAN_ADD;
			}
			if (!pretty_name)
				goto next;
			if (!NM_FLAGS_HAS (flags, NM_VPN_EDITOR_PLUGIN_SERVICE_FLAGS_CAN_ADD))
				goto next;

			add_details_free = nm_vpn_editor_plugin_get_service_add_details (plugin, service_type);
			add_details = add_details_free;
			i_add_detail = add_details ? add_details[0] : NULL;
			do {
				const char *i_pretty_name, *i_description;
				gs_free char *i_pretty_name_free = NULL;
				gs_free char *i_description_free = NULL;
				gs_free char *i_add_detail_key = NULL;
				gs_free char *i_add_detail_val = NULL;

				if (i_add_detail) {
					if (i_add_detail[0] == '\0')
						goto i_next;
					if (!nm_vpn_editor_plugin_get_service_add_detail (plugin, service_type, i_add_detail,
					                                                  &i_pretty_name_free, &i_description_free,
					                                                  &i_add_detail_key, &i_add_detail_val, NULL))
						goto i_next;
					if (!i_pretty_name_free)
						goto i_next;
					if (i_add_detail_key && !i_add_detail_key[0])
						goto i_next;
					if (i_add_detail_val && !i_add_detail_val[0])
						goto next;
					if (!i_add_detail_key ^ !i_add_detail_val)
						goto next;
					i_pretty_name = i_pretty_name_free;
					i_description = i_description_free;
				} else {
					i_pretty_name = pretty_name;
					i_description = description;
				}

				if (show_headers)
					markup = g_markup_printf_escaped ("    %s", i_pretty_name);
				else
					markup = g_markup_escape_text (i_pretty_name, -1);

				gtk_list_store_append (model, &iter);
				gtk_list_store_set (model, &iter,
				                    COL_MARKUP, markup,
				                    COL_SENSITIVE, TRUE,
				                    COL_NEW_FUNC, list[vpn_index].new_connection_func,
				                    COL_DESCRIPTION, i_description,
				                    COL_VPN_PLUGIN, plugin,
				                    COL_VPN_SERVICE_TYPE, service_type,
				                    COL_VPN_ADD_DETAIL_KEY, i_add_detail_key,
				                    COL_VPN_ADD_DETAIL_VAL, i_add_detail_val,
				                    -1);
				g_free (markup);

i_next:
				if (!i_add_detail)
					break;
				i_add_detail = (++add_details)[0];
			} while (i_add_detail);

next:
			if (!aliases || !aliases[0])
				break;
			is_alias = TRUE;
			service_type = aliases[0];
			aliases++;
		}

		if (nm_vpn_editor_plugin_get_capabilities (plugin) & NM_VPN_EDITOR_PLUGIN_CAPABILITY_IMPORT)
			import_supported = TRUE;
	}

	if (import_supported) {
		/* Separator */
		gtk_list_store_append (model, &iter);

		if (show_headers)
			markup = g_strdup_printf ("    %s", _("Import a saved VPN configuration…"));
		else
			markup = g_strdup (_("Import a saved VPN configuration…"));
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
		                    COL_MARKUP, markup,
		                    COL_SENSITIVE, TRUE,
		                    COL_NEW_FUNC, vpn_connection_import,
		                    -1);
		g_free (markup);
	}

	gtk_combo_box_set_active (combo, show_headers ? active + 1 : active);
}

typedef struct {
	GtkWindow *parent_window;
	NMClient *client;
	NewConnectionResultFunc result_func;
	gpointer user_data;
} NewConnectionData;

static void
new_connection_result (FUNC_TAG_PAGE_NEW_CONNECTION_RESULT_IMPL,
                       NMConnection *connection, /* allow-none, don't transfer reference, allow-keep */
                       gboolean canceled,
                       GError *error,
                       gpointer user_data)
{
	NewConnectionData *ncd = user_data;
	NewConnectionResultFunc result_func;
	GtkWindow *parent_window;
	const char *default_message = _("The connection editor dialog could not be initialized due to an unknown error.");

	result_func = ncd->result_func;
	user_data = ncd->user_data;
	parent_window = ncd->parent_window;
	g_slice_free (NewConnectionData, ncd);

	if (!connection && !canceled) {
		nm_connection_editor_error (parent_window,
		                            _("Could not create new connection"),
		                            "%s",
		                            (error && error->message) ? error->message : default_message);
	}

	result_func (FUNC_TAG_NEW_CONNECTION_RESULT_CALL, connection, user_data);
}

void
new_connection_of_type (GtkWindow *parent_window,
                        const char *detail,
                        gpointer detail_data,
                        NMConnection *connection,
                        NMClient *client,
                        PageNewConnectionFunc new_func,
                        NewConnectionResultFunc result_func,
                        gpointer user_data)
{
	NewConnectionData *ncd;

	ncd = g_slice_new (NewConnectionData);
	ncd->parent_window = parent_window;
	ncd->client = client;
	ncd->result_func = result_func;
	ncd->user_data = user_data;

	new_func (FUNC_TAG_PAGE_NEW_CONNECTION_CALL,
	          parent_window,
	          detail,
	          detail_data,
	          connection,
	          client,
	          new_connection_result,
	          ncd);
}

void
new_connection_dialog (GtkWindow *parent_window,
                       NMClient *client,
                       NewConnectionTypeFilterFunc type_filter_func,
                       NewConnectionResultFunc result_func,
                       gpointer user_data)
{
	new_connection_dialog_full (parent_window, client,
	                            NULL, NULL,
	                            type_filter_func,
	                            result_func,
	                            user_data);
}

void
new_connection_dialog_full (GtkWindow *parent_window,
                            NMClient *client,
                            const char *primary_label,
                            const char *secondary_label,
                            NewConnectionTypeFilterFunc type_filter_func,
                            NewConnectionResultFunc result_func,
                            gpointer user_data)
{

	GtkBuilder *gui;
	GtkDialog *type_dialog;
	GtkComboBox *combo;
	GtkLabel *label;
	GtkTreeIter iter;
	int response;
	PageNewConnectionFunc new_func = NULL;
	gs_free char *vpn_service_type = NULL;
	gs_free char *vpn_add_detail_key = NULL;
	gs_free char *vpn_add_detail_val = NULL;
	const char *detail = NULL;
	gpointer detail_data = NULL;
	GError *error = NULL;
	CEPageVpnDetailData vpn_data;
	GtkButton *create_button;

	/* load GUI */
	gui = gtk_builder_new ();
	if (!gtk_builder_add_from_resource (gui,
	                                    "/org/freedesktop/network-manager-applet/ce-new-connection.ui",
	                                    &error)) {
		g_warning ("Couldn't load builder resource: %s", error->message);
		g_error_free (error);
		g_object_unref (gui);
		return;
	}

	type_dialog = GTK_DIALOG (gtk_builder_get_object (gui, "new_connection_type_dialog"));
	gtk_window_set_transient_for (GTK_WINDOW (type_dialog), parent_window);

	combo = GTK_COMBO_BOX (gtk_builder_get_object (gui, "new_connection_type_combo"));
	label = GTK_LABEL (gtk_builder_get_object (gui, "new_connection_desc_label"));
	create_button = GTK_BUTTON (gtk_builder_get_object (gui, "create_button"));
	set_up_connection_type_combo (combo, label, type_filter_func, user_data);

	/* Disable "Create" button if no item is available */
	if (!gtk_tree_model_iter_n_children (gtk_combo_box_get_model (combo), NULL))
		gtk_widget_set_sensitive (GTK_WIDGET (create_button), FALSE);

	if (primary_label) {
		label = GTK_LABEL (gtk_builder_get_object (gui, "new_connection_primary_label"));
		gtk_label_set_text (label, primary_label);
	}
	if (secondary_label) {
		label = GTK_LABEL (gtk_builder_get_object (gui, "new_connection_secondary_label"));
		gtk_label_set_text (label, secondary_label);
	}

	response = gtk_dialog_run (type_dialog);
	if (response == GTK_RESPONSE_OK) {
		if (gtk_combo_box_get_active_iter (combo, &iter)) {
			gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter,
			                    COL_NEW_FUNC, &new_func,
			                    COL_VPN_SERVICE_TYPE, &vpn_service_type,
			                    COL_VPN_ADD_DETAIL_KEY, &vpn_add_detail_key,
			                    COL_VPN_ADD_DETAIL_VAL, &vpn_add_detail_val,
			                    -1);
			if (vpn_service_type) {
				memset (&vpn_data, 0, sizeof (vpn_data));
				vpn_data.add_detail_key = vpn_add_detail_key;
				vpn_data.add_detail_val = vpn_add_detail_val;

				detail = vpn_service_type;
				detail_data = &vpn_data;
			}
		}
	}

	gtk_widget_destroy (GTK_WIDGET (type_dialog));
	g_object_unref (gui);

	if (new_func) {
		new_connection_of_type (parent_window,
		                        detail,
		                        detail_data,
		                        NULL,
		                        client,
		                        new_func,
		                        result_func,
		                        user_data);
	} else
		result_func (FUNC_TAG_NEW_CONNECTION_RESULT_CALL, NULL, user_data);
}

typedef struct {
	GtkWindow *parent_window;
	NMConnectionEditor *editor;
	DeleteConnectionResultFunc result_func;
	gpointer user_data;
} DeleteInfo;

static void
delete_cb (GObject *connection,
           GAsyncResult *result,
           gpointer user_data)
{
	DeleteInfo *info = user_data;
	DeleteConnectionResultFunc result_func;
	GError *error = NULL;

	nm_remote_connection_delete_finish (NM_REMOTE_CONNECTION (connection), result, &error);
	if (error) {
		nm_connection_editor_error (info->parent_window,
		                            _("Connection delete failed"),
		                            "%s", error->message);
	}

	if (info->editor) {
		nm_connection_editor_set_busy (info->editor, FALSE);
		g_object_unref (info->editor);
	}
	if (info->parent_window)
		g_object_unref (info->parent_window);

	result_func = info->result_func;
	user_data = info->user_data;
	g_free (info);
	g_clear_error (&error);

	if (result_func)
		(*result_func) (FUNC_TAG_DELETE_CONNECTION_RESULT_CALL, NM_REMOTE_CONNECTION (connection), error == NULL, user_data);
}

void
delete_connection (GtkWindow *parent_window,
                   NMRemoteConnection *connection,
                   DeleteConnectionResultFunc result_func,
                   gpointer user_data)
{
	NMConnectionEditor *editor;
	NMSettingConnection *s_con;
	GtkWidget *dialog;
	const char *id;
	guint result;
	DeleteInfo *info;

	editor = nm_connection_editor_get (NM_CONNECTION (connection));
	if (editor && nm_connection_editor_get_busy (editor)) {
		/* Editor already has an operation in progress, raise it */
		nm_connection_editor_present (editor);
		return;
	}

	s_con = nm_connection_get_setting_connection (NM_CONNECTION (connection));
	g_assert (s_con);
	id = nm_setting_connection_get_id (s_con);

	dialog = gtk_message_dialog_new (parent_window,
	                                 GTK_DIALOG_DESTROY_WITH_PARENT,
	                                 GTK_MESSAGE_QUESTION,
	                                 GTK_BUTTONS_NONE,
	                                 _("Are you sure you wish to delete the connection %s?"),
	                                 id);
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
	                        _("_Cancel"), GTK_RESPONSE_CANCEL,
	                        _("_Delete"), GTK_RESPONSE_YES,
	                        NULL);

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (result != GTK_RESPONSE_YES)
		return;

	info = g_malloc0 (sizeof (DeleteInfo));
	info->editor = editor ? g_object_ref (editor) : NULL;
	info->parent_window = parent_window ? g_object_ref (parent_window) : NULL;
	info->result_func = result_func;
	info->user_data = user_data;

	if (editor)
		nm_connection_editor_set_busy (editor, TRUE);

	nm_remote_connection_delete_async (connection, NULL, delete_cb, info);
}

gboolean
connection_supports_proxy (NMConnection *connection)
{
	NMSettingConnection *s_con;

	g_return_val_if_fail (NM_IS_CONNECTION (connection), FALSE);

	s_con = nm_connection_get_setting_connection (connection);
	return (nm_setting_connection_get_slave_type (s_con) == NULL);
}

gboolean
connection_supports_ip4 (NMConnection *connection)
{
	NMSettingConnection *s_con;

	g_return_val_if_fail (NM_IS_CONNECTION (connection), FALSE);

	s_con = nm_connection_get_setting_connection (connection);
	return (nm_setting_connection_get_slave_type (s_con) == NULL);
}

gboolean
connection_supports_ip6 (NMConnection *connection)
{
	NMSettingConnection *s_con;
	const char *connection_type;

	g_return_val_if_fail (NM_IS_CONNECTION (connection), FALSE);

	s_con = nm_connection_get_setting_connection (connection);
	if (nm_setting_connection_get_slave_type (s_con) != NULL)
		return FALSE;

	connection_type = nm_setting_connection_get_connection_type (s_con);
	if (!strcmp (connection_type, NM_SETTING_VPN_SETTING_NAME))
		return vpn_supports_ipv6 (connection);
	else if (!strcmp (connection_type, NM_SETTING_PPPOE_SETTING_NAME))
		return FALSE;
	else
		return TRUE;
}
