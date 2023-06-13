#!/usr/bin/env gjs
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

/**
 * Champlain launcher example in Javascript.
 *
 * Dependencies:
 *  * gobject-introspection
 *  * Build Champlain with '--enable-introspection'
 *  * Gjs: https://gitlab.gnome.org/GNOME/gjs/wikis/Home
 *
 * If you installed Champlain in /usr/local you have to run:
 * export GI_TYPELIB_PATH=$GI_TYPELIB_PATH:/usr/local/lib/girepository-1.0/
 */

const Lang = imports.lang;
const Clutter = imports.gi.Clutter;
const Champlain = imports.gi.Champlain;

Clutter.init (null);

const PADDING = 10;

function make_button (text)
{
  let white = new Clutter.Color ({red:0xff, blue:0xff, green:0xff, alpha:0xff});
  let black = new Clutter.Color ({red:0x00, blue:0x00, green:0x00, alpha:0xff});

  let button = new Clutter.Actor ();

  let button_bg = new Clutter.Actor ();
  button_bg.set_background_color (white)
  button.add_child (button_bg);
  button.set_opacity (0xcc);

  let button_text = new Clutter.Text ({font_name:"Sans 10", text:text, color:black});
  button.add_child (button_text);

  let [width, height] = button_text.get_size();
  button_bg.set_size (width + PADDING * 2, height + PADDING * 2);
  button_bg.set_position (0, 0);
  button_text.set_position (PADDING, PADDING);

  return button;
}

function map_view_button_release_cb (actor, event)
{
  // FIXME: it does not print and GLib.printf does not work.
  if (event.button != 1 || event.click_count > 1)
    return false;

  let lat = view.y_to_latitude (event.y);
  let lon = view.x_to_longitude (event.x);

  log ("Map clicked at %f, %f \n", lat, lon);

  return true;
}

let stage = new Clutter.Stage ();
stage.title = "Champlain Javascript Example";
stage.set_size (800, 600);

/* Create the map view */
let view = new Champlain.View();
view.set_size (800, 600);
stage.add_actor (view);

/* Create the buttons */
let buttons = new Clutter.Actor ();
let total_width = 0;
buttons.set_position (PADDING, PADDING);

let button = make_button ("Zoom in");
buttons.add_child (button);
button.set_reactive (true);

let width = button.width;
total_width += width + PADDING;
button.connect ("button-release-event", Lang.bind (view,
    function (actor, event)
      {
        view.zoom_in ();
        return true;
      }));

let button = make_button ("Zoom out");
buttons.add_child (button);
button.set_reactive (true);
button.set_position (total_width, 0);

let width = button.width;
total_width += width + PADDING;
button.connect ("button-release-event", Lang.bind (view,
    function (actor, event)
      {
        view.zoom_out ();
        return true;
      }));

stage.add_child (buttons);

/* Create the markers and marker layer */
let orange= Clutter.Color.new(0xf3,0x94,0x07,0xbb);
let layer=new Champlain.MarkerLayer();
let marker=Champlain.Label.new_with_text("Sample Marker","Serif 14",null,orange);
marker.set_location(45.466, -73.75);
layer.add_marker(marker);
marker.set_reactive(true);
layer.show();
view.add_layer(layer);

/* Connect to the click event */
view.set_reactive (true);
view.connect ("button-release-event", Lang.bind (view,
    map_view_button_release_cb));

/* Finish initialising the map view */
view.zoom_level = 12;
view.kinetic_mode = true;
view.center_on (45.466, -73.75);

stage.connect ("destroy", Clutter.main_quit);

stage.show ();
Clutter.main ();
