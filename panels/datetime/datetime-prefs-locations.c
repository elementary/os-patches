/* -*- Mode: C; coding: utf-8; indent-tabs-mode: nil; tab-width: 2 -*-

A dialog for setting time and date preferences.

Copyright 2011 Canonical Ltd.

Authors:
    Michael Terry <michael.terry@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#include <stdlib.h>
#include <time.h> /* time_t */
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <timezonemap/timezone-completion.h>

#include "datetime-prefs-locations.h"
#include "settings-shared.h"
#include "utils.h"

#define DATETIME_DIALOG_UI_FILE PKGDATADIR "/ui/datetime/datetime-dialog.ui"

#define COL_NAME         0
#define COL_TIME         1
#define COL_ZONE         2
#define COL_VISIBLE_NAME 3
#define COL_ICON         4

static gboolean update_times (GtkWidget * dlg);
static void save_when_idle (GtkWidget * dlg);

/***
**** Sorting
***/

/**
 * A temporary struct used for sorting
 */
struct TimeLocation
{
  gchar * collated_name;
  gint pos;
  gint32 offset;
};

static struct TimeLocation*
time_location_new (const char * zone, const char * name, int pos, time_t now)
{
  struct TimeLocation * loc = g_new (struct TimeLocation, 1);
  GTimeZone * tz = g_time_zone_new (zone);
  const gint interval = g_time_zone_find_interval (tz, G_TIME_TYPE_UNIVERSAL, now);
  loc->offset = g_time_zone_get_offset (tz, interval);
  loc->collated_name = g_utf8_collate_key (name, -1);
  loc->pos = pos;
  g_time_zone_unref (tz);
  return loc;
}

static void
time_location_free (struct TimeLocation * loc)
{
  g_free (loc->collated_name);
  g_free (loc);
}

static GSList*
time_location_array_new_from_model (GtkTreeModel * model)
{
  int pos = 0;
  GtkTreeIter iter;
  GSList * list = NULL;
  const time_t now = time (NULL);

  if (gtk_tree_model_get_iter_first (model, &iter)) do
    {
      gchar * zone = NULL;
      gchar * name = NULL;

      gtk_tree_model_get (model, &iter,
                          COL_ZONE, &zone,
                          COL_VISIBLE_NAME, &name,
                          -1);

      if (zone && name)
        list = g_slist_prepend (list, time_location_new (zone, name, pos++, now));

      g_free (name);
      g_free (zone);
    }
  while (gtk_tree_model_iter_next (model, &iter));

  return g_slist_reverse (list);
}

static void
handle_sort(GtkWidget * button G_GNUC_UNUSED,
            GtkTreeView * tree_view,
            GCompareFunc compare)
{
  GtkTreeModel * model = gtk_tree_view_get_model (tree_view);
  GSList * l;
  GSList * list = g_slist_sort (time_location_array_new_from_model(model), compare);

  gint i;
  gint * reorder = g_new (gint, g_slist_length(list));
  for (i=0, l=list; l!=NULL; l=l->next, i++)
      reorder[i] = ((struct TimeLocation*)l->data)->pos;
  gtk_list_store_reorder (GTK_LIST_STORE(model), reorder);

  g_free (reorder);
  g_slist_free_full (list, (GDestroyNotify)time_location_free);
}

static gint
time_location_compare_by_name (gconstpointer ga, gconstpointer gb)
{
  const struct TimeLocation * a = ga;
  const struct TimeLocation * b = gb;
  int ret = g_strcmp0 (a->collated_name, b->collated_name); /* primary key */
  if (!ret)
    ret = a->offset - b->offset; /* secondary key */
  return ret;
}
static void
handle_sort_by_name (GtkWidget * button, GtkTreeView * tree_view)
{
  handle_sort (button, tree_view, time_location_compare_by_name);
}

