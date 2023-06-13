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
#include <math.h>

#define MARKER_SIZE 10


static gboolean
draw_center (ClutterCanvas *canvas,
    cairo_t *cr,
    int width,
    int height)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  /* Draw the circle */
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_arc (cr, MARKER_SIZE / 2.0, MARKER_SIZE / 2.0, MARKER_SIZE / 2.0, 0, 2 * M_PI);
  cairo_close_path (cr);

  /* Fill the circle */
  cairo_set_source_rgba (cr, 0.1, 0.1, 0.9, 1.0);
  cairo_fill (cr);
  
  return TRUE;
}


static gboolean
draw_circle (ClutterCanvas *canvas,
    cairo_t *cr,
    int width,
    int height)
{
   /* Draw the circle */
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_arc (cr, MARKER_SIZE, MARKER_SIZE, 0.9 * MARKER_SIZE, 0, 2 * M_PI);
  cairo_close_path (cr);

  /* Stroke the circle */
  cairo_set_line_width (cr, 2.0);
  cairo_set_source_rgba (cr, 0.1, 0.1, 0.7, 1.0);
  cairo_stroke (cr);

  return TRUE;
}


/* The marker is drawn with cairo.  It is composed of 1 static filled circle
 * and 1 stroked circle animated as an echo.
 */
static ClutterActor *
create_marker ()
{
  ClutterActor *marker;
  ClutterActor *bg;
  ClutterContent *canvas;
  ClutterTransition *transition;

  /* Create the marker */
  marker = champlain_custom_marker_new ();

  /* Static filled circle ----------------------------------------------- */
  canvas = clutter_canvas_new ();
  clutter_canvas_set_size (CLUTTER_CANVAS (canvas), MARKER_SIZE, MARKER_SIZE);
  g_signal_connect (canvas, "draw", G_CALLBACK (draw_center), NULL);

  bg = clutter_actor_new ();
  clutter_actor_set_size (bg, MARKER_SIZE, MARKER_SIZE);
  clutter_actor_set_content (bg, canvas);
  clutter_content_invalidate (canvas);
  g_object_unref (canvas);

  /* Add the circle to the marker */
  clutter_actor_add_child (marker, bg);
  clutter_actor_set_position (bg, -0.5 * MARKER_SIZE, -0.5 * MARKER_SIZE);

  /* Echo circle -------------------------------------------------------- */
  canvas = clutter_canvas_new ();
  clutter_canvas_set_size (CLUTTER_CANVAS (canvas), 2 * MARKER_SIZE, 2 * MARKER_SIZE);
  g_signal_connect (canvas, "draw", G_CALLBACK (draw_circle), NULL);

  bg = clutter_actor_new ();
  clutter_actor_set_size (bg, 2 * MARKER_SIZE, 2 * MARKER_SIZE);
  clutter_actor_set_content (bg, canvas);
  clutter_content_invalidate (canvas);
  g_object_unref (canvas);

  /* Add the circle to the marker */
  clutter_actor_add_child (marker, bg);
  clutter_actor_set_pivot_point (bg, 0.5, 0.5);
  clutter_actor_set_position (bg, -MARKER_SIZE, -MARKER_SIZE);

  transition = clutter_property_transition_new ("opacity");
  clutter_actor_set_easing_mode (bg, CLUTTER_EASE_OUT_SINE);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition), 1000);
  clutter_timeline_set_repeat_count (CLUTTER_TIMELINE (transition), -1);
  clutter_transition_set_from (transition, G_TYPE_UINT, 255);
  clutter_transition_set_to (transition, G_TYPE_UINT, 0);
  clutter_actor_add_transition (bg, "animate-opacity", transition);

  transition = clutter_property_transition_new ("scale-x");
  clutter_actor_set_easing_mode (bg, CLUTTER_EASE_OUT_SINE);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition), 1000);
  clutter_timeline_set_repeat_count (CLUTTER_TIMELINE (transition), -1);
  clutter_transition_set_from (transition, G_TYPE_FLOAT, 0.5);
  clutter_transition_set_to (transition, G_TYPE_FLOAT, 2.0);
  clutter_actor_add_transition (bg, "animate-scale-x", transition);

  transition = clutter_property_transition_new ("scale-y");
  clutter_actor_set_easing_mode (bg, CLUTTER_EASE_OUT_SINE);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition), 1000);
  clutter_timeline_set_repeat_count (CLUTTER_TIMELINE (transition), -1);
  clutter_transition_set_from (transition, G_TYPE_FLOAT, 0.5);
  clutter_transition_set_to (transition, G_TYPE_FLOAT, 2.0);
  clutter_actor_add_transition (bg, "animate-scale-y", transition);

  return marker;
}


double lat = 45.466;
double lon = -73.75;

typedef struct
{
  ChamplainView *view;
  ChamplainMarker *marker;
} GpsCallbackData;

static gboolean
gps_callback (GpsCallbackData *data)
{
  lat += 0.005;
  lon += 0.005;
  champlain_view_center_on (data->view, lat, lon);
  champlain_location_set_location (CHAMPLAIN_LOCATION (data->marker), lat, lon);
  return TRUE;
}


int
main (int argc, char *argv[])
{
  ClutterActor *actor, *marker, *stage;
  ChamplainMarkerLayer *layer;
  GpsCallbackData callback_data;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 800, 600);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* Create the map view */
  actor = champlain_view_new ();
  clutter_actor_set_size (CLUTTER_ACTOR (actor), 800, 600);
  clutter_actor_add_child (stage, actor);

  /* Create the marker layer */
  layer = champlain_marker_layer_new_full (CHAMPLAIN_SELECTION_SINGLE);
  clutter_actor_show (CLUTTER_ACTOR (layer));
  champlain_view_add_layer (CHAMPLAIN_VIEW (actor), CHAMPLAIN_LAYER (layer));

  /* Create a marker */
  marker = create_marker ();
  champlain_marker_layer_add_marker (layer, CHAMPLAIN_MARKER (marker));

  /* Finish initialising the map view */
  g_object_set (G_OBJECT (actor), "zoom-level", 12,
      "kinetic-mode", TRUE, NULL);
  champlain_view_center_on (CHAMPLAIN_VIEW (actor), lat, lon);

  /* Create callback that updates the map periodically */
  callback_data.view = CHAMPLAIN_VIEW (actor);
  callback_data.marker = CHAMPLAIN_MARKER (marker);

  g_timeout_add (1000, (GSourceFunc) gps_callback, &callback_data);

  clutter_actor_show (stage);
  clutter_main ();

  return 0;
}
