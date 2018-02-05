/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Rodrigo Moya <rodrigo@gnome-db.org>
 * Dan Williams <dcbw@redhat.com>
 * Lubomir Rintel <lkundrak@v3.sk>
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
 * Copyright 2007 - 2017 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <gdk/gdkx.h>

#include "ce-page.h"
#include "nm-connection-editor.h"
#include "nm-connection-list.h"
#include "ce-polkit.h"
#include "connection-helpers.h"

extern gboolean nm_ce_keep_above;

enum {
	LIST_DONE,
	EDITING_DONE,
	LIST_LAST_SIGNAL
};

static guint list_signals[LIST_LAST_SIGNAL] = { 0 };

struct _NMConnectionListPrivate {
	GtkWidget *connection_add;
	GtkWidget *connection_del;
	GtkWidget *connection_edit;
	GtkTreeView *connection_list;
	GtkSearchBar *search_bar;
	GtkEntry *search_entry;
	GtkTreeModel *model;
	GtkTreeModelFilter *filter;
	GtkTreeSortable *sortable;
	GType displayed_type;

	NMClient *client;

	gboolean populated;
};

#define NM_CONNECTION_LIST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                           NM_TYPE_CONNECTION_LIST, \
                                           NMConnectionListPrivate))

G_DEFINE_TYPE_WITH_CODE (NMConnectionList, nm_connection_list, GTK_TYPE_WINDOW,
                         G_ADD_PRIVATE (NMConnectionList))

#define COL_ID         0
#define COL_LAST_USED  1
#define COL_TIMESTAMP  2
#define COL_CONNECTION 3
#define COL_GTYPE0     4
#define COL_GTYPE1     5
#define COL_GTYPE2     6
#define COL_ORDER      7

static NMRemoteConnection *
get_active_connection (GtkTreeView *treeview)
{
	GtkTreeSelection *selection;
	GList *selected_rows;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	NMRemoteConnection *connection = NULL;

	selection = gtk_tree_view_get_selection (treeview);
	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);
	if (!selected_rows)
		return NULL;

	if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) selected_rows->data))
		gtk_tree_model_get (model, &iter, COL_CONNECTION, &connection, -1);

	/* free memory */
	g_list_foreach (selected_rows, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected_rows);

	/* gtk_tree_model_get() will have reffed connection, but we don't
	 * need that since we know the model will continue to hold a ref.
	 */
	if (connection)
		g_object_unref (connection);

	return connection;
}

static gboolean
get_iter_for_connection (NMConnectionList *list,
                         NMRemoteConnection *connection,
                         GtkTreeIter *iter)
{
	GtkTreeIter types_iter;
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);

	if (!gtk_tree_model_get_iter_first (priv->model, &types_iter))
		return FALSE;

	do {
		if (!gtk_tree_model_iter_children (priv->model, iter, &types_iter))
			continue;

		do {
			NMRemoteConnection *candidate = NULL;

			gtk_tree_model_get (priv->model, iter,
			                    COL_CONNECTION, &candidate,
			                    -1);
			if (candidate == connection) {
				g_object_unref (candidate);
				return TRUE;
			}
			g_object_unref (candidate);
		} while (gtk_tree_model_iter_next (priv->model, iter));
	} while (gtk_tree_model_iter_next (priv->model, &types_iter));

	return FALSE;
}

static char *
format_last_used (guint64 timestamp)
{
	GTimeVal now_tv;
	GDate *now, *last;
	char *last_used = NULL;

	if (!timestamp)
		return g_strdup (_("never"));

	g_get_current_time (&now_tv);
	now = g_date_new ();
	g_date_set_time_val (now, &now_tv);

	last = g_date_new ();
	g_date_set_time_t (last, (time_t) timestamp);

	/* timestamp is now or in the future */
	if (now_tv.tv_sec <= timestamp) {
		last_used = g_strdup (_("now"));
		goto out;
	}

	if (g_date_compare (now, last) <= 0) {
		guint minutes, hours;

		/* Same day */

		minutes = (now_tv.tv_sec - timestamp) / 60;
		if (minutes == 0) {
			last_used = g_strdup (_("now"));
			goto out;
		}

		hours = (now_tv.tv_sec - timestamp) / 3600;
		if (hours == 0) {
			/* less than an hour ago */
			last_used = g_strdup_printf (ngettext ("%d minute ago", "%d minutes ago", minutes), minutes);
			goto out;
		}

		last_used = g_strdup_printf (ngettext ("%d hour ago", "%d hours ago", hours), hours);
	} else {
		guint days, months, years;

		days = g_date_get_julian (now) - g_date_get_julian (last);
		if (days == 0) {
			last_used = g_strdup ("today");
			goto out;
		}

		months = days / 30;
		if (months == 0) {
			last_used = g_strdup_printf (ngettext ("%d day ago", "%d days ago", days), days);
			goto out;
		}

		years = days / 365;
		if (years == 0) {
			last_used = g_strdup_printf (ngettext ("%d month ago", "%d months ago", months), months);
			goto out;
		}

		last_used = g_strdup_printf (ngettext ("%d year ago", "%d years ago", years), years);
	}

out:
	g_date_free (now);
	g_date_free (last);
	return last_used;
}

