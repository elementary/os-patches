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
 * (C) Copyright 2008 - 2013 Red Hat, Inc.
 */

#include "config.h"

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <nm-utils.h>

#include "ip6-routes-dialog.h"
#include "utils.h"

#define COL_ADDRESS 0
#define COL_PREFIX  1
#define COL_NEXT_HOP 2
#define COL_METRIC  3
#define COL_LAST COL_METRIC

/* Variables to temporarily save last edited cell value
 * from routes treeview (cancelling issues) */
static char *last_edited = NULL; /* cell text */
static char *last_path = NULL;   /* row in treeview */
static int last_column = -1;     /* column in treeview */

static gboolean
get_one_int (GtkTreeModel *model,
             GtkTreeIter *iter,
             int column,
             guint32 max_value,
             gboolean fail_if_missing,
             guint *out)
{
	char *item = NULL;
	gboolean success = FALSE;
	long int tmp_int;

	gtk_tree_model_get (model, iter, column, &item, -1);
	if (!item || !strlen (item)) {
		g_free (item);
		return fail_if_missing ? FALSE : TRUE;
	}

	errno = 0;
	tmp_int = strtol (item, NULL, 10);
	if (errno || tmp_int < 0 || tmp_int > max_value)
		goto out;

	*out = (guint) tmp_int;
	success = TRUE;

out:
	g_free (item);
	return success;
}

static gboolean
get_one_addr (GtkTreeModel *model,
              GtkTreeIter *iter,
              int column,
              gboolean fail_if_missing,
              struct in6_addr *out)
{
	char *item = NULL;
	gboolean success = FALSE;

	gtk_tree_model_get (model, iter, column, &item, -1);
	if (!item || !strlen (item)) {
		g_free (item);
		return fail_if_missing ? FALSE : TRUE;
	}

	if (inet_pton (AF_INET6, item, out) > 0)
		success = TRUE;

	g_free (item);
	return success;
}

static void
validate (GtkWidget *dialog)
{
	GtkBuilder *builder;
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter tree_iter;
	gboolean valid = FALSE, iter_valid = FALSE;

	g_return_if_fail (dialog != NULL);

	builder = g_object_get_data (G_OBJECT (dialog), "builder");
	g_return_if_fail (builder != NULL);
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes"));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	iter_valid = gtk_tree_model_get_iter_first (model, &tree_iter);

	while (iter_valid) {
		struct in6_addr dest, next_hop;
		guint prefix = 0, metric = 0;

		/* Address */
		if (!get_one_addr (model, &tree_iter, COL_ADDRESS, TRUE, &dest))
			goto done;

		/* Prefix */
		if (!get_one_int (model, &tree_iter, COL_PREFIX, 128, TRUE, &prefix))
			goto done;

		/* Next hop (optional) */
		if (!get_one_addr (model, &tree_iter, COL_NEXT_HOP, FALSE, &next_hop))
			goto done;

		/* Metric (optional) */
		if (!get_one_int (model, &tree_iter, COL_METRIC, G_MAXUINT32, FALSE, &metric))
			goto done;

		iter_valid = gtk_tree_model_iter_next (model, &tree_iter);
	}
	valid = TRUE;

done:
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ok_button"));
	gtk_widget_set_sensitive (widget, valid);
}

static void
route_add_clicked (GtkButton *button, gpointer user_data)
{
	GtkBuilder *builder = GTK_BUILDER (user_data);
	GtkWidget *widget;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	GList *cells;

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes"));
	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, COL_ADDRESS, "", -1);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gtk_tree_selection_select_iter (selection, &iter);

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (widget), COL_ADDRESS);

	/* FIXME: using cells->data is pretty fragile but GTK apparently doesn't
	 * have a way to get a cell renderer from a column based on path or iter
	 * or whatever.
	 */
	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (widget), path, column, cells->data, TRUE);

	g_list_free (cells);
	gtk_tree_path_free (path);

	validate (GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes_dialog")));
}

