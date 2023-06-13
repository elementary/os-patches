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
#include <markers.h>


ChamplainMarkerLayer *
create_marker_layer (G_GNUC_UNUSED ChamplainView *view, ChamplainPathLayer **path)
{
  ClutterActor *marker;
  ChamplainMarkerLayer *layer;
  ClutterActor *layer_actor;
  ClutterColor orange = { 0xf3, 0x94, 0x07, 0xbb };

  *path = champlain_path_layer_new ();
  layer = champlain_marker_layer_new_full (CHAMPLAIN_SELECTION_SINGLE);
  layer_actor = CLUTTER_ACTOR (layer);

  marker = champlain_label_new_with_text ("Montréal\n<span size=\"xx-small\">Québec</span>",
        "Serif 14", NULL, NULL);
  champlain_label_set_use_markup (CHAMPLAIN_LABEL (marker), TRUE);
  champlain_label_set_alignment (CHAMPLAIN_LABEL (marker), PANGO_ALIGN_RIGHT);
  champlain_label_set_color (CHAMPLAIN_LABEL (marker), &orange);

  champlain_location_set_location (CHAMPLAIN_LOCATION (marker),
      45.528178, -73.563788);
  champlain_marker_layer_add_marker (layer, CHAMPLAIN_MARKER (marker));
  champlain_path_layer_add_node (*path, CHAMPLAIN_LOCATION (marker));

  marker = champlain_label_new_from_file ("icons/emblem-generic.png", NULL);
  champlain_label_set_text (CHAMPLAIN_LABEL (marker), "New York");
  champlain_location_set_location (CHAMPLAIN_LOCATION (marker), 40.77, -73.98);
  champlain_marker_layer_add_marker (layer, CHAMPLAIN_MARKER (marker));
  champlain_path_layer_add_node (*path, CHAMPLAIN_LOCATION (marker));

  marker = champlain_label_new_from_file ("icons/emblem-important.png", NULL);
  champlain_location_set_location (CHAMPLAIN_LOCATION (marker), 47.130885,
      -70.764141);
  champlain_marker_layer_add_marker (layer, CHAMPLAIN_MARKER (marker));
  champlain_path_layer_add_node (*path, CHAMPLAIN_LOCATION (marker));

  marker = champlain_point_new ();
  champlain_location_set_location (CHAMPLAIN_LOCATION (marker), 45.130885,
      -65.764141);
  champlain_marker_layer_add_marker (layer, CHAMPLAIN_MARKER (marker));
  champlain_path_layer_add_node (*path, CHAMPLAIN_LOCATION (marker));

  marker = champlain_label_new_from_file ("icons/emblem-favorite.png", NULL);
  champlain_label_set_draw_background (CHAMPLAIN_LABEL (marker), FALSE);
  champlain_location_set_location (CHAMPLAIN_LOCATION (marker), 45.41484,
      -71.918907);
  champlain_marker_layer_add_marker (layer, CHAMPLAIN_MARKER (marker));
  champlain_path_layer_add_node (*path, CHAMPLAIN_LOCATION (marker));
  
  champlain_marker_layer_set_all_markers_draggable (layer);

  clutter_actor_show (layer_actor);
  return layer;
}
