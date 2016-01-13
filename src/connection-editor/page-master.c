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

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <nm-setting-connection.h>
#include <nm-utils.h>

#include "page-master.h"
#include "nm-connection-editor.h"

G_DEFINE_TYPE (CEPageMaster, ce_page_master, CE_TYPE_PAGE)

#define CE_PAGE_MASTER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_MASTER, CEPageMasterPrivate))

typedef struct {
	const char *uuid;

	GtkWindow *toplevel;

	GtkEntry *interface_name;

	GtkTreeView *connections;
	GtkTreeModel *connections_model;
	GtkButton *add, *edit, *delete;

	GHashTable *new_slaves;  /* track whether some slave(s) were added */

} CEPageMasterPrivate;

enum {
	CREATE_CONNECTION,
	CONNECTION_ADDED,
	CONNECTION_REMOVED,

	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void finish_setup (CEPageMaster *self, gpointer unused, GError *error, gpointer user_data);

enum {
	COL_CONNECTION,
	COL_NAME
};

static int
name_sort_func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	NMConnection *conn_a, *conn_b;
	int ret;

	/* We fetch COL_CONNECTION rather than COL_NAME to avoid a strdup/free. */
	gtk_tree_model_get (model, a, COL_CONNECTION, &conn_a, -1);
	gtk_tree_model_get (model, b, COL_CONNECTION, &conn_b, -1);
	ret = strcmp (nm_connection_get_id (conn_a), nm_connection_get_id (conn_b));
	g_object_unref (conn_a);
	g_object_unref (conn_b);

	return ret;
}

static void
constructed (GObject *object)
{
	CEPageMaster *self = CE_PAGE_MASTER (object);

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	G_OBJECT_CLASS (ce_page_master_parent_class)->constructed (object);
}

static void
dispose (GObject *object)
{
	CEPageMaster *self = CE_PAGE_MASTER (object);
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	GtkTreeIter iter;

	g_signal_handlers_disconnect_matched (CE_PAGE (self)->settings, G_SIGNAL_MATCH_DATA,
	                                      0, 0, NULL, NULL, self);

	if (gtk_tree_model_get_iter_first (priv->connections_model, &iter)) {
		do {
			NMRemoteConnection *connection = NULL;

			gtk_tree_model_get (priv->connections_model, &iter,
			                    COL_CONNECTION, &connection,
			                    -1);
			g_signal_handlers_disconnect_matched (connection, G_SIGNAL_MATCH_DATA,
			                                      0, 0, NULL, NULL, self);
			g_object_unref (connection);
		} while (gtk_tree_model_iter_next (priv->connections_model, &iter));
	}

	g_hash_table_destroy (priv->new_slaves);

	G_OBJECT_CLASS (ce_page_master_parent_class)->dispose (object);
}

static void
stuff_changed (GtkWidget *widget, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static gboolean
find_connection (CEPageMaster *self, NMRemoteConnection *connection, GtkTreeIter *iter)
{
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);

	if (!gtk_tree_model_get_iter_first (priv->connections_model, iter))
		return FALSE;

	do {
		NMRemoteConnection *candidate = NULL;

		gtk_tree_model_get (priv->connections_model, iter,
		                    COL_CONNECTION, &candidate,
		                    -1);
		if (candidate == connection)
			return TRUE;
	} while (gtk_tree_model_iter_next (priv->connections_model, iter));

	return FALSE;
}

static void
connection_removed (NMRemoteConnection *connection, gpointer user_data)
{
	CEPageMaster *self = CE_PAGE_MASTER (user_data);
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	GtkTreeIter iter;

	if (!find_connection (self, connection, &iter))
		return;

	gtk_list_store_remove (GTK_LIST_STORE (priv->connections_model), &iter);
	ce_page_changed (CE_PAGE (self));

	g_signal_emit (self, signals[CONNECTION_REMOVED], 0, connection);
}

static void
connection_updated (NMRemoteConnection *connection, gpointer user_data)
{
	CEPageMaster *self = CE_PAGE_MASTER (user_data);
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	GtkTreeIter iter;
	NMSettingConnection *s_con;

	if (!find_connection (self, connection, &iter))
		return;

	/* Name might have changed */
	s_con = nm_connection_get_setting_connection (NM_CONNECTION (connection));
	gtk_list_store_set (GTK_LIST_STORE (priv->connections_model), &iter,
	                    COL_NAME, nm_setting_connection_get_id (s_con),
	                    -1);
}