static void
route_delete_clicked (GtkButton *button, gpointer user_data)
{
	GtkBuilder *builder = GTK_BUILDER (user_data);
	GtkTreeView *treeview;
	GtkTreeSelection *selection;
	GList *selected_rows;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	int num_rows;

	treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "ip6_routes"));

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

	validate (GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes_dialog")));
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
	GtkBuilder *builder = GTK_BUILDER (user_data);
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	guint32 column;

	if (last_edited) {
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_builder_get_object (builder, "ip6_routes")));
		if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
			column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (renderer), "column"));
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, last_edited, -1);
		}

		g_free (last_edited);
		last_edited = NULL;
	}

	g_free (last_path);
	last_path = NULL;
	last_column = -1;

	validate (GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes_dialog")));
}

static void
cell_edited (GtkCellRendererText *cell,
             const gchar *path_string,
             const gchar *new_text,
             gpointer user_data)
{
	GtkBuilder *builder = GTK_BUILDER (user_data);
	GtkWidget *widget, *dialog;
	GtkListStore *store;
	GtkTreePath *path;
	GtkTreeIter iter;
	guint32 column;
	GtkTreeViewColumn *next_col;
	GtkCellRenderer *next_cell;

	/* Free auxiliary stuff */
	g_free (last_edited);
	last_edited = NULL;
	g_free (last_path);
	last_path = NULL;
	last_column = -1;

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes"));
	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
	path = gtk_tree_path_new_from_string (path_string);
	column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (cell), "column"));

	gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
	gtk_list_store_set (store, &iter, column, new_text, -1);

	/* Move focus to the next column */
	column = (column >= COL_LAST) ? 0 : column + 1;
	next_col = gtk_tree_view_get_column (GTK_TREE_VIEW (widget), column);
	dialog = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes_dialog"));
	next_cell = g_slist_nth_data (g_object_get_data (G_OBJECT (dialog), "renderers"), column);

	gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (widget), path, next_col, next_cell, TRUE);

	gtk_tree_path_free (path);

	validate (dialog);
}

static void
ip_address_filter_cb (GtkEditable *editable,
                      gchar *text,
                      gint length,
                      gint *position,
                      gpointer user_data)
{
	GtkWidget *ok_button = user_data;
	gboolean changed;

	changed = utils_filter_editable_on_insert_text (editable,
	                                                text, length, position, user_data,
	                                                utils_char_is_ascii_ip6_address,
	                                                ip_address_filter_cb);

	if (changed) {
		g_free (last_edited);
		last_edited = gtk_editable_get_chars (editable, 0, -1);
	}

	/* Desensitize the OK button during input to simplify input validation.
	 * All routes will be validated on focus-out, which will then re-enable
	 * the OK button if the routes are valid.
	 */
	gtk_widget_set_sensitive (ok_button, FALSE);
}

