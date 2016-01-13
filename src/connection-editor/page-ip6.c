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
 * (C) Copyright 2008 - 2012 Red Hat, Inc.
 */

#include "config.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <nm-setting-connection.h>
#include <nm-setting-ip6-config.h>
#include <nm-setting-wired.h>
#include <nm-setting-wireless.h>
#include <nm-setting-gsm.h>
#include <nm-setting-cdma.h>
#include <nm-setting-pppoe.h>
#include <nm-setting-vpn.h>
#include <nm-utils.h>

#include "page-ip6.h"
#include "ip6-routes-dialog.h"
#include "nm-glib-compat.h"

G_DEFINE_TYPE (CEPageIP6, ce_page_ip6, CE_TYPE_PAGE)

#define CE_PAGE_IP6_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_IP6, CEPageIP6Private))

#define COL_ADDRESS 0
#define COL_PREFIX 1
#define COL_GATEWAY 2
#define COL_LAST COL_GATEWAY

typedef struct {
	NMSettingIP6Config *setting;
	char *connection_id;
	GType connection_type;

	GtkComboBox *method;
	GtkListStore *method_store;
	int normal_method_idx;
	int hotspot_method_idx;

	/* Addresses */
	GtkWidget *addr_label;
	GtkButton *addr_add;
	GtkButton *addr_delete;
	GtkTreeView *addr_list;
	GtkCellRenderer *addr_cells[COL_LAST + 1];

	/* DNS servers */
	GtkWidget *dns_servers_label;
	GtkEntry *dns_servers;

	/* Search domains */
	GtkWidget *dns_searches_label;
	GtkEntry *dns_searches;

	/* Routes */
	GtkButton *routes_button;

	/* IPv6 privacy extensions combo */
	GtkWidget *ip6_privacy_label;
	GtkComboBox *ip6_privacy_combo;

	/* IPv6 required */
	GtkCheckButton *ip6_required;

	GtkWindowGroup *window_group;
	gboolean window_added;

	/* Cached tree view entry for editing-canceled */
	/* Used also for saving old value when switching between cells via mouse
	 * clicks - GTK3 produces neither editing-canceled nor editing-done for
	 * that :( */
	char *last_edited; /* cell text */
	char *last_path;   /* row in treeview */
	int last_column;   /* column in treeview */
} CEPageIP6Private;

#define METHOD_COL_NAME 0
#define METHOD_COL_NUM  1
#define METHOD_COL_ENABLED 2

#define IP6_METHOD_IGNORE          0
#define IP6_METHOD_AUTO            1
#define IP6_METHOD_AUTO_ADDRESSES  2
#define IP6_METHOD_AUTO_DHCP_ONLY  3
#define IP6_METHOD_MANUAL          4
#define IP6_METHOD_LINK_LOCAL      5
#define IP6_METHOD_SHARED          6

#define IP6_PRIVACY_DISABLED       0
#define IP6_PRIVACY_PREFER_PUBLIC  1
#define IP6_PRIVACY_PREFER_TEMP    2

