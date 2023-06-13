#!/usr/bin/env python

# To run this example, you need to set the GI_TYPELIB_PATH environment
# variable to point to the gir directory:
#
# export GI_TYPELIB_PATH=$GI_TYPELIB_PATH:/usr/local/lib/girepository-1.0/

import gi
gi.require_version('GtkChamplain', '0.12')
gi.require_version('GtkClutter', '1.0')
from gi.repository import GtkClutter
from gi.repository import Gtk, Gdk, GtkChamplain

class KeyboardMapping:

    def __init__(self):
        GtkClutter.init([])

        window = Gtk.Window(type=Gtk.WindowType.TOPLEVEL)
        window.connect("destroy", Gtk.main_quit)
        window.connect("key-press-event", self.on_key_press)

        self.widget = GtkChamplain.Embed()
        self.widget.set_size_request(640, 480)
        self.view = self.widget.get_view()
        self.view.set_horizontal_wrap(True)

        window.add(self.widget)
        window.show_all()

    def on_key_press(self, widget, ev):
        deltax = self.widget.get_allocation().width / 4
        deltay = self.widget.get_allocation().height / 4
        if ev.keyval == Gdk.KEY_Left:
            self.scroll(-deltax, 0)
        elif ev.keyval == Gdk.KEY_Right:
            self.scroll(deltax, 0)
        elif ev.keyval == Gdk.KEY_Up:
            self.scroll(0, -deltay)
        elif ev.keyval == Gdk.KEY_Down:
            self.scroll(0, deltay)
        elif ev.keyval == Gdk.KEY_plus or ev.keyval == Gdk.KEY_KP_Add:
            self.view.zoom_in()
        elif ev.keyval == Gdk.KEY_minus or ev.keyval == Gdk.KEY_KP_Subtract:
            self.view.zoom_out()
        else:
            return False
        return True

    def scroll(self, deltax, deltay):
        lat = self.view.get_center_latitude()
        lon = self.view.get_center_longitude()
        
        x = self.view.longitude_to_x(lon) + deltax
        y = self.view.latitude_to_y(lat) + deltay
        
        lon = self.view.x_to_longitude(x)
        lat = self.view.y_to_latitude(y)
      
        self.view.center_on(lat, lon)
        #self.view.go_to(lat, lon)


if __name__ == "__main__":
    KeyboardMapping()
    Gtk.main()