static void
delete_text_cb (GtkEditable *editable,
                gint start_pos,
                gint end_pos,
                gpointer user_data)
{
	GtkWidget *ok_button = user_data;

	/* Keep last_edited up-to-date */
	g_free (last_edited);
	last_edited = gtk_editable_get_chars (editable, 0, -1);

	/* Desensitize the OK button during input to simplify input validation.
	 * All routes will be validated on focus-out, which will then re-enable
	 * the OK button if the routes are valid.
	 */
	gtk_widget_set_sensitive (ok_button, FALSE);
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

	column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (user_data), "column"));

	if (column == COL_PREFIX) {
		long int tmp_int;

		errno = 0;
		tmp_int = strtol (cell_text, NULL, 10);
		if (errno || tmp_int < 0 || tmp_int > 128)
			value_valid = FALSE;
		else
			value_valid = TRUE;
	} else if (column == COL_METRIC) {
		long int tmp_int;

		errno = 0;
		tmp_int = strtol (cell_text, NULL, 10);
		if (errno || tmp_int < 0 || tmp_int > G_MAXUINT32)
			value_valid = FALSE;
		else
			value_valid = TRUE;
	} else {
		struct in6_addr tmp_addr;

		if (inet_pton (AF_INET6, cell_text, &tmp_addr) > 0)
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
ip6_cell_editing_started (GtkCellRenderer *cell,
                          GtkCellEditable *editable,
                          const gchar     *path,
                          gpointer         user_data)
{
	if (!GTK_IS_ENTRY (editable)) {
		g_warning ("%s: Unexpected cell editable type.", __func__);
		return;
	}

	/* Initialize last_path and last_column, last_edited is initialized when the cell is edited */
	g_free (last_edited);
	last_edited = NULL;
	g_free (last_path);
	last_path = g_strdup (path);
	last_column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (cell), "column"));

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
uint_filter_cb (GtkEditable *editable,
                gchar *text,
                gint length,
                gint *position,
                gpointer user_data)
{
	GtkWidget *ok_button = user_data;
	gboolean changed;

	changed = utils_filter_editable_on_insert_text (editable,
	                                                text, length, position, user_data,
	                                                utils_char_is_ascii_digit,
	                                                uint_filter_cb);

	if (changed) {
		g_free (last_edited);
		last_edited = gtk_editable_get_chars (editable, 0, -1);
	}

	/* Desensitize the OK button during input to simplify input validation.
	 * All routes will be validated on focus-out, which will then re-enable
	 * the OK button if the routes are valid.
	 */
	gtk_widget_set_sensitive (ok_button, FALSE);
}

