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
using Champlain;

public class Launcher : GLib.Object
{
  private const int PADDING = 10;
  private Champlain.View view;
  private Clutter.Stage stage;

  public Launcher ()
  {
    float width, total_width = 0;

    stage = new Clutter.Stage ();
    stage.title = "Champlain Vala Example";
    stage.set_size (800, 600);
    stage.destroy.connect(Clutter.main_quit);

    /* Create the map view */
    view = new Champlain.View ();
    view.set_size (800, 600);
    stage.add_child (view);

    /* Create the buttons */
    var buttons = new Clutter.Actor ();
    buttons.set_position (PADDING, PADDING);

    var button = make_button ("Zoom in");
    buttons.add_child (button);
    button.reactive = true;
    button.get_size (out width, null);
    total_width += width + PADDING;
    button.button_release_event.connect ((event) => {
        view.zoom_in ();
        return true;
      });

    button = make_button ("Zoom out");
    buttons.add_child (button);
    button.reactive = true;
    button.set_position (total_width, 0);
    button.get_size (out width, null);
    total_width += width + PADDING;
    button.button_release_event.connect ((event) => {
        view.zoom_out ();
        return true;
      });

    stage.add_child (buttons);

    /* Create the markers and marker layer */
    var layer = new  DemoLayer ();
    view.add_layer (layer);

    /* Connect to the click event */
    view.reactive = true;
    view.button_release_event.connect (button_release_cb);

    /* Finish initialising the map view */
    view.zoom_level = 7;
    view.kinetic_mode = true;
    view.center_on (45.466, -73.75);
  }

  public void show ()
  {
    stage.show ();
  }

  private bool button_release_cb (Clutter.ButtonEvent event)
  {
    double lat, lon;

    if (event.button != 1 || event.click_count > 1)
      return false;
      
    lat = view.y_to_latitude (event.y);
    lon = view.x_to_longitude (event.x);

    GLib.print ("Map clicked at %f, %f \n", lat, lon);

    return true;
  }

  public Clutter.Actor make_button (string text)
  {
    Clutter.Color white = { 0xff, 0xff, 0xff, 0xff };
    Clutter.Color black = { 0x00, 0x00, 0x00, 0xff };
    float width, height;

    var button = new Clutter.Actor ();

    var button_bg = new Clutter.Actor ();
    button_bg.set_background_color (white);
    button.add_child (button_bg);
    button_bg.opacity = 0xcc;

    var button_text = new Clutter.Text.full ("Sans 10", text, black);
    button.add_child (button_text);
    button_text.get_size (out width, out height);

    button_bg.set_size (width + PADDING * 2, height + PADDING * 2);
    button_bg.set_position (0, 0);
    button_text.set_position (PADDING, PADDING);

    return button;
  }

  public static int main (string[] args)
  {
    if (Clutter.init (ref args) != InitError.SUCCESS)
      return 1;

    var launcher = new Launcher ();
    launcher.show ();
    Clutter.main ();
    return 0;
  }
}