static void
ip6_private_init (CEPageIP6 *self, NMConnection *connection)
{
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	GtkBuilder *builder;
	GtkTreeIter iter;
	NMSettingConnection *s_con;
	const char *connection_type;
	char *str_auto = NULL, *str_auto_only = NULL;
	GList *cells;

	builder = CE_PAGE (self)->builder;

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);
	connection_type = nm_setting_connection_get_connection_type (s_con);
	g_assert (connection_type);

	priv->connection_type = nm_connection_lookup_setting_type (connection_type);

	if (priv->connection_type == NM_TYPE_SETTING_VPN) {
		str_auto = _("Automatic (VPN)");
		str_auto_only = _("Automatic (VPN) addresses only");
	} else if (priv->connection_type == NM_TYPE_SETTING_PPPOE) {
		str_auto = _("Automatic (PPPoE)");
		str_auto_only = _("Automatic (PPPoE) addresses only");
	} else {
		str_auto = _("Automatic");
		str_auto_only = _("Automatic, addresses only");
	}

	priv->method = GTK_COMBO_BOX (gtk_builder_get_object (builder, "ip6_method"));
	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (priv->method));
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->method), cells->data,
	                               "sensitive", METHOD_COL_ENABLED);

	priv->method_store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_BOOLEAN);

	gtk_list_store_append (priv->method_store, &iter);
	gtk_list_store_set (priv->method_store, &iter,
	                    METHOD_COL_NAME, _("Ignore"),
	                    METHOD_COL_NUM, IP6_METHOD_IGNORE,
	                    METHOD_COL_ENABLED, TRUE,
	                    -1);

	gtk_list_store_append (priv->method_store, &iter);
	gtk_list_store_set (priv->method_store, &iter,
	                    METHOD_COL_NAME, str_auto,
	                    METHOD_COL_NUM, IP6_METHOD_AUTO,
	                    METHOD_COL_ENABLED, TRUE,
	                    -1);

	gtk_list_store_append (priv->method_store, &iter);
	gtk_list_store_set (priv->method_store, &iter,
	                    METHOD_COL_NAME, str_auto_only,
	                    METHOD_COL_NUM, IP6_METHOD_AUTO_ADDRESSES,
	                    METHOD_COL_ENABLED, TRUE,
	                    -1);

	/* DHCP only used on Wi-Fi and ethernet for now */
	if (   priv->connection_type == NM_TYPE_SETTING_WIRED
	    || priv->connection_type == NM_TYPE_SETTING_WIRELESS) {
		gtk_list_store_append (priv->method_store, &iter);
		gtk_list_store_set (priv->method_store, &iter,
		                    METHOD_COL_NAME, _("Automatic, DHCP only"),
		                    METHOD_COL_NUM, IP6_METHOD_AUTO_DHCP_ONLY,
		                    METHOD_COL_ENABLED, TRUE,
		                    -1);
	}

	/* Manual is pointless for Mobile Broadband */
	if (   priv->connection_type != NM_TYPE_SETTING_GSM
	    && priv->connection_type != NM_TYPE_SETTING_CDMA
	    && priv->connection_type != NM_TYPE_SETTING_VPN) {
		gtk_list_store_append (priv->method_store, &iter);
		gtk_list_store_set (priv->method_store, &iter,
		                    METHOD_COL_NAME, _("Manual"),
		                    METHOD_COL_NUM, IP6_METHOD_MANUAL,
		                    METHOD_COL_ENABLED, TRUE,
		                    -1);
	}

	/* Link-local is pointless for VPNs, Mobile Broadband, and PPPoE */
	if (   priv->connection_type != NM_TYPE_SETTING_VPN
	    && priv->connection_type != NM_TYPE_SETTING_PPPOE
	    && priv->connection_type != NM_TYPE_SETTING_GSM
	    && priv->connection_type != NM_TYPE_SETTING_CDMA) {
		gtk_list_store_append (priv->method_store, &iter);
		gtk_list_store_set (priv->method_store, &iter,
		                    METHOD_COL_NAME, _("Link-Local Only"),
		                    METHOD_COL_NUM, IP6_METHOD_LINK_LOCAL,
		                    METHOD_COL_ENABLED, TRUE,
		                    -1);

		gtk_list_store_append (priv->method_store, &iter);
		gtk_list_store_set (priv->method_store, &iter,
		                    METHOD_COL_NAME, _("Shared to other computers"),
		                    METHOD_COL_NUM, IP6_METHOD_SHARED,
		                    METHOD_COL_ENABLED, FALSE,
		                    -1);
	}

	gtk_combo_box_set_model (priv->method, GTK_TREE_MODEL (priv->method_store));

	priv->addr_label = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_addr_label"));
	priv->addr_add = GTK_BUTTON (gtk_builder_get_object (builder, "ip6_addr_add_button"));
	priv->addr_delete = GTK_BUTTON (gtk_builder_get_object (builder, "ip6_addr_delete_button"));
	priv->addr_list = GTK_TREE_VIEW (gtk_builder_get_object (builder, "ip6_addresses"));

	priv->dns_servers_label = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_dns_servers_label"));
	priv->dns_servers = GTK_ENTRY (gtk_builder_get_object (builder, "ip6_dns_servers_entry"));

	priv->dns_searches_label = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_dns_searches_label"));
	priv->dns_searches = GTK_ENTRY (gtk_builder_get_object (builder, "ip6_dns_searches_entry"));

	priv->ip6_privacy_label = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_privacy_label"));
	priv->ip6_privacy_combo = GTK_COMBO_BOX (gtk_builder_get_object (builder, "ip6_privacy_combo"));

	priv->ip6_required = GTK_CHECK_BUTTON (gtk_builder_get_object (builder, "ip6_required_checkbutton"));
	/* Hide IP6-require button if it'll never be used for a particular method */
	if (   priv->connection_type == NM_TYPE_SETTING_VPN
	    || priv->connection_type == NM_TYPE_SETTING_GSM
	    || priv->connection_type == NM_TYPE_SETTING_CDMA
	    || priv->connection_type == NM_TYPE_SETTING_PPPOE)
		gtk_widget_hide (GTK_WIDGET (priv->ip6_required));

	priv->routes_button = GTK_BUTTON (gtk_builder_get_object (builder, "ip6_routes_button"));
}

static void
method_changed (GtkComboBox *combo, gpointer user_data)
{
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (user_data);
	guint32 method = IP6_METHOD_AUTO;
	gboolean addr_enabled = FALSE;
	gboolean dns_enabled = FALSE;
	gboolean routes_enabled = FALSE;
	gboolean ip6_privacy_enabled = FALSE;
	gboolean ip6_required_enabled = TRUE;
	gboolean method_auto = FALSE;
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter (priv->method, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (priv->method_store), &iter,
		                    METHOD_COL_NUM, &method, -1);
	}

	switch (method) {
	case IP6_METHOD_AUTO:
		addr_enabled = FALSE;
		routes_enabled = TRUE;
		dns_enabled = TRUE;
		method_auto = TRUE;
		ip6_privacy_enabled = TRUE;
		break;
	case IP6_METHOD_AUTO_ADDRESSES:
		addr_enabled = FALSE;
		dns_enabled = routes_enabled = TRUE;
		ip6_privacy_enabled = TRUE;
		break;
	case IP6_METHOD_AUTO_DHCP_ONLY:
		addr_enabled = FALSE;
		routes_enabled = TRUE;
		break;
	case IP6_METHOD_MANUAL:
		addr_enabled = dns_enabled = routes_enabled = TRUE;
		break;
	case IP6_METHOD_IGNORE:
		ip6_required_enabled = FALSE;
		break;
	default:
		break;
	}

	gtk_widget_set_sensitive (priv->addr_label, addr_enabled);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->addr_add), addr_enabled);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->addr_delete), addr_enabled);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->addr_list), addr_enabled);
	if (!addr_enabled) {
		GtkListStore *store;

		store = GTK_LIST_STORE (gtk_tree_view_get_model (priv->addr_list));
		gtk_list_store_clear (store);
	}

	gtk_widget_set_sensitive (priv->dns_servers_label, dns_enabled);
	if (method_auto)
		gtk_label_set_text_with_mnemonic (GTK_LABEL (priv->dns_servers_label), _("Additional DNS ser_vers:"));
	else
		gtk_label_set_text_with_mnemonic (GTK_LABEL (priv->dns_servers_label), _("DNS ser_vers:"));
	gtk_widget_set_sensitive (GTK_WIDGET (priv->dns_servers), dns_enabled);
	if (!dns_enabled)
		gtk_entry_set_text (priv->dns_servers, "");

	gtk_widget_set_sensitive (priv->dns_searches_label, dns_enabled);
	if (method_auto)
		gtk_label_set_text_with_mnemonic (GTK_LABEL (priv->dns_searches_label), _("Additional s_earch domains:"));
	else
		gtk_label_set_text_with_mnemonic (GTK_LABEL (priv->dns_searches_label), _("S_earch domains:"));
	gtk_widget_set_sensitive (GTK_WIDGET (priv->dns_searches), dns_enabled);
	if (!dns_enabled)
		gtk_entry_set_text (priv->dns_searches, "");

	gtk_widget_set_sensitive (priv->ip6_privacy_label, ip6_privacy_enabled);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->ip6_privacy_combo), ip6_privacy_enabled);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->ip6_required), ip6_required_enabled);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->routes_button), routes_enabled);

	ce_page_changed (CE_PAGE (user_data));
}