static void
update_connection_row (NMConnectionList *self,
                       GtkTreeIter *iter,
                       NMRemoteConnection *connection)
{
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (self);
	NMSettingConnection *s_con;
	char *last_used, *id;

	s_con = nm_connection_get_setting_connection (NM_CONNECTION (connection));
	g_assert (s_con);

	last_used = format_last_used (nm_setting_connection_get_timestamp (s_con));
	id = g_markup_escape_text (nm_setting_connection_get_id (s_con), -1);
	gtk_tree_store_set (GTK_TREE_STORE (priv->model), iter,
	                    COL_ID, id,
	                    COL_LAST_USED, last_used,
	                    COL_TIMESTAMP, nm_setting_connection_get_timestamp (s_con),
	                    COL_CONNECTION, connection,
	                    -1);
	g_free (last_used);
	g_free (id);

	gtk_tree_model_filter_refilter (priv->filter);
}

static void
delete_slaves_of_connection (NMConnectionList *list, NMConnection *connection)
{
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);
	const char *uuid, *iface;
	GtkTreeIter iter, types_iter;

	if (!gtk_tree_model_get_iter_first (priv->model, &types_iter))
		return;

	uuid = nm_connection_get_uuid (connection);
	iface = nm_connection_get_interface_name (connection);

	do {
		if (!gtk_tree_model_iter_children (priv->model, &iter, &types_iter))
			continue;

		do {
			NMRemoteConnection *candidate = NULL;
			NMSettingConnection *s_con;
			const char *master;

			gtk_tree_model_get (priv->model, &iter,
			                    COL_CONNECTION, &candidate,
			                    -1);
			s_con = nm_connection_get_setting_connection (NM_CONNECTION (candidate));
			master = nm_setting_connection_get_master (s_con);
			if (master) {
				if (!g_strcmp0 (master, uuid) || !g_strcmp0 (master, iface))
					nm_remote_connection_delete (candidate, NULL, NULL);
			}

			g_object_unref (candidate);
		} while (gtk_tree_model_iter_next (priv->model, &iter));
	} while (gtk_tree_model_iter_next (priv->model, &types_iter));
}


/**********************************************/
/* dialog/UI handling stuff */

static void
add_response_cb (NMConnectionEditor *editor, GtkResponseType response, gpointer user_data)
{
	NMConnectionList *list = user_data;

	if (response == GTK_RESPONSE_CANCEL)
		delete_slaves_of_connection (list, nm_connection_editor_get_connection (editor));

	g_object_unref (editor);
	g_signal_emit (list, list_signals[EDITING_DONE], 0, 0);
}

static void
really_add_connection (FUNC_TAG_NEW_CONNECTION_RESULT_IMPL,
                       NMConnection *connection,
                       gpointer user_data)
{
	NMConnectionList *list = user_data;
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);
	NMConnectionEditor *editor;

	if (!connection) {
		g_signal_emit (list, list_signals[EDITING_DONE], 0, 0);
		return;
	}

	if (connection_supports_proxy (connection) && !nm_connection_get_setting_proxy (connection))
		nm_connection_add_setting (connection, nm_setting_proxy_new ());
	if (connection_supports_ip4 (connection) && !nm_connection_get_setting_ip4_config (connection))
		nm_connection_add_setting (connection, nm_setting_ip4_config_new ());
	if (connection_supports_ip6 (connection) && !nm_connection_get_setting_ip6_config (connection))
		nm_connection_add_setting (connection, nm_setting_ip6_config_new ());

	editor = nm_connection_editor_new (GTK_WINDOW (list), connection, priv->client);
	if (!editor) {
		g_signal_emit (list, list_signals[EDITING_DONE], 0, 0);
		return;
	}

	g_signal_connect (editor, "done", G_CALLBACK (add_response_cb), list);
	nm_connection_editor_run (editor);
}