static gint
time_location_compare_by_time (gconstpointer ga, gconstpointer gb)
{
  const struct TimeLocation * a = ga;
  const struct TimeLocation * b = gb;
  int ret = a->offset - b->offset; /* primary key */
  if (!ret)
    ret = g_strcmp0 (a->collated_name, b->collated_name); /* secondary key */
  return ret;
}
static void
handle_sort_by_time (GtkWidget * button, GtkTreeView * tree_view)
{
  handle_sort (button, tree_view, time_location_compare_by_time);
}

static gboolean
time_location_list_test_sorted (GSList * list, GCompareFunc compare)
{
  GSList * l;
  for (l=list; l!=NULL && l->next!=NULL; l=l->next)
    if (compare(l->data, l->next->data) > 0)
      return FALSE;
  return TRUE;
}
static void
location_model_test_sorted (GtkTreeModel * model, gboolean * is_sorted_by_name, gboolean * is_sorted_by_time)
{
  GSList * list = time_location_array_new_from_model(model);
  *is_sorted_by_name = time_location_list_test_sorted (list, time_location_compare_by_name);
  *is_sorted_by_time = time_location_list_test_sorted (list, time_location_compare_by_time);
  g_slist_free_full (list, (GDestroyNotify)time_location_free);
}

/***
****
***/

static void
handle_add (GtkWidget * button G_GNUC_UNUSED, GtkTreeView * tree)
{
  GtkListStore * store = GTK_LIST_STORE (gtk_tree_view_get_model (tree));

  GtkTreeIter iter;
  gtk_list_store_append (store, &iter);

  GtkTreePath * path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
  gtk_tree_view_set_cursor (tree, path, gtk_tree_view_get_column (tree, 0), TRUE);
  gtk_tree_path_free (path);
}

static void
handle_remove (GtkWidget * button G_GNUC_UNUSED, GtkTreeView * tree)
{
  GtkListStore * store = GTK_LIST_STORE (gtk_tree_view_get_model (tree));
  GtkTreeSelection * selection = gtk_tree_view_get_selection (tree);

  GList * paths = gtk_tree_selection_get_selected_rows (selection, NULL);

  /* Convert all paths to iters so we can safely delete multiple paths.  For a
     GtkListStore, iters persist past model changes. */
  GList * tree_iters = NULL;
  GList * iter;
  for (iter = paths; iter; iter = iter->next) {
    GtkTreeIter * tree_iter = g_new(GtkTreeIter, 1);
    if (gtk_tree_model_get_iter (GTK_TREE_MODEL (store), tree_iter, (GtkTreePath *)iter->data)) {
      tree_iters = g_list_prepend (tree_iters, tree_iter);
    }
    gtk_tree_path_free (iter->data);
  }
  g_list_free (paths);
  
  // Find the next item to select
  GtkTreeIter *last_selected = g_list_nth_data(tree_iters, 0);
  GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL (store), last_selected);
  GtkTreeIter titer;
  if (!gtk_tree_model_get_iter(GTK_TREE_MODEL (store), &titer, path)) {
    g_debug("Failed to get last selected iter from path");
  	last_selected = NULL;
  } else {
	  if (!gtk_tree_model_iter_next(GTK_TREE_MODEL (store), &titer)) {
	  	if (gtk_tree_path_prev(path)) {
	  	  	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL (store), &titer, path)) {
	  			g_debug("Failed to get iter from path");
	  			last_selected = NULL;
	  		} else {
		  		last_selected = &titer;
		  	}
	  	} else {
	  		g_debug("handle_remove: Failed to find another location to select (assume single selected)");
	  		last_selected = NULL;
	  	}
	  } else {
	  	g_debug("Got next item in model");
		last_selected = &titer;
	  }
  }
 
  if (last_selected) {
	  gboolean clear = TRUE;
	  path = gtk_tree_model_get_path(GTK_TREE_MODEL (store), last_selected);
	  
      // Step over the path to find an item which isn't in the delete list
	  if (g_list_length(tree_iters) > 1) {
		  for (iter = tree_iters; iter; iter = iter->next) {
		  	GtkTreePath *ipath = gtk_tree_model_get_path(GTK_TREE_MODEL (store), (GtkTreeIter *)iter->data);
		  	if (gtk_tree_path_compare(path, ipath) == 0) {
		  		clear = FALSE;
		  		break;
		  	}
		  }
	  	  while (clear == FALSE) {
			if (gtk_tree_path_prev(path)) {
				clear = TRUE;
		  	  	for (iter = tree_iters; iter; iter = iter->next) {
		  			GtkTreePath *ipath = gtk_tree_model_get_path(GTK_TREE_MODEL (store), (GtkTreeIter *)iter->data);
		  			if (gtk_tree_path_compare(path, ipath) == 0) {
		  				clear = FALSE;
		  				break;
		  			}
		  		}
		  		if (clear) {
			  		if (!gtk_tree_model_get_iter(GTK_TREE_MODEL (store), &titer, path)) {
			  			g_debug("Failed to get iter from path");
			  			last_selected = NULL;
			  		} else {
				  		last_selected = &titer;
				  	}
				}
		  	} else {
		  		last_selected = NULL;
		  		break;
		  	}
		  }
	  }
  }
  
  /* Now delete each iterator */
  for (iter = tree_iters; iter; iter = iter->next) {
    gtk_list_store_remove (store, (GtkTreeIter *)iter->data);
    g_free (iter->data);
  }
  g_list_free (tree_iters);
  
  if (last_selected)
	  gtk_tree_selection_select_iter(selection, last_selected);
}

