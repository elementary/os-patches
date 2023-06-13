/*
 * Copyright (C) 2008 Pierre-Luc Beaudoin <pierre-luc@pierlux.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <gtk/gtk.h>

#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#include <clutter-gtk/clutter-gtk.h>

#include <markers.h>

#define N_COLS 2
#define COL_ID 0
#define COL_NAME 1

static ChamplainPathLayer *path_layer;
static ChamplainPathLayer *path;
static gboolean destroying = FALSE;

/*
 * Terminate the main loop.
 */
static void
on_destroy (GtkWidget *widget, gpointer data)
{
  destroying = TRUE;
  gtk_main_quit ();
}


static void
toggle_layer (GtkToggleButton *widget,
    ClutterActor *layer)
{
  if (gtk_toggle_button_get_active (widget))
    {
      champlain_path_layer_set_visible (path_layer, TRUE);
      champlain_path_layer_set_visible (path, TRUE);
      champlain_marker_layer_animate_in_all_markers (CHAMPLAIN_MARKER_LAYER (layer));
    }
  else
    {
      champlain_path_layer_set_visible (path_layer, FALSE);
      champlain_path_layer_set_visible (path, FALSE);
      champlain_marker_layer_animate_out_all_markers (CHAMPLAIN_MARKER_LAYER (layer));
    }
}


static gboolean
mouse_click_cb (ClutterActor *actor, ClutterButtonEvent *event, ChamplainView *view)
{
  gdouble lat, lon;

  lon = champlain_view_x_to_longitude (view, event->x);
  lat = champlain_view_y_to_latitude (view, event->y);
  g_print ("Mouse click at: %f  %f\n", lat, lon);

  return TRUE;
}


static void
map_source_changed (GtkWidget *widget,
    ChamplainView *view)
{
  gchar *id;
  ChamplainMapSource *source;
  GtkTreeIter iter;
  GtkTreeModel *model;

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter))
    return;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));

  gtk_tree_model_get (model, &iter, COL_ID, &id, -1);

  ChamplainMapSourceFactory *factory = champlain_map_source_factory_dup_default ();
  source = champlain_map_source_factory_create_cached_source (factory, id);

  g_object_set (G_OBJECT (view), "map-source", source, NULL);
  g_object_unref (factory);
}


static void
zoom_changed (GtkSpinButton *spinbutton,
    ChamplainView *view)
{
  gint zoom = gtk_spin_button_get_value_as_int (spinbutton);

  g_object_set (G_OBJECT (view), "zoom-level", zoom, NULL);
}


static void
map_zoom_changed (ChamplainView *view,
    GParamSpec *gobject,
    GtkSpinButton *spinbutton)
{
  gint zoom;

  g_object_get (G_OBJECT (view), "zoom-level", &zoom, NULL);
  gtk_spin_button_set_value (spinbutton, zoom);
}


static void
view_state_changed (ChamplainView *view,
    GParamSpec *gobject,
    GtkImage *image)
{
  ChamplainState state;

  if (destroying)
    return;

  g_object_get (G_OBJECT (view), "state", &state, NULL);
  if (state == CHAMPLAIN_STATE_LOADING)
    {
      gtk_image_set_from_icon_name (image, "edit-find", GTK_ICON_SIZE_BUTTON);
    }
  else
    {
      gtk_image_clear (image);
    }
}


static void
zoom_in (GtkWidget *widget,
    ChamplainView *view)
{
  champlain_view_zoom_in (view);
}


static void
zoom_out (GtkWidget *widget,
    ChamplainView *view)
{
  champlain_view_zoom_out (view);
}


static void
toggle_wrap (GtkWidget *widget,
    ChamplainView *view)
{
 gboolean wrap;

  wrap = champlain_view_get_horizontal_wrap (view);
  champlain_view_set_horizontal_wrap (view, !wrap);
}