static void
add_clicked (GtkButton *button, gpointer user_data)
{
	nm_connection_list_add (user_data);
}

void
nm_connection_list_add (NMConnectionList *list)
{
	NMConnectionListPrivate *priv;

	g_return_if_fail (NM_IS_CONNECTION_LIST (list));
	priv = NM_CONNECTION_LIST_GET_PRIVATE (list);

	new_connection_dialog (GTK_WINDOW (list),
	                       priv->client,
	                       NULL,
	                       really_add_connection,
	                       list);
}

static void
edit_done_cb (NMConnectionEditor *editor, GtkResponseType response, gpointer user_data)
{
	NMConnectionList *list = user_data;

	if (response == GTK_RESPONSE_OK) {
		NMRemoteConnection *connection = NM_REMOTE_CONNECTION (nm_connection_editor_get_connection (editor));
		GtkTreeIter iter;

		if (get_iter_for_connection (list, connection, &iter))
			update_connection_row (list, &iter, connection);
	}

	g_object_unref (editor);
	g_signal_emit (list, list_signals[EDITING_DONE], 0, 0);
}

static void
edit_connection (NMConnectionList *list, NMConnection *connection)
{
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);
	NMConnectionEditor *editor;

	g_return_if_fail (connection != NULL);

	/* Don't allow two editors for the same connection */
	editor = nm_connection_editor_get (connection);
	if (editor) {
		nm_connection_editor_present (editor);
		return;
	}

	editor = nm_connection_editor_new (GTK_WINDOW (list),
	                                   NM_CONNECTION (connection),
	                                   priv->client);
	if (editor) {
		g_signal_connect (editor, "done", G_CALLBACK (edit_done_cb), list);
		nm_connection_editor_run (editor);
	}
}

static void
do_edit (NMConnectionList *list)
{
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);

	if (gtk_widget_get_sensitive (priv->connection_edit))
		edit_connection (list, NM_CONNECTION (get_active_connection (priv->connection_list)));
}

static void
delete_connection_cb (FUNC_TAG_DELETE_CONNECTION_RESULT_IMPL,
                      NMRemoteConnection *connection,
                      gboolean deleted,
                      gpointer user_data)
{
	NMConnectionList *list = user_data;

	if (deleted)
		delete_slaves_of_connection (list, NM_CONNECTION (connection));
}

static void
delete_clicked (GtkButton *button, gpointer user_data)
{
	NMConnectionList *list = user_data;
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);
	NMRemoteConnection *connection;

	connection = get_active_connection (priv->connection_list);
	g_return_if_fail (connection != NULL);

	delete_connection (GTK_WINDOW (list), connection,
	                   delete_connection_cb, list);
}

static void
selection_changed_cb (GtkTreeSelection *selection, gpointer user_data)
{
	NMConnectionList *list = user_data;
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);
	GtkTreeIter iter;
	GtkTreeModel *model;
	NMRemoteConnection *connection = NULL;
	NMSettingConnection *s_con;
	gboolean sensitive = FALSE;

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		connection = get_active_connection (priv->connection_list);

	if (connection) {
		s_con = nm_connection_get_setting_connection (NM_CONNECTION (connection));
		g_assert (s_con);

		sensitive = !nm_setting_connection_get_read_only (s_con);

		ce_polkit_set_widget_validation_error (priv->connection_edit,
		                                       sensitive ? NULL : _("Connection cannot be modified"));
		ce_polkit_set_widget_validation_error (priv->connection_del,
		                                       sensitive ? NULL : _("Connection cannot be deleted"));
	} else {
		ce_polkit_set_widget_validation_error (priv->connection_edit,
		                                       _("Select a connection to edit"));
		ce_polkit_set_widget_validation_error (priv->connection_del,
		                                       _("Select a connection to delete"));
	}
}

static void
list_close_cb (GtkDialog *dialog, gpointer user_data)
{
	g_signal_emit (NM_CONNECTION_LIST (user_data),
	               list_signals[LIST_DONE],
	               0,
	               GTK_RESPONSE_CLOSE);
}