static NMDevice *
get_device_for_connection (NMClient *client, NMConnection *conn)
{
	const GPtrArray *devices;
	NMSettingConnection *s_con;
	int i;

	devices = nm_client_get_devices (client);
	if (!devices)
		return NULL;

	/* Make sure the connection is actually locked to a specific device */
	s_con = nm_connection_get_setting_connection (conn);
	if (   !nm_setting_connection_get_interface_name (s_con)
	       && !nm_connection_get_virtual_iface_name (conn)) {
		NMSetting *s_hw;
		GByteArray *mac_address;

		s_hw = nm_connection_get_setting_by_name (conn, nm_setting_connection_get_connection_type (s_con));
		if (!s_hw || !g_object_class_find_property (G_OBJECT_GET_CLASS (s_hw), "mac-address"))
			return NULL;

		g_object_get (G_OBJECT (s_hw), "mac-address", &mac_address, NULL);
		if (!mac_address)
			return NULL;
		g_byte_array_unref (mac_address);
	}

	/* OK, now find that device */
	for (i = 0; i < devices->len; i++) {
		NMDevice *device = devices->pdata[i];

		if (nm_device_connection_compatible (device, conn, NULL))
			return device;
	}

	return NULL;
}

static void
check_new_slave_physical_port (CEPageMaster *self, NMConnection *conn)
{
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	NMConnection *conn2;
	NMDevice *dev, *dev2;
	const char *id, *id2;
	GtkTreeIter iter;

	dev = get_device_for_connection (CE_PAGE (self)->client, conn);
	if (!dev)
		return;
	id = nm_device_get_physical_port_id (dev);
	if (!id)
		return;

	if (!gtk_tree_model_get_iter_first (priv->connections_model, &iter))
		return;
	do {
		gtk_tree_model_get (priv->connections_model, &iter, 0, &conn2, -1);
		g_object_unref (conn2); /* gtk_tree_model_get() adds a ref */
		dev2 = get_device_for_connection (CE_PAGE (self)->client, conn2);
		if (!dev2)
			continue;
		if (dev == dev2) {
			nm_connection_editor_warning (CE_PAGE (self)->parent_window,
			                              _("Duplicate slaves"),
			                              _("Slaves '%s' and '%s' both apply to device '%s'"),
			                              nm_connection_get_id (conn),
			                              nm_connection_get_id (conn2),
			                              nm_device_get_iface (dev));
			return;
		}

		id2 = nm_device_get_physical_port_id (dev2);
		if (self->aggregating && id && id2 && !strcmp (id, id2)) {
			nm_connection_editor_warning (CE_PAGE (self)->parent_window,
			                              _("Duplicate slaves"),
			                              _("Slaves '%s' and '%s' apply to different virtual "
			                                "ports ('%s' and '%s') of the same physical device."),
			                              nm_connection_get_id (conn),
			                              nm_connection_get_id (conn2),
			                              nm_device_get_iface (dev),
			                              nm_device_get_iface (dev2));
			return;
		}
	} while (gtk_tree_model_iter_next (priv->connections_model, &iter));
}