typedef struct {
	int method;
	GtkComboBox *combo;
} SetMethodInfo;

static gboolean
set_method (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	SetMethodInfo *info = (SetMethodInfo *) user_data;
	int method = 0;

	gtk_tree_model_get (model, iter, METHOD_COL_NUM, &method, -1);
	if (method == info->method) {
		gtk_combo_box_set_active_iter (info->combo, iter);
		return TRUE;
	}
	return FALSE;
}

static void
populate_ui (CEPageIP6 *self)
{
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	NMSettingIP6Config *setting = priv->setting;
	GtkListStore *store;
	GtkTreeIter model_iter;
	int method = IP6_METHOD_AUTO;
	NMSettingIP6ConfigPrivacy ip6_privacy;
	int ip6_privacy_idx = IP6_PRIVACY_DISABLED;
	GString *string = NULL;
	SetMethodInfo info;
	const char *str_method;
	int i;

	/* Method */
	gtk_combo_box_set_active (priv->method, 0);
	str_method = nm_setting_ip6_config_get_method (setting);
	if (str_method) {
		if (!strcmp (str_method, NM_SETTING_IP6_CONFIG_METHOD_IGNORE))
			method = IP6_METHOD_IGNORE;
		if (!strcmp (str_method, NM_SETTING_IP6_CONFIG_METHOD_AUTO))
			method = IP6_METHOD_AUTO;
		if (!strcmp (str_method, NM_SETTING_IP6_CONFIG_METHOD_DHCP))
			method = IP6_METHOD_AUTO_DHCP_ONLY;
		else if (!strcmp (str_method, NM_SETTING_IP6_CONFIG_METHOD_LINK_LOCAL))
			method = IP6_METHOD_LINK_LOCAL;
		else if (!strcmp (str_method, NM_SETTING_IP6_CONFIG_METHOD_MANUAL))
			method = IP6_METHOD_MANUAL;
		else if (!strcmp (str_method, NM_SETTING_IP6_CONFIG_METHOD_SHARED))
			method = IP6_METHOD_SHARED;
	}

	if (method == IP6_METHOD_AUTO && nm_setting_ip6_config_get_ignore_auto_dns (setting))
		method = IP6_METHOD_AUTO_ADDRESSES;

	info.method = method;
	info.combo = priv->method;
	gtk_tree_model_foreach (GTK_TREE_MODEL (priv->method_store), set_method, &info);

	/* Addresses */
	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	for (i = 0; i < nm_setting_ip6_config_get_num_addresses (setting); i++) {
		NMIP6Address *addr = nm_setting_ip6_config_get_address (setting, i);
		const struct in6_addr *tmp_addr;
		char buf[INET6_ADDRSTRLEN + 1];

		if (!addr) {
			g_warning ("%s: empty IP6 Address structure!", __func__);
			continue;
		}

		gtk_list_store_append (store, &model_iter);

		/* Address */
		tmp_addr = nm_ip6_address_get_address (addr);
		(void) inet_ntop (AF_INET6, tmp_addr, &buf[0], sizeof (buf));
		gtk_list_store_set (store, &model_iter, COL_ADDRESS, buf, -1);

		/* Prefix */
		snprintf (buf, sizeof (buf), "%u", nm_ip6_address_get_prefix (addr));
		gtk_list_store_set (store, &model_iter, COL_PREFIX, buf, -1);

		/* Gateway */
		tmp_addr = nm_ip6_address_get_gateway (addr);
		if (tmp_addr && !IN6_IS_ADDR_UNSPECIFIED (tmp_addr)) {
			(void) inet_ntop (AF_INET6, tmp_addr, &buf[0], sizeof (buf));
			gtk_list_store_set (store, &model_iter, COL_GATEWAY, buf, -1);
		}
	}

	gtk_tree_view_set_model (priv->addr_list, GTK_TREE_MODEL (store));
	g_signal_connect_swapped (store, "row-inserted", G_CALLBACK (ce_page_changed), self);
	g_signal_connect_swapped (store, "row-deleted", G_CALLBACK (ce_page_changed), self);
	g_object_unref (store);

	/* DNS servers */
	string = g_string_new ("");
	for (i = 0; i < nm_setting_ip6_config_get_num_dns (setting); i++) {
		const struct in6_addr *tmp_addr;
		char buf[INET6_ADDRSTRLEN + 1];

		tmp_addr = nm_setting_ip6_config_get_dns (setting, i);
		if (!tmp_addr)
			continue;

		(void) inet_ntop (AF_INET6, tmp_addr, &buf[0], sizeof (buf));
		if (string->len)
			g_string_append (string, ", ");
		g_string_append (string, buf);
	}
	gtk_entry_set_text (priv->dns_servers, string->str);
	g_string_free (string, TRUE);

	/* DNS searches */
	string = g_string_new ("");
	for (i = 0; i < nm_setting_ip6_config_get_num_dns_searches (setting); i++) {
		if (string->len)
			g_string_append (string, ", ");
		g_string_append (string, nm_setting_ip6_config_get_dns_search (setting, i));
	}
	gtk_entry_set_text (priv->dns_searches, string->str);
	g_string_free (string, TRUE);

	/* IPv6 privacy extensions */
	ip6_privacy = nm_setting_ip6_config_get_ip6_privacy (setting);
	switch (ip6_privacy) {
	case NM_SETTING_IP6_CONFIG_PRIVACY_DISABLED:
		ip6_privacy_idx = IP6_PRIVACY_DISABLED;
		break;
	case NM_SETTING_IP6_CONFIG_PRIVACY_PREFER_PUBLIC_ADDR:
		ip6_privacy_idx = IP6_PRIVACY_PREFER_PUBLIC;
		break;
	case NM_SETTING_IP6_CONFIG_PRIVACY_PREFER_TEMP_ADDR:
		ip6_privacy_idx = IP6_PRIVACY_PREFER_TEMP;
		break;
	default:
		ip6_privacy_idx = IP6_PRIVACY_DISABLED;
		break;
	}
	gtk_combo_box_set_active (priv->ip6_privacy_combo, ip6_privacy_idx);

	/* IPv6 required */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->ip6_required),
	                              !nm_setting_ip6_config_get_may_fail (setting));
}