static gboolean
key_press_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	NMConnectionList *list = user_data;
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);
	GdkEventKey *key_event = (GdkEventKey *) event;

	if (gtk_search_bar_handle_event (priv->search_bar, event) == GDK_EVENT_STOP)
		return GDK_EVENT_STOP;

	if (key_event->keyval == GDK_KEY_Escape) {
		if (gtk_search_bar_get_search_mode (priv->search_bar))
			gtk_search_bar_set_search_mode (priv->search_bar, FALSE);
		else
			gtk_window_close (GTK_WINDOW (user_data));
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static gboolean
start_search (GtkTreeView *treeview, gpointer user_data)
{
	NMConnectionList *list = user_data;
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);

	gtk_search_bar_set_search_mode (priv->search_bar, TRUE);

	return TRUE;
}

static void
search_changed (GtkSearchEntry *entry, gpointer user_data)
{
	NMConnectionList *list = user_data;
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);

	gtk_tree_model_filter_refilter (priv->filter);
	gtk_tree_view_expand_all (priv->connection_list);
}

static void
nm_connection_list_init (NMConnectionList *list)
{
	gtk_widget_init_template (GTK_WIDGET (list));
}

static void
dispose (GObject *object)
{
	NMConnectionList *list = NM_CONNECTION_LIST (object);
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);

	g_clear_object (&priv->client);

	G_OBJECT_CLASS (nm_connection_list_parent_class)->dispose (object);
}

static void
nm_connection_list_class_init (NMConnectionListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	/* virtual methods */
	object_class->dispose = dispose;

	/* Signals */
	list_signals[LIST_DONE] =
		g_signal_new ("done",
					  G_OBJECT_CLASS_TYPE (object_class),
					  G_SIGNAL_RUN_FIRST,
					  G_STRUCT_OFFSET (NMConnectionListClass, done),
		              NULL, NULL, NULL,
					  G_TYPE_NONE, 1, G_TYPE_INT);

	list_signals[EDITING_DONE] =
		g_signal_new ("editing-done",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_FIRST,
		              G_STRUCT_OFFSET (NMConnectionListClass, done),
		              NULL, NULL, NULL,
		              G_TYPE_NONE, 1, G_TYPE_INT);

	/* Initialize the widget template */
        gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/freedesktop/network-manager-applet/nm-connection-list.ui");

        gtk_widget_class_bind_template_child_private (widget_class, NMConnectionList, connection_list);
        gtk_widget_class_bind_template_child_private (widget_class, NMConnectionList, connection_add);
        gtk_widget_class_bind_template_child_private (widget_class, NMConnectionList, connection_del);
        gtk_widget_class_bind_template_child_private (widget_class, NMConnectionList, connection_edit);
        gtk_widget_class_bind_template_child_private (widget_class, NMConnectionList, search_bar);
        gtk_widget_class_bind_template_child_private (widget_class, NMConnectionList, search_entry);

        gtk_widget_class_bind_template_callback (widget_class, add_clicked);
        gtk_widget_class_bind_template_callback (widget_class, do_edit);
        gtk_widget_class_bind_template_callback (widget_class, delete_clicked);
        gtk_widget_class_bind_template_callback (widget_class, list_close_cb);
        gtk_widget_class_bind_template_callback (widget_class, selection_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, key_press_cb);
        gtk_widget_class_bind_template_callback (widget_class, start_search);
        gtk_widget_class_bind_template_callback (widget_class, search_changed);
}

static void
column_header_clicked_cb (GtkTreeViewColumn *treeviewcolumn, gpointer user_data)
{
	gint sort_col_id = GPOINTER_TO_INT (user_data);

	gtk_tree_view_column_set_sort_column_id (treeviewcolumn, sort_col_id);
}

static gint
sort_connection_types (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	GtkTreeSortable *sortable = user_data;
	int order_a, order_b;
	GtkSortType order;

	gtk_tree_model_get (model, a, COL_ORDER, &order_a, -1);
	gtk_tree_model_get (model, b, COL_ORDER, &order_b, -1);

	/* The connection types should stay in the same order regardless of whether
	 * the table is sorted ascending or descending.
	 */
	gtk_tree_sortable_get_sort_column_id (sortable, NULL, &order);
	if (order == GTK_SORT_ASCENDING)
		return order_a - order_b;
	else
		return order_b - order_a;
}