static void
handle_edit (GtkCellRendererText * renderer G_GNUC_UNUSED,
             gchar * path,
             gchar * new_text,
             GtkListStore * store)
{
  GtkTreeIter iter;

  // Manual user edits are always wrong (unless they are undoing a previous
  // edit), so we set the error icon here if needed.  Common way to get to
  // this code path is to lose entry focus.
  if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), &iter, path)) {
    gchar * name;
    gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COL_NAME, &name, -1);
    gboolean correct = g_strcmp0 (name, new_text) == 0;
    g_free (name);

    gtk_list_store_set (store, &iter,
                        COL_VISIBLE_NAME, new_text,
                        COL_ICON, correct ? NULL : "dialog-error",
                        -1);
  }
}

static gboolean
timezone_selected (GtkEntryCompletion * widget, GtkTreeModel * model,
                   GtkTreeIter * iter, GtkWidget * dlg)
{
  gchar * zone = NULL;
  gchar * name = NULL;

  gtk_tree_model_get (model, iter,
                      CC_TIMEZONE_COMPLETION_ZONE, &zone,
                      CC_TIMEZONE_COMPLETION_NAME, &name,
                      -1);

  /* if no explicit timezone, try to determine one from latlon */
  if (!zone || !*zone)
  {
    gchar * strlat = NULL;
    gchar * strlon = NULL;
    gdouble lat = 0;
    gdouble lon = 0;

    gtk_tree_model_get (model, iter,
                        CC_TIMEZONE_COMPLETION_LATITUDE, &strlat,
                        CC_TIMEZONE_COMPLETION_LONGITUDE, &strlon,
                        -1);

    if (strlat && *strlat) lat = g_ascii_strtod(strlat, NULL);
    if (strlon && *strlon) lon = g_ascii_strtod(strlon, NULL);

    CcTimezoneMap * tzmap = CC_TIMEZONE_MAP (g_object_get_data (G_OBJECT (widget), "tzmap"));
    g_free (zone);
    zone = g_strdup (cc_timezone_map_get_timezone_at_coords (tzmap, lon, lat));

    g_free (strlat);
    g_free (strlon);
  }

  GtkListStore * store = GTK_LIST_STORE (g_object_get_data (G_OBJECT (widget), "store"));
  GtkTreeIter * store_iter = (GtkTreeIter *)g_object_get_data (G_OBJECT (widget), "store_iter");
  if (store != NULL && store_iter != NULL) {
    gtk_list_store_set (store, store_iter,
                        COL_VISIBLE_NAME, name,
                        COL_ICON, NULL,
                        COL_NAME, name,
                        COL_ZONE, zone, -1);
  }

  update_times (dlg);

  /* cleanup */
  g_free (name);
  g_free (zone);

  return FALSE; // Do normal action too
}

