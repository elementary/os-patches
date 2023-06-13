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

#include <champlain/champlain.h>
#include "markers.h"

#define PADDING 10

static gboolean
map_view_button_release_cb (G_GNUC_UNUSED ClutterActor *actor,
    ClutterButtonEvent *event,
    ChamplainView *view)
{
  gdouble lat, lon;

  if (event->button != 1 || event->click_count > 1)
    return FALSE;

  lon = champlain_view_x_to_longitude (view, event->x);
  lat = champlain_view_y_to_latitude (view, event->y);

  g_print ("Map clicked at %f, %f \n", lat, lon);

  return TRUE;
}


static gboolean
zoom_in (G_GNUC_UNUSED ClutterActor *actor,
    G_GNUC_UNUSED ClutterButtonEvent *event,
    ChamplainView *view)
{
  champlain_view_zoom_in (view);
  return TRUE;
}


static gboolean
zoom_out (G_GNUC_UNUSED ClutterActor *actor,
    G_GNUC_UNUSED ClutterButtonEvent *event,
    ChamplainView *view)
{
  champlain_view_zoom_out (view);
  return TRUE;
}


static ClutterActor *
make_button (char *text)
{
  ClutterActor *button, *button_bg, *button_text;
  ClutterColor white = { 0xff, 0xff, 0xff, 0xff };
  ClutterColor black = { 0x00, 0x00, 0x00, 0xff };
  gfloat width, height;

  button = clutter_actor_new ();

  button_bg = clutter_actor_new ();
  clutter_actor_set_background_color (button_bg, &white);
  clutter_actor_add_child (button, button_bg);
  clutter_actor_set_opacity (button_bg, 0xcc);

  button_text = clutter_text_new_full ("Sans 10", text, &black);
  clutter_actor_add_child (button, button_text);
  clutter_actor_get_size (button_text, &width, &height);

  clutter_actor_set_size (button_bg, width + PADDING * 2, height + PADDING * 2);
  clutter_actor_set_position (button_bg, 0, 0);
  clutter_actor_set_position (button_text, PADDING, PADDING);

  return button;
}

#define TILE_SQUARE_SIZE 64

static gboolean
draw_background_tile (ClutterCanvas *canvas,
    cairo_t *cr,
    int width,
    int height)
{
  cairo_pattern_t *pat;
  gint no_of_squares_x = width / TILE_SQUARE_SIZE;
  gint no_of_squares_y = height / TILE_SQUARE_SIZE;
  gint row, column;

  /* Create the background tile */
  pat = cairo_pattern_create_linear (width / 2.0, 0.0, width, height / 2.0);
  cairo_pattern_add_color_stop_rgb (pat, 0, 0.662, 0.662, 0.662);
  cairo_set_source (cr, pat);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);
  cairo_pattern_destroy (pat);

  /* Filling the tile */
  cairo_set_source_rgb (cr, 0.811, 0.811, 0.811);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

  for (row = 0; row < no_of_squares_y; ++row)
    {
      for (column = 0; column < no_of_squares_x; column++)
        {
          /* drawing square alternatively */
          if ((row % 2 == 0 && column % 2 == 0) ||
              (row % 2 != 0 && column % 2 != 0))
            cairo_rectangle (cr,
                column * TILE_SQUARE_SIZE,
                row * TILE_SQUARE_SIZE,
                TILE_SQUARE_SIZE,
                TILE_SQUARE_SIZE);
        }
      cairo_fill (cr);
    }
  cairo_stroke (cr);
  
  return TRUE;
}


int
main (int argc,
    char *argv[])
{
  ClutterActor *actor, *stage, *buttons, *button;
  ChamplainMarkerLayer *layer;
  ChamplainPathLayer *path;
  gfloat width, total_width = 0;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 800, 600);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* Create the map view */
  actor = champlain_view_new ();
  clutter_actor_set_size (CLUTTER_ACTOR (actor), 800, 600);
  clutter_actor_add_child (stage, actor);

  /* Create the buttons */
  buttons = clutter_actor_new ();
  clutter_actor_set_position (buttons, PADDING, PADDING);

  button = make_button ("Zoom in");
  clutter_actor_add_child (buttons, button);
  clutter_actor_set_reactive (button, TRUE);
  clutter_actor_get_size (button, &width, NULL);
  total_width += width + PADDING;
  g_signal_connect (button, "button-release-event",
      G_CALLBACK (zoom_in),
      actor);

  button = make_button ("Zoom out");
  clutter_actor_add_child (buttons, button);
  clutter_actor_set_reactive (button, TRUE);
  clutter_actor_set_position (button, total_width, 0);
  clutter_actor_get_size (button, &width, NULL);
  g_signal_connect (button, "button-release-event",
      G_CALLBACK (zoom_out),
      actor);

  clutter_actor_add_child (stage, buttons);

  ClutterContent *canvas;
  canvas = clutter_canvas_new ();
  clutter_canvas_set_size (CLUTTER_CANVAS (canvas), 512, 256);
  g_signal_connect (canvas, "draw", G_CALLBACK (draw_background_tile), NULL);
  clutter_content_invalidate (canvas);
  champlain_view_set_background_pattern (CHAMPLAIN_VIEW (actor), canvas);

  /* Create the markers and marker layer */
  layer = create_marker_layer (CHAMPLAIN_VIEW (actor), &path);
  champlain_view_add_layer (CHAMPLAIN_VIEW (actor), CHAMPLAIN_LAYER (layer));

  /* Connect to the click event */
  clutter_actor_set_reactive (actor, TRUE);
  g_signal_connect (actor, "button-release-event",
      G_CALLBACK (map_view_button_release_cb),
      actor);

  /* Finish initialising the map view */
  g_object_set (G_OBJECT (actor), "zoom-level", 12,
      "kinetic-mode", TRUE, NULL);
  champlain_view_center_on (CHAMPLAIN_VIEW (actor), 45.466, -73.75);

  clutter_actor_show (stage);
  clutter_main ();

  return 0;
}