static gint
id_sort_func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	NMConnection *conn_a, *conn_b;
	gint ret;

	gtk_tree_model_get (model, a, COL_CONNECTION, &conn_a, -1);
	gtk_tree_model_get (model, b, COL_CONNECTION, &conn_b, -1);

	if (!conn_a || !conn_b) {
		g_assert (!conn_a && !conn_b);
		return sort_connection_types (model, a, b, user_data);
	}

	ret = strcmp (nm_connection_get_id (conn_a), nm_connection_get_id (conn_b));
	g_object_unref (conn_a);
	g_object_unref (conn_b);

	return ret;
}

static gint
timestamp_sort_func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	NMConnection *conn_a, *conn_b;
	guint64 time_a, time_b;

	gtk_tree_model_get (model, a,
	                    COL_CONNECTION, &conn_a,
	                    COL_TIMESTAMP, &time_a,
	                    -1);
	gtk_tree_model_get (model, b,
	                    COL_CONNECTION, &conn_b,
	                    COL_TIMESTAMP, &time_b,
	                    -1);

	if (!conn_a || !conn_b) {
		g_assert (!conn_a && !conn_b);
		return sort_connection_types (model, a, b, user_data);
	}

	g_object_unref (conn_a);
	g_object_unref (conn_b);

	return time_b - time_a;
}

static gboolean
has_visible_children (NMConnectionList *self, GtkTreeModel *model, GtkTreeIter *parent)
{
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (self);
	GtkTreeIter iter;
	const char *search;
	char *id = NULL;

	if (!gtk_search_bar_get_search_mode (priv->search_bar))
		return gtk_tree_model_iter_has_child  (model, parent);

	if (!gtk_tree_model_iter_children (model, &iter, parent))
		return FALSE;

	search = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));
	do {
		gtk_tree_model_get (model, &iter, COL_ID, &id, -1);
		if (strcasestr (id, search) != NULL) {
			g_free (id);
			return TRUE;
		}
		g_free (id);
	} while (gtk_tree_model_iter_next (model, &iter));

	return FALSE;
}

static gboolean
tree_model_visible_func (GtkTreeModel *model,
                         GtkTreeIter *iter,
                         gpointer user_data)
{
	NMConnectionList *self = user_data;
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (self);
	gs_unref_object NMConnection *connection = NULL;
	NMSettingConnection *s_con;
	const char *master;
	const char *slave_type;
	gs_free char *id = NULL;

	gtk_tree_model_get (model, iter,
	                    COL_ID, &id,
	                    COL_CONNECTION, &connection,
	                    -1);
	if (!connection) {
		/* Top-level type nodes are visible iff they have visible children */
		return has_visible_children (self, model, iter);
	}

	if (   gtk_search_bar_get_search_mode (priv->search_bar)
	    && strcasestr (id, gtk_entry_get_text (GTK_ENTRY (priv->search_entry))) == NULL)
		return FALSE;

	/* A connection node is visible unless it is a slave to a known
	 * bond or team or bridge.
	 */
	s_con = nm_connection_get_setting_connection (connection);
	if (   !s_con
	    || !nm_remote_connection_get_visible (NM_REMOTE_CONNECTION (connection)))
		return FALSE;

	master = nm_setting_connection_get_master (s_con);
	if (!master)
		return TRUE;
	slave_type = nm_setting_connection_get_slave_type (s_con);
	if (   g_strcmp0 (slave_type, NM_SETTING_BOND_SETTING_NAME) != 0
	    && g_strcmp0 (slave_type, NM_SETTING_TEAM_SETTING_NAME) != 0
	    && g_strcmp0 (slave_type, NM_SETTING_BRIDGE_SETTING_NAME) != 0)
		return TRUE;

	if (nm_client_get_connection_by_uuid (priv->client, master))
		return FALSE;
	if (nm_connection_editor_get_master (connection))
		return FALSE;

	/* FIXME: what if master is an interface name */

	return TRUE;
}

static gboolean
connection_list_equal (GtkTreeModel *model, gint column, const gchar *key,
                       GtkTreeIter *iter, gpointer user_data)
{
	gs_free char *id = NULL;
	gs_unref_object NMConnection *connection = NULL;

	gtk_tree_model_get (model, iter,
	                    COL_ID, &id,
	                    COL_CONNECTION, &connection,
	                    -1);

	if (!connection)
		return TRUE;

	return strcasestr (id, key) == NULL;
}

