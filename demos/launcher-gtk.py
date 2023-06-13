#!/usr/bin/env python
# -*- coding: utf-8 -*-

import gi
gi.require_version('Champlain', '0.12')
gi.require_version('GtkChamplain', '0.12')
gi.require_version('GtkClutter', '1.0')
from gi.repository import GtkClutter, Clutter
GtkClutter.init([])  # Must be initialized before importing those:
from gi.repository import GObject, Gtk, Champlain, GtkChamplain, Pango

import os
import sys
os.chdir(os.path.dirname(os.path.abspath(sys.argv[0])))

class LauncherGTK:

    def __init__(self):
        self.window = Gtk.Window()
        self.window.set_border_width(10)
        self.window.set_title("libchamplain Gtk+ demo (python introspection)")
        self.window.connect("destroy", Gtk.main_quit)

        vbox = Gtk.VBox(False, 10)

        embed = GtkChamplain.Embed()

        self.view = embed.get_view()
        self.view.set_reactive(True)
        self.view.connect('button-release-event', self.mouse_click_cb,
            self.view)

        self.view.set_property('kinetic-mode', True)
        self.view.set_property('zoom-level', 5)

        scale = Champlain.Scale()
        scale.connect_view(self.view)
        self.view.bin_layout_add(scale, Clutter.BinAlignment.START,
            Clutter.BinAlignment.END)

        license = self.view.get_license_actor()
        license.set_extra_text("Don't eat cereals with orange juice\nIt tastes bad")

        self.view.center_on(45.466, -73.75)

        self.layer = self.create_marker_layer(self.view)
        self.view.add_layer(self.layer)
        self.layer.hide_all_markers()

        self.path_layer = Champlain.PathLayer()
        # Cheap approx of Highway 10
        self.add_node(self.path_layer, 45.4095, -73.3197)
        self.add_node(self.path_layer, 45.4104, -73.2846)
        self.add_node(self.path_layer, 45.4178, -73.2239)
        self.add_node(self.path_layer, 45.4176, -73.2181)
        self.add_node(self.path_layer, 45.4151, -73.2126)
        self.add_node(self.path_layer, 45.4016, -73.1926)
        self.add_node(self.path_layer, 45.3994, -73.1877)
        self.add_node(self.path_layer, 45.4000, -73.1815)
        self.add_node(self.path_layer, 45.4151, -73.1218)
        self.view.add_layer(self.path_layer)

        embed.set_size_request(640, 480)

        bbox = Gtk.HBox(False, 10)
        button = Gtk.Button(stock=Gtk.STOCK_ZOOM_IN)
        button.connect("clicked", self.zoom_in)
        bbox.add(button)

        button = Gtk.Button(stock=Gtk.STOCK_ZOOM_OUT)
        button.connect("clicked", self.zoom_out)
        bbox.add(button)

        button = Gtk.ToggleButton(label="Markers")
        button.set_active(False)
        button.connect("toggled", self.toggle_layer)
        bbox.add(button)

        combo = Gtk.ComboBox()
        map_source_factory = Champlain.MapSourceFactory.dup_default()
        liststore = Gtk.ListStore(str, str)
        for source in map_source_factory.get_registered():
            liststore.append([source.get_id(), source.get_name()])
        combo.set_model(liststore)
        cell = Gtk.CellRendererText()
        combo.pack_start(cell, False)
        combo.add_attribute(cell, 'text', 1)
        combo.connect("changed", self.map_source_changed)
        combo.set_active(0)
        bbox.add(combo)

        self.spinbutton = Gtk.SpinButton.new_with_range(0, 20, 1)
        self.spinbutton.connect("changed", self.zoom_changed)
        self.view.connect("notify::zoom-level", self.map_zoom_changed)
        self.spinbutton.set_value(5)
        bbox.add(self.spinbutton)

        button = Gtk.Image()
        self.view.connect("notify::state", self.view_state_changed, button)
        bbox.pack_end(button, False, False, 0)

        vbox.pack_start(bbox, expand=False, fill=False, padding=0)
        vbox.add(embed)

        self.window.add(vbox)

        self.window.show_all()
    
    def add_node(self, path_layer, lat, lon):
        coord = Champlain.Coordinate.new_full(lat, lon)
        path_layer.add_node(coord)

    def zoom_in(self, widget):
        self.view.zoom_in()

    def zoom_out(self, widget):
        self.view.zoom_out()

    def toggle_layer(self, widget):
        if widget.get_active():
            self.path_layer.show()
            self.layer.animate_in_all_markers()
        else:
            self.path_layer.hide()
            self.layer.animate_out_all_markers()

    def mouse_click_cb(self, actor, event, view):
        x, y = event.x, event.y
        lon, lat = view.x_to_longitude(x), view.y_to_latitude(y)
        print("Mouse click at: %f %f" % (lat, lon))
        return True

    def zoom_changed(self, widget):
        self.view.set_property("zoom-level", self.spinbutton.get_value_as_int())

    def map_source_changed(self, widget):
        model = widget.get_model()
        iter = widget.get_active_iter()
        id = model.get_value(iter, 0)
        map_source_factory = Champlain.MapSourceFactory.dup_default()
        source = map_source_factory.create_cached_source(id)
        self.view.set_property("map-source", source)

    def map_zoom_changed(self, widget, value):
        self.spinbutton.set_value(self.view.get_property("zoom-level"))

    def view_state_changed(self, view, paramspec, image):
        state = view.get_state()
        if state == Champlain.State.LOADING:
            image.set_from_stock(Gtk.STOCK_NETWORK, Gtk.IconSize.BUTTON)
        else:
            image.clear()

    def marker_button_release_cb(self, actor, event, view):
        if event.button != 1 and event.click_count > 1:
            return False

        print("Montreal was clicked\n")
        return True

    def create_marker_layer(self, view):
        orange = Clutter.Color.new(0xf3, 0x94, 0x07, 0xbb)
        layer = Champlain.MarkerLayer()

        marker = Champlain.Label.new_with_text(
            "Montréal\n<span size=\"xx-small\">Québec</span>", "Serif 14", None,
            orange)
        marker.set_use_markup(True)
        marker.set_alignment(Pango.Alignment.RIGHT)
        marker.set_color(orange)

        marker.set_location(45.528178, -73.563788)
        layer.add_marker(marker)
        marker.set_reactive(True)
        marker.connect("button-release-event", self.marker_button_release_cb, view)

        marker = Champlain.Label.new_from_file(
            "icons/emblem-generic.png")
        marker.set_text("New York")
        marker.set_location(40.77, -73.98)
        layer.add_marker(marker)

        marker = Champlain.Label.new_from_file(
            "icons/emblem-important.png")
        marker.set_location(47.130885, -70.764141)
        layer.add_marker(marker)

        marker = Champlain.Label.new_from_file(
            "icons/emblem-favorite.png")
        marker.set_draw_background(False)
        marker.set_location(45.41484, -71.918907)
        layer.add_marker(marker)

        marker = Champlain.Label.new_from_file(
            "icons/emblem-new.png")
        marker.set_draw_background(False)
        marker.set_location(50.639663, 5.570798)
        layer.add_marker(marker)

        layer.set_all_markers_draggable()
        layer.show()
        return layer


if __name__ == "__main__":
    GObject.threads_init()
    LauncherGTK()
    Gtk.main()