static void
build_combo_box (GtkComboBox *box)
{
  ChamplainMapSourceFactory *factory;
  GSList *sources, *iter;
  GtkTreeStore *store;
  GtkTreeIter parent;
  GtkCellRenderer *cell;

  store = gtk_tree_store_new (N_COLS, G_TYPE_STRING, /* id */
        G_TYPE_STRING, /* name */
        -1);

  factory = champlain_map_source_factory_dup_default ();
  sources = champlain_map_source_factory_get_registered (factory);

  iter = sources;
  while (iter != NULL)
    {
      ChamplainMapSourceDesc *desc = CHAMPLAIN_MAP_SOURCE_DESC (iter->data);
      const gchar *id = champlain_map_source_desc_get_id (desc);
      const gchar *name = champlain_map_source_desc_get_name (desc);

      gtk_tree_store_append (store, &parent, NULL);
      gtk_tree_store_set (store, &parent, COL_ID, id,
          COL_NAME, name, -1);

      iter = g_slist_next (iter);
    }

  g_slist_free (sources);
  g_object_unref (factory);

  gtk_combo_box_set_model (box, GTK_TREE_MODEL (store));

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (box), cell, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (box), cell,
      "text", COL_NAME, NULL);
}


static void
append_point (ChamplainPathLayer *layer, gdouble lon, gdouble lat)
{
  ChamplainCoordinate *coord;  
  
  coord = champlain_coordinate_new_full (lon, lat);
  champlain_path_layer_add_node (layer, CHAMPLAIN_LOCATION (coord));
}


static void
export_png (GtkButton     *button,
    ChamplainView *view)
{
  cairo_surface_t *surface;
  GdkPixbuf *pixbuf;
  GFileOutputStream *os;
  GFile *file;
  gint width, height;

  if (champlain_view_get_state (view) != CHAMPLAIN_STATE_DONE)
    return;

  surface = champlain_view_to_surface (view, TRUE);
  if (!surface)
    return;

  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);
  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
  if (!pixbuf)
    return;

  file = g_file_new_for_path ("champlain-map.png");
  os = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL);
  if (!os)
    {
      g_object_unref (pixbuf);
      return;
    }

  gdk_pixbuf_save_to_stream (pixbuf, G_OUTPUT_STREAM (os), "png", NULL, NULL, NULL);
  g_output_stream_close (G_OUTPUT_STREAM (os), NULL, NULL);
}


static void
add_clicked (GtkButton     *button,
             ChamplainView *view)
{
  GtkWidget *window, *dialog, *vbox, *combo;
  GtkResponseType response;

  window = g_object_get_data (G_OBJECT (view), "window");
  dialog = gtk_dialog_new_with_buttons ("Add secondary map source",
                                        GTK_WINDOW (window),
                                        GTK_DIALOG_MODAL,
                                        "Add",
                                        GTK_RESPONSE_OK,
                                        "Cancel",
                                        GTK_RESPONSE_CANCEL,
                                        NULL);

  combo = gtk_combo_box_new ();
  build_combo_box (GTK_COMBO_BOX (combo));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_add (GTK_CONTAINER (vbox), combo);

  gtk_widget_show_all (dialog);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      GtkTreeModel *model;
      GtkTreeIter iter;
      ChamplainMapSource *source;
      ChamplainMapSourceFactory *factory;
      char *id;

      if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
        return;

      model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

      gtk_tree_model_get (model, &iter, COL_ID, &id, -1);

      factory = champlain_map_source_factory_dup_default ();
      source = champlain_map_source_factory_create_memcached_source (factory, id);

      champlain_view_add_overlay_source (view, source, 0.6 * 255);
      g_object_unref (factory);
      g_free (id);
    }

  gtk_widget_destroy (dialog);
}