static void
connection_added (NMRemoteSettings *settings,
                  NMRemoteConnection *connection,
                  gpointer user_data)
{
	CEPageMaster *self = CE_PAGE_MASTER (user_data);
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	const char *master_type;
	const char *slave_type, *master;
	const char *interface_name;
	GtkTreeIter iter;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	master_type = nm_setting_connection_get_connection_type (s_con);

	s_con = nm_connection_get_setting_connection (NM_CONNECTION (connection));
	if (!s_con)
		return;

	slave_type = nm_setting_connection_get_slave_type (s_con);
	if (g_strcmp0 (slave_type, master_type) != 0)
		return;

	master = nm_setting_connection_get_master (s_con);
	if (!master)
		return;

	interface_name = nm_connection_get_virtual_iface_name (CE_PAGE (self)->connection);
	if (strcmp (master, interface_name) != 0 && strcmp (master, priv->uuid) != 0)
		return;

	check_new_slave_physical_port (self, NM_CONNECTION (connection));

	gtk_list_store_append (GTK_LIST_STORE (priv->connections_model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (priv->connections_model), &iter,
	                    COL_CONNECTION, connection,
	                    COL_NAME, nm_setting_connection_get_id (s_con),
	                    -1);
	ce_page_changed (CE_PAGE (self));

	g_signal_connect (connection, NM_REMOTE_CONNECTION_REMOVED,
	                  G_CALLBACK (connection_removed), self);
	g_signal_connect (connection, NM_REMOTE_CONNECTION_UPDATED,
	                  G_CALLBACK (connection_updated), self);

	g_signal_emit (self, signals[CONNECTION_ADDED], 0, connection);
}

static void
connections_selection_changed_cb (GtkTreeSelection *selection, gpointer user_data)
{
	CEPageMaster *self = user_data;
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	GtkTreeIter iter;
	GtkTreeModel *model;
	NMRemoteConnection *connection;
	NMSettingConnection *s_con;
	gboolean sensitive = FALSE;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter,
		                    0, &connection,
		                    -1);
		s_con = nm_connection_get_setting_connection (NM_CONNECTION (connection));
		g_assert (s_con);

		sensitive = !nm_setting_connection_get_read_only (s_con);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (priv->edit), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->delete), sensitive);
}

static void
add_response_cb (NMConnectionEditor *editor, GtkResponseType response, gpointer user_data)
{
	CEPageMaster *self = user_data;
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	const char *uuid;

	if (response == GTK_RESPONSE_OK) {
		uuid = nm_connection_get_uuid (editor->connection);
		g_hash_table_add (priv->new_slaves, g_strdup (uuid));
	}
	g_object_unref (editor);
}

static void
add_connection (NMConnection *connection,
                gpointer user_data)
{
	CEPageMaster *self = user_data;
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	NMConnectionEditor *editor;
	const char *iface_name, *master_type;
	char *name;

	if (!connection)
		return;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	master_type = nm_setting_connection_get_connection_type (s_con);

	/* Mark the connection as a slave, and rename it. */
	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con != NULL);

	iface_name = gtk_entry_get_text (priv->interface_name);
	if (!*iface_name)
		iface_name = nm_connection_get_virtual_iface_name (connection);
	if (!iface_name || !nm_utils_iface_valid_name (iface_name))
		iface_name = nm_connection_get_id (connection);
	name = g_strdup_printf (_("%s slave %d"), iface_name,
	                        gtk_tree_model_iter_n_children (priv->connections_model, NULL) + 1);

	g_object_set (G_OBJECT (s_con),
	              NM_SETTING_CONNECTION_ID, name,
	              NM_SETTING_CONNECTION_MASTER, priv->uuid,
	              NM_SETTING_CONNECTION_SLAVE_TYPE, master_type,
	              NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
	              NULL);
	g_free (name);

	g_signal_emit (self, signals[CREATE_CONNECTION], 0, connection);

	editor = nm_connection_editor_new (priv->toplevel,
	                                   connection,
	                                   CE_PAGE (self)->client,
	                                   CE_PAGE (self)->settings);
	if (!editor) {
		g_object_unref (connection);
		return;
	}

	g_signal_connect (editor, "done", G_CALLBACK (add_response_cb), self);
	nm_connection_editor_run (editor);
}

static void
add_clicked (GtkButton *button, gpointer user_data)
{
	CEPageMaster *self = user_data;

	CE_PAGE_MASTER_GET_CLASS (self)->add_slave (self, add_connection);
}

static NMRemoteConnection *
get_selected_connection (CEPageMaster *self)
{
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	GtkTreeSelection *selection;
	GList *selected_rows;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	NMRemoteConnection *connection = NULL;

	selection = gtk_tree_view_get_selection (priv->connections);
	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);
	if (!selected_rows)
		return NULL;

	if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) selected_rows->data))
		gtk_tree_model_get (model, &iter, 0, &connection, -1);

	/* free memory */
	g_list_foreach (selected_rows, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected_rows);

	return connection;
}

static void
edit_done_cb (NMConnectionEditor *editor, GtkResponseType response, gpointer user_data)
{
	g_object_unref (editor);
}