static void
addr_add_clicked (GtkButton *button, gpointer user_data)
{
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (user_data);
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	GList *cells;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (priv->addr_list));
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, COL_ADDRESS, "", -1);

	selection = gtk_tree_view_get_selection (priv->addr_list);
	gtk_tree_selection_select_iter (selection, &iter);

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
	column = gtk_tree_view_get_column (priv->addr_list, COL_ADDRESS);

	/* FIXME: using cells->data is pretty fragile but GTK apparently doesn't
	 * have a way to get a cell renderer from a column based on path or iter
	 * or whatever.
	 */
	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	gtk_tree_view_set_cursor_on_cell (priv->addr_list, path, column, cells->data, TRUE);

	g_list_free (cells);
	gtk_tree_path_free (path);
}

static void
addr_delete_clicked (GtkButton *button, gpointer user_data)
{
	GtkTreeView *treeview = GTK_TREE_VIEW (user_data);
	GtkTreeSelection *selection;
	GList *selected_rows;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	int num_rows;

	selection = gtk_tree_view_get_selection (treeview);
	if (gtk_tree_selection_count_selected_rows (selection) != 1)
		return;

	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);
	if (!selected_rows)
		return;

	if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) selected_rows->data))
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	g_list_foreach (selected_rows, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected_rows);

	num_rows = gtk_tree_model_iter_n_children (model, NULL);
	if (num_rows && gtk_tree_model_iter_nth_child (model, &iter, NULL, num_rows - 1)) {
		selection = gtk_tree_view_get_selection (treeview);
		gtk_tree_selection_select_iter (selection, &iter);
	}
}

static void
list_selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
	GtkWidget *button = GTK_WIDGET (user_data);
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_widget_set_sensitive (button, TRUE);
	else
		gtk_widget_set_sensitive (button, FALSE);
}

static void
cell_editing_canceled (GtkCellRenderer *renderer, gpointer user_data)
{
	CEPageIP6 *self;
	CEPageIP6Private *priv;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	guint32 column;

	/* user_data disposed? */
	if (GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (renderer), "ce-page-not-valid")))
		return;

	self = CE_PAGE_IP6 (user_data);
	priv = CE_PAGE_IP6_GET_PRIVATE (self);

	if (priv->last_edited) {
		selection = gtk_tree_view_get_selection (priv->addr_list);
		if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
			column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (renderer), "column"));
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, priv->last_edited, -1);
		}

		g_free (priv->last_edited);
		priv->last_edited = NULL;

		ce_page_changed (CE_PAGE (self));
	}

	g_free (priv->last_path);
	priv->last_path = NULL;
	priv->last_column = -1;
}

static void
cell_edited (GtkCellRendererText *cell,
             const gchar *path_string,
             const gchar *new_text,
             gpointer user_data)
{
	CEPageIP6 *self = CE_PAGE_IP6 (user_data);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (priv->addr_list));
	GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
	GtkTreeIter iter;
	guint32 column;
	GtkTreeViewColumn *next_col;

	/* Free auxiliary stuff */
	g_free (priv->last_edited);
	priv->last_edited = NULL;
	g_free (priv->last_path);
	priv->last_path = NULL;
	priv->last_column = -1;

	column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (cell), "column"));
	gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
	gtk_list_store_set (store, &iter, column, new_text, -1);

	/* Move focus to the next column */
	column = (column >= COL_LAST) ? 0 : column + 1;
	next_col = gtk_tree_view_get_column (priv->addr_list, column);
	gtk_tree_view_set_cursor_on_cell (priv->addr_list, path, next_col, priv->addr_cells[column], TRUE);

	gtk_tree_path_free (path);
	ce_page_changed (CE_PAGE (self));
}


static void
ip_address_filter_cb (GtkEditable *editable,
                      gchar *text,
                      gint length,
                      gint *position,
                      gpointer user_data)
{
	CEPageIP6 *self = CE_PAGE_IP6 (user_data);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	guint column;
	gboolean changed;


	/* The prefix column only allows numbers, no ':' */
	column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (editable), "column"));

	changed = utils_filter_editable_on_insert_text (editable,
	                                                text, length, position, user_data,
	                                                column == COL_PREFIX ? utils_char_is_ascii_digit : utils_char_is_ascii_ip6_address,
	                                                ip_address_filter_cb);

	if (changed) {
		g_free (priv->last_edited);
		priv->last_edited = gtk_editable_get_chars (editable, 0, -1);
	}
}

static gboolean
_char_is_ascii_dns_servers (char character)
{
	return utils_char_is_ascii_ip6_address (character) ||
	       character == ' ' ||
	       character == ',' ||
	       character == ';';
}