static gboolean
query_tooltip (GtkTreeView * tree, gint x, gint y, gboolean keyboard_mode,
               GtkTooltip * tooltip, GtkCellRenderer * cell)
{
  GtkTreeModel * model;
  GtkTreeIter iter;
  if (!gtk_tree_view_get_tooltip_context (tree, &x, &y, keyboard_mode,
                                          &model, NULL, &iter))
    return FALSE;

  const gchar * icon;
  gtk_tree_model_get (model, &iter, COL_ICON, &icon, -1);
  if (icon == NULL)
    return FALSE;

  GtkTreeViewColumn * col = gtk_tree_view_get_column (tree, 0);
  gtk_tree_view_set_tooltip_cell (tree, tooltip, NULL, col, cell);
  gtk_tooltip_set_text (tooltip, _("You need to complete this location for it to appear in the menu."));
  return TRUE;
}

static void
handle_edit_started (GtkCellRendererText * renderer G_GNUC_UNUSED,
                     GtkCellEditable * editable,
                     gchar * path,
                     CcTimezoneCompletion * completion)
{
  if (GTK_IS_ENTRY (editable)) {
    GtkEntry *entry = GTK_ENTRY (editable);
    cc_timezone_completion_watch_entry (completion, entry);

    GtkListStore * store = GTK_LIST_STORE (g_object_get_data (G_OBJECT (completion), "store"));
    GtkTreeIter * store_iter = g_new(GtkTreeIter, 1);
    if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (store), store_iter, path)) {
      g_object_set_data_full (G_OBJECT (completion), "store_iter", store_iter, g_free);
    }
  }
}

static gboolean
update_times (GtkWidget * dlg)
{
  /* For each entry, check zone in column 2 and set column 1 to it's time */
  CcTimezoneCompletion * completion = CC_TIMEZONE_COMPLETION (g_object_get_data (G_OBJECT (dlg), "completion"));
  GtkListStore * store = GTK_LIST_STORE (g_object_get_data (G_OBJECT (completion), "store"));
  GObject * cell = G_OBJECT (g_object_get_data (G_OBJECT (completion), "name-cell"));

  gboolean editing;
  g_object_get (cell, "editing", &editing, NULL);
  if (editing) { /* No updates while editing, it cancels the edit */
    return TRUE;
  }

  g_signal_handlers_block_by_func (store, save_when_idle, dlg);

  GSettings * settings = g_settings_new (SETTINGS_INTERFACE);
  GtkTreeIter iter;
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
    GDateTime * now = g_date_time_new_now_local ();
    do {
      gchar * strzone;

      gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COL_ZONE, &strzone, -1);

      if (strzone && *strzone) {
        GTimeZone * tz = g_time_zone_new (strzone);
        GDateTime * now_tz = g_date_time_to_timezone (now, tz);
        gchar * format = generate_full_format_string_at_time (now, now_tz, settings);
        gchar * time_str = g_date_time_format (now_tz, format);
        gchar * old_time_str;

        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COL_TIME, &old_time_str, -1);
        if (g_strcmp0 (old_time_str, time_str))
          gtk_list_store_set (store, &iter, COL_TIME, time_str, -1);

        g_free (old_time_str);
        g_free (time_str);
        g_free (format);
        g_date_time_unref (now_tz);
        g_time_zone_unref (tz);
      }
      g_free (strzone);
    } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
    g_date_time_unref (now);
  }

  g_object_unref (settings);

  g_signal_handlers_unblock_by_func (store, save_when_idle, dlg);

  return TRUE;
}