int
main (int argc,
    char *argv[])
{
  GtkWidget *window;
  GtkWidget *widget, *vbox, *bbox, *button, *viewport, *image;
  ChamplainView *view;
  ChamplainMarkerLayer *layer;
  ClutterActor *scale;
  ChamplainLicense *license_actor;

  if (gtk_clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  /* create the main, top level, window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  /* give the window a 10px wide border */
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  /* give it the title */
  gtk_window_set_title (GTK_WINDOW (window), "libchamplain Gtk+ demo");

  /* Connect the destroy event of the window with our on_destroy function
   * When the window is about to be destroyed we get a notificaiton and
   * stop the main GTK loop
   */
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (on_destroy),
      NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);

  widget = gtk_champlain_embed_new ();
  view = gtk_champlain_embed_get_view (GTK_CHAMPLAIN_EMBED (widget));
  clutter_actor_set_reactive (CLUTTER_ACTOR (view), TRUE);
  g_signal_connect (view, "button-release-event", G_CALLBACK (mouse_click_cb), view);


  g_object_set (G_OBJECT (view),
      "kinetic-mode", TRUE,
      "zoom-level", 5,
      NULL);

  g_object_set_data (G_OBJECT (view), "window", window);
      
  scale = champlain_scale_new ();
  champlain_scale_connect_view (CHAMPLAIN_SCALE (scale), view);
  
  /* align to the bottom left */
  clutter_actor_set_x_expand (scale, TRUE);
  clutter_actor_set_y_expand (scale, TRUE);
  clutter_actor_set_x_align (scale, CLUTTER_ACTOR_ALIGN_START);
  clutter_actor_set_y_align (scale, CLUTTER_ACTOR_ALIGN_END);
  clutter_actor_add_child (CLUTTER_ACTOR (view), scale);
  
  license_actor = champlain_view_get_license_actor (view);
  champlain_license_set_extra_text (license_actor, "Don't eat cereals with orange juice\nIt tastes bad");
  
  champlain_view_center_on (CHAMPLAIN_VIEW (view), 45.466, -73.75);

  layer = create_marker_layer (view, &path);
  champlain_view_add_layer (view, CHAMPLAIN_LAYER (path));
  champlain_view_add_layer (view, CHAMPLAIN_LAYER (layer));
  
  path_layer = champlain_path_layer_new ();
  /* Cheap approx of Highway 10 */
  append_point (path_layer, 45.4095, -73.3197);
  append_point (path_layer, 45.4104, -73.2846);
  append_point (path_layer, 45.4178, -73.2239);
  append_point (path_layer, 45.4176, -73.2181);
  append_point (path_layer, 45.4151, -73.2126);
  append_point (path_layer, 45.4016, -73.1926);
  append_point (path_layer, 45.3994, -73.1877);
  append_point (path_layer, 45.4000, -73.1815);
  append_point (path_layer, 45.4151, -73.1218);
  champlain_view_add_layer (view, CHAMPLAIN_LAYER (path_layer));

  gtk_widget_set_size_request (widget, 640, 481);

  bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  button = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("zoom-in", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_button_set_label (GTK_BUTTON (button), "Zoom In");
  g_signal_connect (button, "clicked", G_CALLBACK (zoom_in), view);
  gtk_container_add (GTK_CONTAINER (bbox), button);

  button = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("zoom-out", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_button_set_label (GTK_BUTTON (button), "Zoom Out");
  g_signal_connect (button, "clicked", G_CALLBACK (zoom_out), view);
  gtk_container_add (GTK_CONTAINER (bbox), button);

  button = gtk_toggle_button_new_with_label ("Markers");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  g_signal_connect (button, "toggled", G_CALLBACK (toggle_layer), layer);
  gtk_container_add (GTK_CONTAINER (bbox), button);

  button = gtk_toggle_button_new_with_label ("Toggle wrap");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                champlain_view_get_horizontal_wrap (view));
  g_signal_connect (button, "toggled", G_CALLBACK (toggle_wrap), view);
  gtk_container_add (GTK_CONTAINER (bbox), button);

  button = gtk_combo_box_new ();
  build_combo_box (GTK_COMBO_BOX (button));
  gtk_combo_box_set_active (GTK_COMBO_BOX (button), 0);
  g_signal_connect (button, "changed", G_CALLBACK (map_source_changed), view);
  gtk_container_add (GTK_CONTAINER (bbox), button);

  button = gtk_spin_button_new_with_range (0, 20, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (button),
      champlain_view_get_zoom_level (view));
  g_signal_connect (button, "changed", G_CALLBACK (zoom_changed), view);
  g_signal_connect (view, "notify::zoom-level", G_CALLBACK (map_zoom_changed),
      button);
  gtk_container_add (GTK_CONTAINER (bbox), button);

  button = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  g_signal_connect (button, "clicked", G_CALLBACK (add_clicked), view);
  gtk_container_add (GTK_CONTAINER (bbox), button);

  button = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("camera-photo-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  g_signal_connect (button, "clicked", G_CALLBACK (export_png), view);
  gtk_container_add (GTK_CONTAINER (bbox), button);

  button = gtk_image_new ();
  gtk_widget_set_size_request (button, 22, -1);
  g_signal_connect (view, "notify::state", G_CALLBACK (view_state_changed),
      button);
  gtk_box_pack_end (GTK_BOX (bbox), button, FALSE, FALSE, 0);

  viewport = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (viewport), widget);

  gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (vbox), viewport);

  /* and insert it into the main window  */
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* make sure that everything, window and label, are visible */
  gtk_widget_show_all (window);
  /* start the main loop */
  gtk_main ();

  return 0;
}