static void
dns_servers_filter_cb (GtkEditable *editable,
                       gchar *text,
                       gint length,
                       gint *position,
                       gpointer user_data)
{
	utils_filter_editable_on_insert_text (editable,
	                                      text, length, position, user_data,
	                                      _char_is_ascii_dns_servers,
	                                      dns_servers_filter_cb);
}

static void
delete_text_cb (GtkEditable *editable,
                    gint start_pos,
                    gint end_pos,
                    gpointer user_data)
{
	CEPageIP6 *self = CE_PAGE_IP6 (user_data);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);

	/* Keep last_edited up-to-date */
	g_free (priv->last_edited);
	priv->last_edited = gtk_editable_get_chars (editable, 0, -1);
}

static gboolean
cell_changed_cb (GtkEditable *editable,
                 gpointer user_data)
{
	char *cell_text;
	guint column;
	GdkRGBA rgba;
	gboolean value_valid = FALSE;
	const char *colorname = NULL;

	cell_text = gtk_editable_get_chars (editable, 0, -1);

	/* The Prefix column is 0..128 */
	column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (user_data), "column"));
	if (column == COL_PREFIX) {
		guint32 prefix;
		char *end;

		prefix = strtoul (cell_text, &end, 10);
		if (!end || *end || prefix == 0 || prefix > 128)
			value_valid = FALSE;
		else
			value_valid = TRUE;
	} else {
		struct in6_addr tmp_addr;

		if (inet_pton (AF_INET6, cell_text, &tmp_addr))
			value_valid = TRUE;
	}

	/* Change cell's background color while editing */
	colorname = value_valid ? "lightgreen" : "red";

	gdk_rgba_parse (&rgba, colorname);
	gtk_widget_override_background_color (GTK_WIDGET (editable), GTK_STATE_FLAG_NORMAL, &rgba);

	g_free (cell_text);
	return FALSE;
}

static gboolean
key_pressed_cb (GtkWidget *widget,
                GdkEvent *event,
                gpointer user_data)
{
	GdkKeymapKey *keys = NULL;
	gint n_keys;

	/*
	 * Tab should behave the same way as Enter (cycling on cells).
	 *
	 * Previously, we had finished cell editing, which appeared to work:
	 *   gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (widget));
	 * But unfortunately, it showed up crash occurred with XIM input (GTK_IM_MODULE=xim).
	 * https://bugzilla.redhat.com/show_bug.cgi?id=747368
	 */
	if (event->type == GDK_KEY_PRESS && event->key.keyval == GDK_KEY_Tab) {
		/* Get hardware keycode for GDK_KEY_Return */
		if (gdk_keymap_get_entries_for_keyval (gdk_keymap_get_default (), GDK_KEY_Return, &keys, &n_keys)) {
			/* Change 'Tab' to 'Enter' key */
			event->key.keyval = GDK_KEY_Return;
			event->key.hardware_keycode = keys[0].keycode;
		}
		g_free (keys);
	}

	return FALSE;
}

static void
cell_editing_started (GtkCellRenderer *cell,
                      GtkCellEditable *editable,
                      const gchar     *path,
                      gpointer         user_data)
{
	CEPageIP6 *self = CE_PAGE_IP6 (user_data);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	guint column;

	if (!GTK_IS_ENTRY (editable)) {
		g_warning ("%s: Unexpected cell editable type.", __func__);
		return;
	}

	/* Initialize last_path and last_column, last_edited is initialized when the cell is edited */
	g_free (priv->last_edited);
	priv->last_edited = NULL;
	g_free (priv->last_path);
	priv->last_path = g_strdup (path);
	priv->last_column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (cell), "column"));

	/* Need to pass column # to the editable's insert-text function */	
	column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (cell), "column"));
	g_object_set_data (G_OBJECT (editable), "column", GUINT_TO_POINTER (column));

	/* Set up the entry filter */
	g_signal_connect (G_OBJECT (editable), "insert-text",
	                  (GCallback) ip_address_filter_cb,
	                  user_data);

	g_signal_connect_after (G_OBJECT (editable), "delete-text",
	                        (GCallback) delete_text_cb,
	                        user_data);

	/* Set up handler for value verifying and changing cell background */
	g_signal_connect (G_OBJECT (editable), "changed",
	                  (GCallback) cell_changed_cb,
	                  cell);

	/* Set up key pressed handler - need to handle Tab key */
	g_signal_connect (G_OBJECT (editable), "key-press-event",
	                  (GCallback) key_pressed_cb,
	                  user_data);
}

static void
routes_dialog_close_cb (GtkWidget *dialog, gpointer user_data)
{
	gtk_widget_hide (dialog);
	/* gtk_widget_destroy() will remove the window from the window group */
	gtk_widget_destroy (dialog);
}

static void
routes_dialog_response_cb (GtkWidget *dialog, gint response, gpointer user_data)
{
	CEPageIP6 *self = CE_PAGE_IP6 (user_data);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);

	if (response == GTK_RESPONSE_OK)
		ip6_routes_dialog_update_setting (dialog, priv->setting);

	routes_dialog_close_cb (dialog, NULL);
}

static void
routes_button_clicked_cb (GtkWidget *button, gpointer user_data)
{
	CEPageIP6 *self = CE_PAGE_IP6 (user_data);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	GtkWidget *dialog, *toplevel;
	gboolean automatic = FALSE;
	const char *method;
	char *tmp;

	toplevel = gtk_widget_get_toplevel (CE_PAGE (self)->page);
	g_return_if_fail (gtk_widget_is_toplevel (toplevel));

	method = nm_setting_ip6_config_get_method (priv->setting);
	if (!method || !strcmp (method, NM_SETTING_IP6_CONFIG_METHOD_AUTO))
		automatic = TRUE;

	dialog = ip6_routes_dialog_new (priv->setting, automatic);
	if (!dialog) {
		g_warning ("%s: failed to create the routes dialog!", __func__);
		return;
	}

	gtk_window_group_add_window (priv->window_group, GTK_WINDOW (dialog));
	if (!priv->window_added) {
		gtk_window_group_add_window (priv->window_group, GTK_WINDOW (toplevel));
		priv->window_added = TRUE;
	}

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	tmp = g_strdup_printf (_("Editing IPv6 routes for %s"), priv->connection_id);
	gtk_window_set_title (GTK_WINDOW (dialog), tmp);
	g_free (tmp);

	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (routes_dialog_response_cb), self);

	gtk_widget_show_all (dialog);
}