static void
initialize_treeview (NMConnectionList *self)
{
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (self);
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	ConnectionTypeData *types;
	GtkTreeIter iter;
	char *id, *tmp;
	int i;

	/* Model */
	priv->model = GTK_TREE_MODEL (gtk_tree_store_new (8, G_TYPE_STRING,
	                                                     G_TYPE_STRING,
	                                                     G_TYPE_UINT64,
	                                                     G_TYPE_OBJECT,
	                                                     G_TYPE_GTYPE,
	                                                     G_TYPE_GTYPE,
	                                                     G_TYPE_GTYPE,
	                                                     G_TYPE_INT));

	/* Filter */
	priv->filter = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (priv->model, NULL));
	gtk_tree_model_filter_set_visible_func (priv->filter,
	                                        tree_model_visible_func,
	                                        self, NULL);

	/* Sortable */
	priv->sortable = GTK_TREE_SORTABLE (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (priv->filter)));
	gtk_tree_sortable_set_default_sort_func (priv->sortable, NULL, NULL, NULL);
	gtk_tree_sortable_set_sort_func (priv->sortable, COL_TIMESTAMP, timestamp_sort_func,
	                                 priv->sortable, NULL);
	gtk_tree_sortable_set_sort_func (priv->sortable, COL_ID, id_sort_func,
	                                 priv->sortable, NULL);
	gtk_tree_sortable_set_sort_column_id (priv->sortable, COL_TIMESTAMP, GTK_SORT_ASCENDING);

	gtk_tree_view_set_model (priv->connection_list, GTK_TREE_MODEL (priv->sortable));
	gtk_tree_view_set_search_equal_func (priv->connection_list, connection_list_equal, NULL, NULL);
	gtk_tree_view_set_search_entry (priv->connection_list, priv->search_entry);

	/* Name column */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Name"),
	                                                   renderer,
	                                                   "markup", COL_ID,
	                                                   NULL);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, COL_ID);
	g_signal_connect (column, "clicked", G_CALLBACK (column_header_clicked_cb), GINT_TO_POINTER (COL_ID));
	gtk_tree_view_append_column (priv->connection_list, column);

	/* Last Used column */
	renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
	                         "foreground", "SlateGray",
	                         NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Last Used"),
	                                                   renderer,
	                                                   "text", COL_LAST_USED,
	                                                   NULL);
	gtk_tree_view_column_set_sort_column_id (column, COL_TIMESTAMP);
	g_signal_connect (column, "clicked", G_CALLBACK (column_header_clicked_cb), GINT_TO_POINTER (COL_TIMESTAMP));
	gtk_tree_view_append_column (priv->connection_list, column);

	/* Selection */
	selection = gtk_tree_view_get_selection (priv->connection_list);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	/* Fill in connection types */
	types = get_connection_type_list ();
	for (i = 0; types[i].name; i++) {

		tmp = g_markup_escape_text (types[i].name, -1);
		id = g_strdup_printf ("<b>%s</b>", tmp);
		g_free (tmp);

		gtk_tree_store_append (GTK_TREE_STORE (priv->model), &iter, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (priv->model), &iter,
		                    COL_ID, id,
		                    COL_GTYPE0, types[i].setting_types[0],
		                    COL_GTYPE1, types[i].setting_types[1],
		                    COL_GTYPE2, types[i].setting_types[2],
		                    COL_ORDER, i,
		                    -1);
		g_free (id);
	}
}

static void
add_connection_buttons (NMConnectionList *self)
{
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (self);

	ce_polkit_connect_widget (priv->connection_edit,
	                          _("Edit the selected connection"),
	                          _("Authenticate to edit the selected connection"),
	                          priv->client,
	                          NM_CLIENT_PERMISSION_SETTINGS_MODIFY_SYSTEM);

	ce_polkit_connect_widget (priv->connection_del,
	                          _("Delete the selected connection"),
	                          _("Authenticate to delete the selected connection"),
	                          priv->client,
	                          NM_CLIENT_PERMISSION_SETTINGS_MODIFY_SYSTEM);
}