static void
fill_from_settings (GObject * store, GSettings * conf)
{
  gchar ** locations = g_settings_get_strv (conf, SETTINGS_LOCATIONS_S);

  gtk_list_store_clear (GTK_LIST_STORE (store));

  gchar ** striter;
  GtkTreeIter iter;
  for (striter = locations; *striter; ++striter) {
    gchar * zone, * name;
    split_settings_location (*striter, &zone, &name);

    gtk_list_store_append (GTK_LIST_STORE (store), &iter);
    gtk_list_store_set (GTK_LIST_STORE (store), &iter,
                        COL_VISIBLE_NAME, name,
                        COL_ICON, NULL,
                        COL_NAME, name,
                        COL_ZONE, zone, -1);

    g_free (zone);
    g_free (name);
  }

  g_strfreev (locations);
}

static void
save_to_settings (GObject * store, GSettings * conf)
{
  gboolean empty = TRUE;
  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

  GtkTreeIter iter;
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
    GString * gstr = g_string_new (NULL);
    do {
      gchar * strname;
      gchar * strzone;
      gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                          COL_NAME, &strname,
                          COL_ZONE, &strzone,
                          -1);
      if (strzone && *strzone && strname && *strname) {
        g_string_printf (gstr, "%s %s", strzone, strname);
        g_variant_builder_add (&builder, "s", gstr->str);
        empty = FALSE;
      }
      g_free (strname);
      g_free (strzone);
    } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
    g_string_free (gstr, TRUE);
  }

  if (empty) {
    /* Empty list */
    g_variant_builder_clear (&builder);
    g_settings_set_strv (conf, SETTINGS_LOCATIONS_S, NULL);
  }
  else {
    g_settings_set_value (conf, SETTINGS_LOCATIONS_S, g_variant_builder_end (&builder));
  }
}

static gboolean
save_now (GtkWidget *dlg)
{
  GSettings * conf = G_SETTINGS (g_object_get_data (G_OBJECT (dlg), "conf"));
  GObject * completion = G_OBJECT (g_object_get_data (G_OBJECT (dlg), "completion"));
  GObject * store = G_OBJECT (g_object_get_data (completion, "store"));

  save_to_settings (store, conf);

  g_object_set_data (G_OBJECT (dlg), "save-id", GINT_TO_POINTER(0));

  return FALSE;
}

static void
save_when_idle (GtkWidget *dlg)
{
  guint save_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dlg), "save-id"));

  if (save_id == 0) {
    save_id = g_idle_add ((GSourceFunc)save_now, dlg);
    g_object_set_data (G_OBJECT (dlg), "save-id", GINT_TO_POINTER(save_id));
  }
}

static void
dialog_closed (GtkWidget * dlg, GObject * store G_GNUC_UNUSED)
{
  /* Cleanup a tad */
  guint time_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dlg), "time-id"));
  g_source_remove (time_id);

  guint save_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dlg), "save-id"));
  if (save_id > 0)
    g_source_remove (save_id);
}

static void
selection_changed (GtkTreeSelection * selection, GtkWidget * remove_button)
{
  gint count = gtk_tree_selection_count_selected_rows (selection);
  gtk_widget_set_sensitive (remove_button, count > 0);
}

static void
update_button_sensitivity (GtkWidget * dlg)
{
  GObject * odlg = G_OBJECT(dlg);
  GObject * completion = g_object_get_data(odlg, "completion");
  GtkTreeModel * model = GTK_TREE_MODEL (g_object_get_data (completion, "store")); 
  gboolean is_sorted_by_name;
  gboolean is_sorted_by_time;
  location_model_test_sorted (model, &is_sorted_by_name, &is_sorted_by_time);
  gtk_widget_set_sensitive (GTK_WIDGET(g_object_get_data(odlg, "sortByNameButton")), !is_sorted_by_name);
  gtk_widget_set_sensitive (GTK_WIDGET(g_object_get_data(odlg, "sortByTimeButton")), !is_sorted_by_time);
}

static void
model_changed (GtkWidget * dlg)
{
  update_button_sensitivity (dlg);
  save_when_idle (dlg);
}