static void
uint_cell_editing_started (GtkCellRenderer *cell,
                           GtkCellEditable *editable,
                           const gchar     *path,
                           gpointer         user_data)
{
	if (!GTK_IS_ENTRY (editable)) {
		g_warning ("%s: Unexpected cell editable type.", __func__);
		return;
	}

	/* Initialize last_path and last_column, last_edited is initialized when the cell is edited */
	g_free (last_edited);
	last_edited = NULL;
	g_free (last_path);
	last_path = g_strdup (path);
	last_column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (cell), "column"));

	/* Set up the entry filter */
	g_signal_connect (G_OBJECT (editable), "insert-text",
	                  (GCallback) uint_filter_cb,
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

static gboolean
tree_view_button_pressed_cb (GtkWidget *widget,
                             GdkEvent *event,
                             gpointer user_data)
{
	GtkBuilder *builder = GTK_BUILDER (user_data);

	/* last_edited can be set e.g. when we get here by clicking an cell while
	 * editing another cell. GTK3 issue neither editing-canceled nor editing-done
	 * for cell renderer. Thus the previous cell value isn't saved. Store it now. */
	if (last_edited && last_path) {
		GtkTreeIter iter;
		GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
		GtkTreePath *last_treepath = gtk_tree_path_new_from_string (last_path);

		gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, last_treepath);
		gtk_list_store_set (store, &iter, last_column, last_edited, -1);
		gtk_tree_path_free (last_treepath);

		g_free (last_edited);
		last_edited = NULL;
		g_free (last_path);
		last_path = NULL;
		last_column = -1;
	}

	/* Ignore double clicks events. (They are issued after the single clicks, see GdkEventButton) */
	if (event->type == GDK_2BUTTON_PRESS)
		return TRUE;

	gtk_widget_grab_focus (GTK_WIDGET (widget));
	validate (GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes_dialog")));
	return FALSE;
}

GtkWidget *
ip6_routes_dialog_new (NMSettingIP6Config *s_ip6, gboolean automatic)
{
	GtkBuilder *builder;
	GtkWidget *dialog, *widget, *ok_button;
	GtkListStore *store;
	GtkTreeIter model_iter;
	GtkTreeSelection *selection;
	gint offset;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	int i;
	GSList *renderers = NULL;
	GError* error = NULL;

	/* Initialize temporary storage vars */
	g_free (last_edited);
	last_edited = NULL;
	last_path = NULL;
	g_free (last_path);
	last_column = -1;

	builder = gtk_builder_new ();

	if (!gtk_builder_add_from_file (builder, UIDIR "/ce-ip6-routes.ui", &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	dialog = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes_dialog"));
	if (!dialog) {
		g_warning ("%s: Couldn't load ip6 routes dialog from .ui file.", __func__);
		g_object_unref (builder);
		return NULL;
	}

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	g_object_set_data_full (G_OBJECT (dialog), "builder",
	                        builder, (GDestroyNotify) g_object_unref);

	ok_button = GTK_WIDGET (gtk_builder_get_object (builder, "ok_button"));

	store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	/* Add existing routes */
	for (i = 0; i < nm_setting_ip6_config_get_num_routes (s_ip6); i++) {
		NMIP6Route *route = nm_setting_ip6_config_get_route (s_ip6, i);
		const struct in6_addr *tmp_addr;
		char ip_string[INET6_ADDRSTRLEN];
		char *tmp;

		if (!route) {
			g_warning ("%s: empty IP6 route structure!", __func__);
			continue;
		}

		gtk_list_store_append (store, &model_iter);

		tmp_addr = nm_ip6_route_get_dest (route);
		if (inet_ntop (AF_INET6, tmp_addr, ip_string, sizeof (ip_string)))
			gtk_list_store_set (store, &model_iter, COL_ADDRESS, ip_string, -1);

		tmp = g_strdup_printf ("%u", nm_ip6_route_get_prefix (route));
		gtk_list_store_set (store, &model_iter, COL_PREFIX, tmp, -1);
		g_free (tmp);

		tmp_addr = nm_ip6_route_get_next_hop (route);
		if (tmp_addr && !IN6_IS_ADDR_UNSPECIFIED (tmp_addr) &&
			inet_ntop (AF_INET6, tmp_addr, ip_string, sizeof (ip_string)))
			gtk_list_store_set (store, &model_iter, COL_NEXT_HOP, ip_string, -1);

		if (nm_ip6_route_get_metric (route)) {
			tmp = g_strdup_printf ("%u", nm_ip6_route_get_metric (route));
			gtk_list_store_set (store, &model_iter, COL_METRIC, tmp, -1);
			g_free (tmp);
		}
	}

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));
	g_object_unref (store);

	/* IP Address column */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), builder);
	g_object_set_data (G_OBJECT (renderer), "column", GUINT_TO_POINTER (COL_ADDRESS));
	g_signal_connect (renderer, "editing-started", G_CALLBACK (ip6_cell_editing_started), ok_button);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (cell_editing_canceled), builder);
	renderers = g_slist_append (renderers, renderer);

	offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
	                                                      -1, _("Address"), renderer,
	                                                      "text", COL_ADDRESS,
	                                                      NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (widget), offset - 1);
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	/* Prefix column */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), builder);
	g_object_set_data (G_OBJECT (renderer), "column", GUINT_TO_POINTER (COL_PREFIX));
	g_signal_connect (renderer, "editing-started", G_CALLBACK (uint_cell_editing_started), ok_button);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (cell_editing_canceled), builder);
	renderers = g_slist_append (renderers, renderer);

	offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
	                                                      -1, _("Prefix"), renderer,
	                                                      "text", COL_PREFIX,
	                                                      NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (widget), offset - 1);
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	/* Gateway column */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), builder);
	g_object_set_data (G_OBJECT (renderer), "column", GUINT_TO_POINTER (COL_NEXT_HOP));
	g_signal_connect (renderer, "editing-started", G_CALLBACK (ip6_cell_editing_started), ok_button);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (cell_editing_canceled), builder);
	renderers = g_slist_append (renderers, renderer);

	offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
	                                                      -1, _("Gateway"), renderer,
	                                                      "text", COL_NEXT_HOP,
	                                                      NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (widget), offset - 1);
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	/* Metric column */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), builder);
	g_object_set_data (G_OBJECT (renderer), "column", GUINT_TO_POINTER (COL_METRIC));
	g_signal_connect (renderer, "editing-started", G_CALLBACK (uint_cell_editing_started), ok_button);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (cell_editing_canceled), builder);
	renderers = g_slist_append (renderers, renderer);

	offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
	                                                      -1, _("Metric"), renderer,
	                                                      "text", COL_METRIC,
	                                                      NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (widget), offset - 1);
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	g_object_set_data_full (G_OBJECT (dialog), "renderers", renderers, (GDestroyNotify) g_slist_free);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
	                  G_CALLBACK (list_selection_changed),
	                  GTK_WIDGET (gtk_builder_get_object (builder, "ip6_route_delete_button")));
	g_signal_connect (widget, "button-press-event", G_CALLBACK (tree_view_button_pressed_cb), builder);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_route_add_button"));
	gtk_widget_set_sensitive (widget, TRUE);
	g_signal_connect (widget, "clicked", G_CALLBACK (route_add_clicked), builder);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_route_delete_button"));
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (widget, "clicked", G_CALLBACK (route_delete_clicked), builder);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_ignore_auto_routes"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
	                              nm_setting_ip6_config_get_ignore_auto_routes (s_ip6));
	gtk_widget_set_sensitive (widget, automatic);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_never_default"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
	                              nm_setting_ip6_config_get_never_default (s_ip6));

	/* Update initial validity */
	validate (dialog);

	return dialog;
}

