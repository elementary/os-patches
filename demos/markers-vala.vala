/*
 * Copyright (C) 2010 Simon Wenner <simon@wenner.ch>
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

using GLib;
using Clutter;

class DemoLayer : Champlain.MarkerLayer
{
  public DemoLayer ()
  {
    Clutter.Color orange = { 0xf3, 0x94, 0x07, 0xbb };
    var marker = new Champlain.Label.with_text (
        "Montréal\n<span size=\"xx-small\">Québec</span>",
        "Serif 14", null, null);
    marker.set_use_markup (true);
    marker.set_alignment (Pango.Alignment.RIGHT);
    marker.set_color (orange);
    marker.set_location (45.528178, -73.563788);
    add_marker (marker);

    try {
      marker = new Champlain.Label.from_file (
          "icons/emblem-generic.png");
    } catch (GLib.Error e) {
      GLib.warning ("%s", e.message);
    }
    marker.set_text ("New York");
    marker.set_location (40.77, -73.98);
    add_marker (marker);

    try {
      marker = new Champlain.Label.from_file (
          "icons/emblem-important.png");
    } catch (GLib.Error e) {
      GLib.warning ("%s", e.message);
    }
    marker.set_location (47.130885, -70.764141);
    add_marker (marker);

    try {
      marker = new Champlain.Label.from_file (
          "icons/emblem-favorite.png");
    } catch (GLib.Error e) {
      GLib.warning ("%s", e.message);
    }
    marker.set_draw_background (false);
    marker.set_location (45.41484, -71.918907);
    add_marker (marker);
  }
}