GtkWidget *
datetime_setup_locations_dialog (CcTimezoneMap * map)
{
  GError * error = NULL;
  GtkBuilder * builder = gtk_builder_new ();
  gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
  gtk_builder_add_from_file (builder, DATETIME_DIALOG_UI_FILE, &error);
  if (error != NULL) {
    /* We have to abort, we can't continue without the ui file */
    g_error ("Could not load ui file %s: %s", DATETIME_DIALOG_UI_FILE, error->message);
    g_error_free (error);
    return NULL;
  }

  GSettings * conf = g_settings_new (SETTINGS_INTERFACE);

#define WIG(name) GTK_WIDGET (gtk_builder_get_object (builder, name))

  GtkWidget * dlg = WIG ("locationsDialog");
  GtkWidget * tree = WIG ("locationsView");
  GObject * store = gtk_builder_get_object (builder, "locationsStore");

  /* Configure tree */
  CcTimezoneCompletion * completion = cc_timezone_completion_new ();
  g_object_set_data (G_OBJECT (completion), "tzmap", map);
  g_object_set_data (G_OBJECT (completion), "store", store);
  g_signal_connect (completion, "match-selected", G_CALLBACK (timezone_selected), dlg);

  GtkCellRenderer * cell = gtk_cell_renderer_text_new ();
  g_object_set (cell, "editable", TRUE, NULL);
  g_signal_connect (cell, "editing-started", G_CALLBACK (handle_edit_started), completion);
  g_signal_connect (cell, "edited", G_CALLBACK (handle_edit), store);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1,
                                               _("Location"), cell,
                                               "text", COL_VISIBLE_NAME, NULL);
  GtkTreeViewColumn * loc_col = gtk_tree_view_get_column (GTK_TREE_VIEW (tree), 0);
  gtk_tree_view_column_set_expand (loc_col, TRUE);
  g_object_set_data (G_OBJECT (completion), "name-cell", cell);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (loc_col, cell, FALSE);
  gtk_tree_view_column_add_attribute (loc_col, cell, "icon-name", COL_ICON);

  gtk_widget_set_has_tooltip (tree, TRUE);
  g_signal_connect (tree, "query-tooltip", G_CALLBACK (query_tooltip), cell);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_set_alignment (cell, 1.0f, 0.5f);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1,
                                               _("Time"), cell,
                                               "text", COL_TIME, NULL);

  GtkTreeSelection * selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect (selection, "changed", G_CALLBACK (selection_changed), WIG ("removeButton"));
  selection_changed (selection, WIG ("removeButton"));

  g_signal_connect (WIG ("addButton"), "clicked", G_CALLBACK (handle_add), tree);
  g_signal_connect (WIG ("removeButton"), "clicked", G_CALLBACK (handle_remove), tree);

  GtkWidget * w = WIG ("sortByNameButton");
  g_signal_connect (w, "clicked", G_CALLBACK (handle_sort_by_name), tree);
  g_object_set_data (G_OBJECT(dlg), "sortByNameButton", w);

  w = WIG ("sortByTimeButton");
  g_signal_connect (w, "clicked", G_CALLBACK (handle_sort_by_time), tree);
  g_object_set_data (G_OBJECT(dlg), "sortByTimeButton", w);

  fill_from_settings (store, conf);
  g_signal_connect_swapped (store, "row-deleted", G_CALLBACK (model_changed), dlg);
  g_signal_connect_swapped (store, "row-inserted", G_CALLBACK (model_changed), dlg);
  g_signal_connect_swapped (store, "row-changed", G_CALLBACK (model_changed), dlg);
  g_signal_connect_swapped (store, "rows-reordered", G_CALLBACK (model_changed), dlg);
  g_object_set_data_full (G_OBJECT (dlg), "conf", g_object_ref (conf), g_object_unref);
  g_object_set_data_full (G_OBJECT (dlg), "completion", completion, g_object_unref);
  g_signal_connect (dlg, "destroy", G_CALLBACK (dialog_closed), store);

  guint time_id = g_timeout_add_seconds (2, (GSourceFunc)update_times, dlg);
  g_object_set_data (G_OBJECT (dlg), "time-id", GINT_TO_POINTER(time_id));
  update_times (dlg);

#undef WIG

  g_object_unref (conf);
  g_object_unref (builder);

  return dlg;
}

