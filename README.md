libchamplain - a map widget
===========================

libchamplain is a Gtk widget displaying zoomable and pannable maps that can be
loaded from various network sources. It supports overlay layers, markers, and
custom elements displayed on top of the maps. The library is written in C but
other language mappings are also available thanks to GObject-Introspection.

libchamplain depends on the following libraries:
* [glib](https://gitlab.gnome.org/GNOME/glib)
* [gtk](https://gitlab.gnome.org/GNOME/gtk)
* [clutter](https://gitlab.gnome.org/GNOME/clutter)
* [clutter-gtk](https://gitlab.gnome.org/GNOME/clutter-gtk)
* [libsoup](https://gitlab.gnome.org/GNOME/libsoup)
* [cairo](https://www.cairographics.org)
* [sqlite](https://www.sqlite.org)

To build libchamplain from sources using [meson](https://mesonbuild.com), run:
```
meson _builddir; cd _builddir; ninja; sudo ninja install
```

It is possible to specify compilation options defined in `meson_options.txt`
by using the `-D` flag, e.g.:
```
meson _builddir -Dgtk_doc=true -Ddemos=true
```

The **repository and bug report** page is at:
* https://gitlab.gnome.org/GNOME/libchamplain

Release **tarballs** can be downloaded from:
* https://download.gnome.org/sources/libchamplain

For simple examples how to use the library, check the `demos` directory;
in particular, the `minimal-gtk.c` and `minimal.py` demos are good starting
points to see how to get the most basic map application running.

Full **documentation** can be found at:
* https://gnome.pages.gitlab.gnome.org/libchamplain/champlain

The official **mailing list** is at:
* https://mail.gnome.org/mailman/listinfo/libchamplain-list

The official **IRC channel** is at:
* irc://irc.freenode.org/#champlain

---

libchamplain is licensed under the terms of the GNU Lesser General Public
License, version 2.1 or (at your option) later.