void
ip6_routes_dialog_update_setting (GtkWidget *dialog, NMSettingIP6Config *s_ip6)
{
	GtkBuilder *builder;
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter tree_iter;
	gboolean iter_valid;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (s_ip6 != NULL);

	builder = g_object_get_data (G_OBJECT (dialog), "builder");
	g_return_if_fail (builder != NULL);
	g_return_if_fail (GTK_IS_BUILDER (builder));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_routes"));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	iter_valid = gtk_tree_model_get_iter_first (model, &tree_iter);

	nm_setting_ip6_config_clear_routes (s_ip6);

	while (iter_valid) {
		struct in6_addr dest, next_hop;
		guint prefix = 0, metric = 0;
		NMIP6Route *route;

		/* Address */
		if (!get_one_addr (model, &tree_iter, COL_ADDRESS, TRUE, &dest)) {
			g_warning ("%s: IPv6 address missing or invalid!", __func__);
			goto next;
		}

		/* Prefix */
		if (!get_one_int (model, &tree_iter, COL_PREFIX, 128, TRUE, &prefix)) {
			g_warning ("%s: IPv6 prefix missing or invalid!", __func__);
			goto next;
		}

		/* Next hop (optional) */
		memset (&next_hop, 0, sizeof (struct in6_addr));
		if (!get_one_addr (model, &tree_iter, COL_NEXT_HOP, FALSE, &next_hop)) {
			g_warning ("%s: IPv6 next hop invalid!", __func__);
			goto next;
		}

		/* Metric (optional) */
		if (!get_one_int (model, &tree_iter, COL_METRIC, G_MAXUINT32, FALSE, &metric)) {
			g_warning ("%s: IPv6 metric invalid!", __func__);
			goto next;
		}

		route = nm_ip6_route_new ();
		nm_ip6_route_set_dest (route, &dest);
		nm_ip6_route_set_prefix (route, prefix);
		nm_ip6_route_set_next_hop (route, &next_hop);
		nm_ip6_route_set_metric (route, metric);
		nm_setting_ip6_config_add_route (s_ip6, route);
		nm_ip6_route_unref (route);

	next:
		iter_valid = gtk_tree_model_iter_next (model, &tree_iter);
	}

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_ignore_auto_routes"));
	g_object_set (s_ip6, NM_SETTING_IP6_CONFIG_IGNORE_AUTO_ROUTES,
	              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)),
	              NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "ip6_never_default"));
	g_object_set (s_ip6, NM_SETTING_IP6_CONFIG_NEVER_DEFAULT,
	              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)),
	              NULL);
}