static void
connection_removed (NMClient *client,
                    NMRemoteConnection *connection,
                    gpointer user_data)
{
	NMConnectionList *self = NM_CONNECTION_LIST (user_data);
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (self);
	GtkTreeIter iter, parent_iter;

	if (get_iter_for_connection (self, connection, &iter)) {
		gtk_tree_model_iter_parent (priv->model, &parent_iter, &iter);
		gtk_tree_store_remove (GTK_TREE_STORE (priv->model), &iter);
	}
	gtk_tree_model_filter_refilter (priv->filter);
}

static void
connection_changed (NMRemoteConnection *connection, gpointer user_data)
{
	NMConnectionList *self = NM_CONNECTION_LIST (user_data);
	GtkTreeIter iter;

	if (   !nm_remote_connection_get_visible (connection)
	    || !nm_connection_get_setting_connection (NM_CONNECTION (connection))) {
		return;
	}

	if (get_iter_for_connection (self, connection, &iter))
		update_connection_row (self, &iter, connection);
}

static gboolean
get_parent_iter_for_connection (NMConnectionList *list,
                                NMRemoteConnection *connection,
                                GtkTreeIter *iter)
{
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (list);
	NMSettingConnection *s_con;
	const char *str_type;
	GType type, row_type0, row_type1, row_type2;

	s_con = nm_connection_get_setting_connection (NM_CONNECTION (connection));
	g_assert (s_con);
	str_type = nm_setting_connection_get_connection_type (s_con);
	if (!str_type) {
		g_warning ("Ignoring incomplete connection");
		return FALSE;
	}

	type = nm_setting_lookup_type (str_type);

	if (gtk_tree_model_get_iter_first (priv->model, iter)) {
		do {
			gtk_tree_model_get (priv->model, iter,
			                    COL_GTYPE0, &row_type0,
			                    COL_GTYPE1, &row_type1,
			                    COL_GTYPE2, &row_type2,
			                    -1);
			if (row_type0 == type || row_type1 == type || row_type2 == type)
				return TRUE;
		} while (gtk_tree_model_iter_next (priv->model, iter));
	}

	return FALSE;
}

static void
connection_added (NMClient *client,
                  NMRemoteConnection *connection,
                  gpointer user_data)
{
	NMConnectionList *self = NM_CONNECTION_LIST (user_data);
	NMConnectionListPrivate *priv = NM_CONNECTION_LIST_GET_PRIVATE (self);
	GtkTreeIter parent_iter, iter;
	NMSettingConnection *s_con;
	char *last_used, *id;
	gboolean expand = TRUE;

	if (!get_parent_iter_for_connection (self, connection, &parent_iter))
		return;

	s_con = nm_connection_get_setting_connection (NM_CONNECTION (connection));

	last_used = format_last_used (nm_setting_connection_get_timestamp (s_con));

	id = g_markup_escape_text (nm_setting_connection_get_id (s_con), -1);

	gtk_tree_store_append (GTK_TREE_STORE (priv->model), &iter, &parent_iter);
	gtk_tree_store_set (GTK_TREE_STORE (priv->model), &iter,
	                    COL_ID, id,
	                    COL_LAST_USED, last_used,
	                    COL_TIMESTAMP, nm_setting_connection_get_timestamp (s_con),
	                    COL_CONNECTION, connection,
	                    -1);

	g_free (id);
	g_free (last_used);

	if (priv->displayed_type) {
		GType added_type0, added_type1, added_type2;

		gtk_tree_model_get (priv->model, &parent_iter,
		                    COL_GTYPE0, &added_type0,
		                    COL_GTYPE1, &added_type1,
		                    COL_GTYPE2, &added_type2,
		                    -1);
		if (   added_type0 != priv->displayed_type
		    && added_type1 != priv->displayed_type
		    && added_type2 != priv->displayed_type)
			expand = FALSE;
	}

	if (expand) {
		GtkTreePath *path, *filtered_path;

		path = gtk_tree_model_get_path (priv->model, &parent_iter);
		filtered_path = gtk_tree_model_filter_convert_child_path_to_path (priv->filter, path);
		gtk_tree_view_expand_row (priv->connection_list, filtered_path, FALSE);
		gtk_tree_path_free (filtered_path);
		gtk_tree_path_free (path);
	}

	g_signal_connect (client, NM_CLIENT_CONNECTION_REMOVED, G_CALLBACK (connection_removed), self);
	g_signal_connect (connection, NM_CONNECTION_CHANGED, G_CALLBACK (connection_changed), self);
	gtk_tree_model_filter_refilter (priv->filter);
}

