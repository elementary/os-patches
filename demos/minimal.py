#!/usr/bin/env python

# To run this example, you need to set the GI_TYPELIB_PATH environment
# variable to point to the gir directory:
#
# export GI_TYPELIB_PATH=$GI_TYPELIB_PATH:/usr/local/lib/girepository-1.0/

import gi
gi.require_version('GtkChamplain', '0.12')
gi.require_version('GtkClutter', '1.0')
from gi.repository import GtkClutter
from gi.repository import Gtk, GtkChamplain

GtkClutter.init([])

window = Gtk.Window(type=Gtk.WindowType.TOPLEVEL)
window.connect("destroy", Gtk.main_quit)

widget = GtkChamplain.Embed()
widget.set_size_request(640, 480)

window.add(widget)
window.show_all()

Gtk.main()