static void
edit_clicked (GtkButton *button, gpointer user_data)
{
	CEPageMaster *self = user_data;
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	NMConnectionEditor *editor;
	NMRemoteConnection *connection;

	connection = get_selected_connection (self);
	if (!connection)
		return;

	editor = nm_connection_editor_get (NM_CONNECTION (connection));
	if (editor) {
		nm_connection_editor_present (editor);
		return;
	}

	editor = nm_connection_editor_new (priv->toplevel,
	                                   NM_CONNECTION (connection),
	                                   CE_PAGE (self)->client,
	                                   CE_PAGE (self)->settings);
	if (!editor)
		return;

	g_signal_connect (editor, "done", G_CALLBACK (edit_done_cb), self);
	nm_connection_editor_run (editor);
}

static void
connection_double_clicked_cb (GtkTreeView *tree_view,
                              GtkTreePath *path,
                              GtkTreeViewColumn *column,
                              gpointer user_data)
{
	edit_clicked (NULL, user_data);
}

static void
delete_result_cb (NMRemoteConnection *connection,
                  gboolean deleted,
                  gpointer user_data)
{
	CEPageMaster *self = user_data;
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);

	if (deleted) {
		g_hash_table_remove (priv->new_slaves,
		                     nm_connection_get_uuid (NM_CONNECTION (connection)));
	}
}

static void
delete_clicked (GtkButton *button, gpointer user_data)
{
	CEPageMaster *self = user_data;
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	NMRemoteConnection *connection;

	connection = get_selected_connection (self);
	if (!connection)
		return;

	delete_connection (priv->toplevel, connection, delete_result_cb, self);
}

static void
populate_ui (CEPageMaster *self)
{
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	const char *iface;
	GSList *connections, *c;

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	g_return_if_fail (s_con != NULL);

	/* Interface name */
	iface = nm_connection_get_virtual_iface_name (CE_PAGE (self)->connection);
	gtk_entry_set_text (priv->interface_name, iface ? iface : "");

	/* Slave connections */
	connections = nm_remote_settings_list_connections (CE_PAGE (self)->settings);
	for (c = connections; c; c = c->next)
		connection_added (CE_PAGE (self)->settings, c->data, self);
}

static void
finish_setup (CEPageMaster *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	GtkTreeSelection *selection;
	GtkBuilder *builder;
	NMSettingConnection *s_con;

	if (error)
		return;

	builder = CE_PAGE (self)->builder;

	priv->interface_name = GTK_ENTRY (gtk_builder_get_object (builder, "master_interface"));

	priv->connections = GTK_TREE_VIEW (gtk_builder_get_object (builder, "master_connections"));
	priv->connections_model = GTK_TREE_MODEL (gtk_builder_get_object (builder, "master_connections_model"));
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->connections_model),
	                                 COL_NAME, name_sort_func,
	                                 NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->connections_model),
	                                      COL_NAME, GTK_SORT_ASCENDING);

	priv->add = GTK_BUTTON (gtk_builder_get_object (builder, "master_connection_add"));
	priv->edit = GTK_BUTTON (gtk_builder_get_object (builder, "master_connection_edit"));
	priv->delete = GTK_BUTTON (gtk_builder_get_object (builder, "master_connection_delete"));

	priv->toplevel = GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (priv->connections),
	                                                      GTK_TYPE_WINDOW));

	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	priv->uuid = nm_setting_connection_get_uuid (s_con);

	populate_ui (self);

	g_signal_connect (CE_PAGE (self)->settings, NM_REMOTE_SETTINGS_NEW_CONNECTION,
	                  G_CALLBACK (connection_added), self);

	g_signal_connect (priv->interface_name, "changed", G_CALLBACK (stuff_changed), self);

	g_signal_connect (priv->add, "clicked", G_CALLBACK (add_clicked), self);
	g_signal_connect (priv->edit, "clicked", G_CALLBACK (edit_clicked), self);
	g_signal_connect (priv->delete, "clicked", G_CALLBACK (delete_clicked), self);

	g_signal_connect (priv->connections, "row-activated", G_CALLBACK (connection_double_clicked_cb), self);

	selection = gtk_tree_view_get_selection (priv->connections);
	g_signal_connect (selection, "changed", G_CALLBACK (connections_selection_changed_cb), self);
	connections_selection_changed_cb (selection, self);
}

