# Copyright (C) 2009 Canonical
#
# Authors:
#  Michael Vogt
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; version 3.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

from __future__ import print_function

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk

import logging
LOG=logging.getLogger(__name__)

def setup_ui(self, path, domain):
    # setup ui
    self.builder = Gtk.Builder()
    self.builder.set_translation_domain(domain)
    self.builder.add_from_file(path)
    self.builder.connect_signals(self)
    for o in self.builder.get_objects():
        if issubclass(type(o), Gtk.Buildable):
            name = Gtk.Buildable.get_name(o)
            setattr(self, name, o)
        else:
            logging.debug("can not get name for object '%s'" % o)
