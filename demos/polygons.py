#!/usr/bin/env python
# Copyright (C) 2012 Pablo Castellano <pablog@gnome.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#

import gi
gi.require_version('Champlain', '0.12')
gi.require_version('Clutter', '1.0')
from gi.repository import Clutter, Champlain
Clutter.init([])

PADDING = 10

def zoom_in(widget, event, view):
	view.zoom_in()
	
def zoom_out(widget, event, view):
	view.zoom_out()
	
def make_button(text):
	black = Clutter.Color.new(0x00, 0x00, 0x00, 0xff)
	white = Clutter.Color.new(0xff, 0xff, 0xff, 0xff)

	button = Clutter.Actor()
	
	button_bg = Clutter.Actor()
	button_bg.set_background_color(white)
	button_bg.set_opacity(0xcc)
	button.add_child(button_bg)
	
	button_text = Clutter.Text.new_full("Sans 10", text, black)
	button.add_child(button_text)
	
	(width, height) = button_text.get_size()
	button_bg.set_size(width + PADDING * 2, height + PADDING * 2)
	button_bg.set_position(0, 0)
	button_text.set_position(PADDING, PADDING)

	return button
	
def append_point(layer, lon, lat):
	coord = Champlain.Coordinate.new_full(lon, lat)
	layer.add_node(coord)

def main_quit(data):
	Clutter.main_quit()


if __name__ == '__main__':
	total_width = 0
	
	stage = Clutter.Stage()
	stage.set_size(800, 600)
	# It complains about the number of arguments passed to main_quit()
	#stage.connect('destroy', Clutter.main_quit)
	stage.connect('destroy', main_quit)
	
	# Create the map view
	view = Champlain.View()
	view.set_size(800, 600)
	stage.add_child(view)

	# Create the buttons
	buttons = Clutter.Actor()
	buttons.set_position(PADDING, PADDING)

	button = make_button('Zoom in')
	buttons.add_child(button)
	button.set_reactive(True)
	(width, height) = button.get_size()
	total_width += width + PADDING
	#button.connect('button-release-event', zoom_in, view)
	button.connect('button-release-event', zoom_in, view)
	
	button = make_button('Zoom out')
	buttons.add_child(button)
	button.set_reactive(True)
	button.set_position(total_width, 0)
	(width, height) = button.get_size()
	button.connect('button-release-event', zoom_out, view)

	stage.add_child(buttons)

	# Draw a line
	layer = Champlain.PathLayer()
	# Cheap approx of Highway 10
	append_point(layer, 45.4104, -73.2846)
	append_point(layer, 45.4178, -73.2239)
	append_point(layer, 45.4176, -73.2181)
	append_point(layer, 45.4151, -73.2126)
	append_point(layer, 45.4016, -73.1926)
	append_point(layer, 45.3994, -73.1877)
	append_point(layer, 45.4000, -73.1815)
	append_point(layer, 45.4151, -73.1218)
	layer.set_stroke_width(4.0)
	view.add_layer(layer)

	dash = [6, 2]
	layer.set_dash(dash)

	# Draw a path
	layer = Champlain.PathLayer()
	append_point(layer, 45.1386, -73.9196)
	append_point(layer, 45.1229, -73.8991)
	append_point(layer, 45.0946, -73.9531)
	append_point(layer, 45.1085, -73.9714)
	append_point(layer, 45.1104, -73.9761)
	layer.set_closed(True)
	layer.set_fill(True)
	layer.set_visible(True)
	view.add_layer(layer)
	
	# Finish initialising the map view
	view.set_zoom_level(8)
	view.set_kinetic_mode(True)
	
	view.center_on(45.466, -73.75)

	stage.show()
	Clutter.main()