static void
ui_to_setting (CEPageMaster *self)
{
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	const char *connection_type;
	NMSetting *setting;
	const char *interface_name;

	/* Find the "toplevel" NMSetting for this connection */
	s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
	connection_type = nm_setting_connection_get_connection_type (s_con);
	setting = nm_connection_get_setting_by_name (CE_PAGE (self)->connection, connection_type);

	/* Interface name */
	interface_name = gtk_entry_get_text (priv->interface_name);
	g_object_set (setting,
	              "interface-name", interface_name,
	              NULL);

	/* Slaves are updated as they're edited, so nothing to do */
}

static gboolean
ce_page_validate_v (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageMaster *self = CE_PAGE_MASTER (page);

	/* We don't need to recursively check that the slaves connections
	 * are valid because they can't end up in the table if they're not.
	 */

	ui_to_setting (self);

	/* Subtype ce_page_validate_v() method will validate the interface name */
	return TRUE;
}

static gboolean
last_update (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageMaster *self = CE_PAGE_MASTER (page);
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	const char *interface_name, *tmp, *uuid;
	NMSettingConnection *s_con;
	GtkTreeIter iter;

	/* No new slave added - leave master property as it is. */
	if (g_hash_table_size (priv->new_slaves) == 0)
		return TRUE;

	/*
	 * Set master property of all slaves to be the interface name.
	 * Even if UUID has the advantage of being stable and thus easier to use,
	 * users may prefer using interface name instead.
	*/
	interface_name = gtk_entry_get_text (priv->interface_name);
	if (gtk_tree_model_get_iter_first (priv->connections_model, &iter)) {
		do {
			NMRemoteConnection *rcon = NULL;

			gtk_tree_model_get (priv->connections_model, &iter,
			                    COL_CONNECTION, &rcon,
			                    -1);
			tmp = nm_connection_get_interface_name (NM_CONNECTION (rcon));
			uuid = nm_connection_get_uuid (NM_CONNECTION (rcon));
			if (   g_hash_table_contains (priv->new_slaves, uuid)
			    && g_strcmp0 (interface_name, tmp) != 0) {
				s_con = nm_connection_get_setting_connection (NM_CONNECTION (rcon));
				g_object_set (s_con, NM_SETTING_CONNECTION_MASTER, interface_name, NULL);
				nm_remote_connection_commit_changes (rcon, NULL, NULL);
			}
			g_object_unref (rcon);
		} while (gtk_tree_model_iter_next (priv->connections_model, &iter));
	}
	return TRUE;
}

static void
ce_page_master_init (CEPageMaster *self)
{
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);

	priv->new_slaves = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
ce_page_master_class_init (CEPageMasterClass *master_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (master_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (master_class);

	g_type_class_add_private (object_class, sizeof (CEPageMasterPrivate));

	/* virtual methods */
	object_class->constructed = constructed;
	object_class->dispose = dispose;

	parent_class->ce_page_validate_v = ce_page_validate_v;
	parent_class->last_update = last_update;

	/* Signals */
	signals[CREATE_CONNECTION] = 
		g_signal_new ("create-connection",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_FIRST,
		              G_STRUCT_OFFSET (CEPageMasterClass, create_connection),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              NM_TYPE_CONNECTION);

	signals[CONNECTION_ADDED] = 
		g_signal_new ("connection-added",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_FIRST,
		              G_STRUCT_OFFSET (CEPageMasterClass, connection_added),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              NM_TYPE_CONNECTION);

	signals[CONNECTION_REMOVED] = 
		g_signal_new ("connection-removed",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_FIRST,
		              G_STRUCT_OFFSET (CEPageMasterClass, connection_removed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              NM_TYPE_CONNECTION);
}

gboolean
ce_page_master_has_slaves (CEPageMaster *self)
{
	CEPageMasterPrivate *priv = CE_PAGE_MASTER_GET_PRIVATE (self);
	GtkTreeIter iter;

	return gtk_tree_model_get_iter_first (priv->connections_model, &iter);
}