static gboolean
tree_view_button_pressed_cb (GtkWidget *widget,
                             GdkEvent *event,
                             gpointer user_data)
{
	CEPageIP6 *self = CE_PAGE_IP6 (user_data);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);

	/* last_edited can be set e.g. when we get here by clicking an cell while
	 * editing another cell. GTK3 issue neither editing-canceled nor editing-done
	 * for cell renderer. Thus the previous cell value isn't saved. Store it now. */
	if (priv->last_edited && priv->last_path) {
		GtkTreeIter iter;
		GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (priv->addr_list));
		GtkTreePath *last_treepath = gtk_tree_path_new_from_string (priv->last_path);

		gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, last_treepath);
		gtk_list_store_set (store, &iter, priv->last_column, priv->last_edited, -1);
		gtk_tree_path_free (last_treepath);

		g_free (priv->last_edited);
		priv->last_edited = NULL;
		g_free (priv->last_path);
		priv->last_path = NULL;
		priv->last_column = -1;
	}

	/* Ignore double clicks events. (They are issued after the single clicks, see GdkEventButton) */
	if (event->type == GDK_2BUTTON_PRESS)
		return TRUE;

	gtk_widget_grab_focus (GTK_WIDGET (priv->addr_list));
	return FALSE;
}

static void
finish_setup (CEPageIP6 *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	GtkTreeSelection *selection;
	gint offset;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	if (error)
		return;

	populate_ui (self);

	/* IP Address column */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), self);
	g_object_set_data (G_OBJECT (renderer), "column", GUINT_TO_POINTER (COL_ADDRESS));
	g_signal_connect (renderer, "editing-started", G_CALLBACK (cell_editing_started), self);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (cell_editing_canceled), self);
	priv->addr_cells[COL_ADDRESS] = GTK_CELL_RENDERER (renderer);

	offset = gtk_tree_view_insert_column_with_attributes (priv->addr_list,
	                                                      -1, _("Address"), renderer,
	                                                      "text", COL_ADDRESS,
	                                                      NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->addr_list), offset - 1);
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	/* Prefix column */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), self);
	g_object_set_data (G_OBJECT (renderer), "column", GUINT_TO_POINTER (COL_PREFIX));
	g_signal_connect (renderer, "editing-started", G_CALLBACK (cell_editing_started), self);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (cell_editing_canceled), self);
	priv->addr_cells[COL_PREFIX] = GTK_CELL_RENDERER (renderer);

	offset = gtk_tree_view_insert_column_with_attributes (priv->addr_list,
	                                                      -1, _("Prefix"), renderer,
	                                                      "text", COL_PREFIX,
	                                                      NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->addr_list), offset - 1);
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	/* Gateway column */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), self);
	g_object_set_data (G_OBJECT (renderer), "column", GUINT_TO_POINTER (COL_GATEWAY));
	g_signal_connect (renderer, "editing-started", G_CALLBACK (cell_editing_started), self);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (cell_editing_canceled), self);
	priv->addr_cells[COL_GATEWAY] = GTK_CELL_RENDERER (renderer);

	offset = gtk_tree_view_insert_column_with_attributes (priv->addr_list,
	                                                      -1, _("Gateway"), renderer,
	                                                      "text", COL_GATEWAY,
	                                                      NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->addr_list), offset - 1);
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	g_signal_connect (priv->addr_list, "button-press-event", G_CALLBACK (tree_view_button_pressed_cb), self);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->addr_add), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->addr_delete), FALSE);

	g_signal_connect (priv->addr_add, "clicked", G_CALLBACK (addr_add_clicked), self);
	g_signal_connect (priv->addr_delete, "clicked", G_CALLBACK (addr_delete_clicked), priv->addr_list);
	selection = gtk_tree_view_get_selection (priv->addr_list);
	g_signal_connect (selection, "changed", G_CALLBACK (list_selection_changed), priv->addr_delete);

	g_signal_connect_swapped (priv->dns_servers, "changed", G_CALLBACK (ce_page_changed), self);
	g_signal_connect (priv->dns_servers, "insert-text", G_CALLBACK (dns_servers_filter_cb), self);
	g_signal_connect_swapped (priv->dns_searches, "changed", G_CALLBACK (ce_page_changed), self);
	g_signal_connect_swapped (priv->ip6_privacy_combo, "changed", G_CALLBACK (ce_page_changed), self);

	method_changed (priv->method, self);
	g_signal_connect (priv->method, "changed", G_CALLBACK (method_changed), self);

	g_signal_connect_swapped (priv->ip6_required, "toggled", G_CALLBACK (ce_page_changed), self);

	g_signal_connect (priv->routes_button, "clicked", G_CALLBACK (routes_button_clicked_cb), self);
}