NMConnectionList *
nm_connection_list_new (void)
{
	NMConnectionList *list;
	NMConnectionListPrivate *priv;
	GError *error = NULL;

	list = g_object_new (NM_TYPE_CONNECTION_LIST, NULL);
	if (!list)
		return NULL;

	priv = NM_CONNECTION_LIST_GET_PRIVATE (list);

	gtk_window_set_default_icon_name ("preferences-system-network");

	priv->client = nm_client_new (NULL, &error);
	if (!priv->client) {
		g_warning ("Couldn't construct the client instance: %s", error->message);
		g_error_free (error);
		goto error;
	}
	g_signal_connect (priv->client,
	                  NM_CLIENT_CONNECTION_ADDED,
	                  G_CALLBACK (connection_added),
	                  list);

	add_connection_buttons (list);
	initialize_treeview (list);

	if (nm_ce_keep_above)
		gtk_window_set_keep_above (GTK_WINDOW (list), TRUE);

	return list;

error:
	g_object_unref (list);
	return NULL;
}

void
nm_connection_list_set_type (NMConnectionList *self, GType ctype)
{
	NMConnectionListPrivate *priv;

	g_return_if_fail (NM_IS_CONNECTION_LIST (self));
	priv = NM_CONNECTION_LIST_GET_PRIVATE (self);

	priv->displayed_type = ctype;
}

void
nm_connection_list_create (NMConnectionList *list,
                           GType ctype,
                           const char *detail,
                           const char *import_filename)
{
	NMConnectionListPrivate *priv;
	ConnectionTypeData *types;
	int i;

	g_return_if_fail (NM_IS_CONNECTION_LIST (list));
	priv = NM_CONNECTION_LIST_GET_PRIVATE (list);

	types = get_connection_type_list ();
	for (i = 0; types[i].name; i++) {
		if (   types[i].setting_types[0] == ctype
		    || types[i].setting_types[1] == ctype
		    || types[i].setting_types[2] == ctype)
			break;
	}

	if (!types[i].name) {
		if (ctype == NM_TYPE_SETTING_VPN) {
			nm_connection_editor_error (NULL, _("Error creating connection"),
			                            _("No VPN plugins are installed."));
		} else {
			nm_connection_editor_error (NULL, _("Error creating connection"),
			                            _("Don’t know how to create “%s” connections"), g_type_name (ctype));
		}
	} else {
		gs_unref_object NMConnection *connection = NULL;

		if (import_filename) {
			connection = vpn_connection_from_file (import_filename);
			if (!connection)
				return;
		}
		new_connection_of_type (GTK_WINDOW (list),
		                        detail,
		                        NULL,
		                        connection,
		                        priv->client,
		                        types[i].new_connection_func,
		                        really_add_connection,
		                        list);
	}
}

void
nm_connection_list_edit (NMConnectionList *self, const gchar *uuid)
{
	NMConnectionListPrivate *priv;
	NMConnection *connection;

	g_return_if_fail (NM_IS_CONNECTION_LIST (self));
	priv = NM_CONNECTION_LIST_GET_PRIVATE (self);

	connection = (NMConnection *) nm_client_get_connection_by_uuid (priv->client, uuid);
	if (!connection) {
		nm_connection_editor_error (NULL,
		                            _("Error editing connection"),
		                            _("Did not find a connection with UUID “%s”"), uuid);
		return;
	}

	edit_connection (self, connection);
}

void
nm_connection_list_present (NMConnectionList *list)
{
	NMConnectionListPrivate *priv;
	const GPtrArray *all_cons;
	GtkTreePath *path;
	GtkTreeIter iter;
	int i;

	g_return_if_fail (NM_IS_CONNECTION_LIST (list));
	priv = NM_CONNECTION_LIST_GET_PRIVATE (list);

	if (!priv->populated) {
		/* Fill the treeview initially */
		all_cons = nm_client_get_connections (priv->client);
		for (i = 0; i < all_cons->len; i++)
			connection_added (priv->client, all_cons->pdata[i], list);
		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->sortable), &iter)) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->sortable), &iter);
			gtk_tree_view_scroll_to_cell (priv->connection_list,
			                              path, NULL,
			                              FALSE, 0, 0);
			gtk_tree_path_free (path);
		}

		priv->populated = TRUE;
	}

	gtk_window_present (GTK_WINDOW (list));
}