CEPage *
ce_page_ip6_new (NMConnectionEditor *editor,
                 NMConnection *connection,
                 GtkWindow *parent_window,
                 NMClient *client,
                 NMRemoteSettings *settings,
                 const char **out_secrets_setting_name,
                 GError **error)
{
	CEPageIP6 *self;
	CEPageIP6Private *priv;
	NMSettingConnection *s_con;

	self = CE_PAGE_IP6 (ce_page_new (CE_TYPE_PAGE_IP6,
	                                 editor,
	                                 connection,
	                                 parent_window,
	                                 client,
	                                 settings,
	                                 UIDIR "/ce-page-ip6.ui",
	                                 "IP6Page",
	                                 _("IPv6 Settings")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load IPv6 user interface."));
		return NULL;
	}

	ip6_private_init (self, connection);
	priv = CE_PAGE_IP6_GET_PRIVATE (self);

	priv->window_group = gtk_window_group_new ();

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);
	priv->connection_id = g_strdup (nm_setting_connection_get_id (s_con));

	priv->setting = nm_connection_get_setting_ip6_config (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_IP6_CONFIG (nm_setting_ip6_config_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static gboolean
ui_to_setting (CEPageIP6 *self, GError **error)
{
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	GtkTreeModel *model;
	GtkTreeIter tree_iter;
	int int_method = IP6_METHOD_AUTO;
	const char *method;
	gboolean valid = FALSE, iter_valid;
	const char *text;
	gboolean ignore_auto_dns = FALSE;
	char **items = NULL, **iter;
	gboolean may_fail;
	NMSettingIP6ConfigPrivacy ip6_privacy;

	/* Method */
	if (gtk_combo_box_get_active_iter (priv->method, &tree_iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (priv->method_store), &tree_iter,
		                    METHOD_COL_NUM, &int_method, -1);
	}

	switch (int_method) {
	case IP6_METHOD_IGNORE:
		method = NM_SETTING_IP6_CONFIG_METHOD_IGNORE;
		break;
	case IP6_METHOD_LINK_LOCAL:
		method = NM_SETTING_IP6_CONFIG_METHOD_LINK_LOCAL;
		break;
	case IP6_METHOD_MANUAL:
		method = NM_SETTING_IP6_CONFIG_METHOD_MANUAL;
		break;
	case IP6_METHOD_SHARED:
		method = NM_SETTING_IP6_CONFIG_METHOD_SHARED;
		break;
	case IP6_METHOD_AUTO_DHCP_ONLY:
		method = NM_SETTING_IP6_CONFIG_METHOD_DHCP;
		break;
	case IP6_METHOD_AUTO_ADDRESSES:
		ignore_auto_dns = TRUE;
		/* fall through */
	default:
		method = NM_SETTING_IP6_CONFIG_METHOD_AUTO;
		break;
	}

	g_object_freeze_notify (G_OBJECT (priv->setting));
	g_object_set (priv->setting,
	              NM_SETTING_IP6_CONFIG_METHOD, method,
	              NM_SETTING_IP6_CONFIG_IGNORE_AUTO_DNS, ignore_auto_dns,
	              NULL);

	/* IP addresses */
	nm_setting_ip6_config_clear_addresses (priv->setting);
	model = gtk_tree_view_get_model (priv->addr_list);
	iter_valid = gtk_tree_model_get_iter_first (model, &tree_iter);
	while (iter_valid) {
		char *item = NULL, *end;
		struct in6_addr tmp_addr, tmp_gw;
		gboolean have_gw = FALSE;
		NMIP6Address *addr;
		guint32 prefix;

		/* IP address */
		gtk_tree_model_get (model, &tree_iter, COL_ADDRESS, &item, -1);
		if (!item || !inet_pton (AF_INET6, item, &tmp_addr)) {
			g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("IPv6 address \"%s\" invalid"), item ? item : "");
			g_free (item);
			goto out;
		}
		g_free (item);

		/* Prefix */
		gtk_tree_model_get (model, &tree_iter, COL_PREFIX, &item, -1);
		if (!item) {
			g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("IPv6 prefix \"%s\" missing"));
			goto out;
		}

		prefix = strtoul (item, &end, 10);
		if (!end || *end || prefix == 0 || prefix > 128) {
			g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("IPv6 prefix \"%s\" invalid"), item);
			g_free (item);
			goto out;
		}
		g_free (item);

		/* Gateway */
		gtk_tree_model_get (model, &tree_iter, COL_GATEWAY, &item, -1);
		if (item && strlen (item)) {
			if (!inet_pton (AF_INET6, item, &tmp_gw)) {
				g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("IPv6 gateway \"%s\" invalid"), item);
				g_free (item);
				goto out;
			}
			if (!IN6_IS_ADDR_UNSPECIFIED (&tmp_gw))
				have_gw = TRUE;
		}
		g_free (item);

		addr = nm_ip6_address_new ();
		nm_ip6_address_set_address (addr, &tmp_addr);
		nm_ip6_address_set_prefix (addr, prefix);
		if (have_gw)
			nm_ip6_address_set_gateway (addr, &tmp_gw);
		nm_setting_ip6_config_add_address (priv->setting, addr);
		nm_ip6_address_unref (addr);

		iter_valid = gtk_tree_model_iter_next (model, &tree_iter);
	}

	/* DNS servers */
	nm_setting_ip6_config_clear_dns (priv->setting);
	text = gtk_entry_get_text (GTK_ENTRY (priv->dns_servers));
	if (text && strlen (text)) {
		items = g_strsplit_set (text, ", ;", 0);
		for (iter = items; *iter; iter++) {
			struct in6_addr tmp_addr;
			char *stripped = g_strstrip (*iter);

			if (!strlen (stripped))
				continue;

			if (inet_pton (AF_INET6, stripped, &tmp_addr)) {
				nm_setting_ip6_config_add_dns (priv->setting, &tmp_addr);
			} else {
				g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("IPv6 DNS server \"%s\" invalid"), stripped);
				g_strfreev (items);
				goto out;
			}
		}
		g_strfreev (items);
	}

	/* Search domains */
	nm_setting_ip6_config_clear_dns_searches (priv->setting);
	text = gtk_entry_get_text (GTK_ENTRY (priv->dns_searches));
	if (text && strlen (text)) {
		items = g_strsplit_set (text, ", ;:", 0);
		for (iter = items; *iter; iter++) {
			char *stripped = g_strstrip (*iter);

			if (strlen (stripped))
				nm_setting_ip6_config_add_dns_search (priv->setting, stripped);
		}
		g_strfreev (items);
	}

	/* IPv6 Privacy */
	switch (gtk_combo_box_get_active (priv->ip6_privacy_combo)) {
	case IP6_PRIVACY_DISABLED:
		ip6_privacy = NM_SETTING_IP6_CONFIG_PRIVACY_DISABLED;
		break;
	case IP6_PRIVACY_PREFER_PUBLIC:
		ip6_privacy = NM_SETTING_IP6_CONFIG_PRIVACY_PREFER_PUBLIC_ADDR;
		break;
	case IP6_PRIVACY_PREFER_TEMP:
		ip6_privacy = NM_SETTING_IP6_CONFIG_PRIVACY_PREFER_TEMP_ADDR;
		break;
	default:
		ip6_privacy = NM_SETTING_IP6_CONFIG_PRIVACY_UNKNOWN;
		break;
	}

	may_fail = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->ip6_required));
	g_object_set (G_OBJECT (priv->setting),
	              NM_SETTING_IP6_CONFIG_MAY_FAIL, may_fail,
	              NM_SETTING_IP6_CONFIG_IP6_PRIVACY, ip6_privacy,
	              NULL);

	valid = TRUE;

out:
	g_object_thaw_notify (G_OBJECT (priv->setting));

	return valid;
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageIP6 *self = CE_PAGE_IP6 (page);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);

	if (!ui_to_setting (self, error))
		return FALSE;
	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static gboolean
get_iter_for_method (GtkTreeModel *model, int column, GtkTreeIter *iter)
{
	int col;

	if (gtk_tree_model_get_iter_first (model, iter)) {
		do {
			gtk_tree_model_get (model, iter, METHOD_COL_NUM, &col, -1);
			if (col == column)
				return TRUE;
		} while (gtk_tree_model_iter_next (model, iter));
	}
	return FALSE;
}

static void
toggle_method_sensitivity (CEPage *page, int column, gboolean sensitive)
{
	CEPageIP6 *self = CE_PAGE_IP6 (page);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	GtkTreeModel *model = GTK_TREE_MODEL (priv->method_store);
	GtkTreeIter iter;

	if (get_iter_for_method (model, column, &iter))
		gtk_list_store_set (priv->method_store, &iter, METHOD_COL_ENABLED, sensitive, -1);
}

static gboolean
get_method_sensitivity (CEPage *page, int column)
{
	CEPageIP6 *self = CE_PAGE_IP6 (page);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	GtkTreeModel *model = GTK_TREE_MODEL (priv->method_store);
	GtkTreeIter iter;
	gboolean sensitive = FALSE;

	if (get_iter_for_method (model, column, &iter))
		gtk_tree_model_get (GTK_TREE_MODEL (priv->method_store), &iter, METHOD_COL_ENABLED, &sensitive, -1);
	return sensitive;
}

static void
change_method_combo (CEPage *page, gboolean is_hotspot)
{
	CEPageIP6 *self = CE_PAGE_IP6 (page);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);

	/* Store previous active method */
	if (get_method_sensitivity (page, IP6_METHOD_AUTO))
		priv->normal_method_idx = gtk_combo_box_get_active (priv->method);
	else
		priv->hotspot_method_idx = gtk_combo_box_get_active (priv->method);

	/* Set active method */
	if (is_hotspot) {
		if (priv->hotspot_method_idx != -1)
			gtk_combo_box_set_active (priv->method, priv->hotspot_method_idx);
		else
			gtk_combo_box_set_active (priv->method, IP6_METHOD_IGNORE);
	} else {
		if (priv->normal_method_idx != -1)
			gtk_combo_box_set_active (priv->method, priv->normal_method_idx);
	}

	toggle_method_sensitivity (page, IP6_METHOD_AUTO, !is_hotspot);
	toggle_method_sensitivity (page, IP6_METHOD_AUTO_ADDRESSES, !is_hotspot);
	toggle_method_sensitivity (page, IP6_METHOD_AUTO_DHCP_ONLY, !is_hotspot);
	toggle_method_sensitivity (page, IP6_METHOD_MANUAL, !is_hotspot);
	toggle_method_sensitivity (page, IP6_METHOD_LINK_LOCAL, !is_hotspot);
}

static gboolean
inter_page_change (CEPage *page)
{
	gpointer wifi_mode_ap;

	if (nm_connection_editor_inter_page_get_value (page->editor, INTER_PAGE_CHANGE_WIFI_MODE, &wifi_mode_ap)) {
		/* For Wi-Fi AP mode restrict IPv6 methods to ignore */
		if (GPOINTER_TO_UINT (wifi_mode_ap))
			change_method_combo (page, TRUE);
		else
			change_method_combo (page, FALSE);
	}
	return TRUE;
}

static void
ce_page_ip6_init (CEPageIP6 *self)
{
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);

	priv->last_column = -1;
	priv->normal_method_idx = -1;
	priv->hotspot_method_idx = -1;
}

static void
dispose (GObject *object)
{
	CEPageIP6 *self = CE_PAGE_IP6 (object);
	CEPageIP6Private *priv = CE_PAGE_IP6_GET_PRIVATE (self);
	int i;

	g_clear_object (&priv->window_group);

	/* Mark CEPageIP6 object as invalid; store this indication to cells to be usable in callbacks */
	for (i = 0; i <= COL_LAST; i++)
		g_object_set_data (G_OBJECT (priv->addr_cells[i]), "ce-page-not-valid", GUINT_TO_POINTER (1));

	g_clear_pointer (&priv->connection_id, g_free);

	G_OBJECT_CLASS (ce_page_ip6_parent_class)->dispose (object);
}

static void
ce_page_ip6_class_init (CEPageIP6Class *ip6_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (ip6_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (ip6_class);

	g_type_class_add_private (object_class, sizeof (CEPageIP6Private));

	/* virtual methods */
	parent_class->ce_page_validate_v = ce_page_validate_v;
	parent_class->inter_page_change = inter_page_change;
	object_class->dispose = dispose;
}
