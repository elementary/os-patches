#
#  Copyright (c) 2022 Canonical Ltd.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
#  USA

import os
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk

from softwareproperties.gtk.utils import (
    setup_ui,
)

class DialogUaFipsEnable:
    def __init__(self, parent, datadir, ua_object):
        """setup up the gtk dialog"""
        setup_ui(self, os.path.join(datadir, "gtkbuilder", "dialog-ua-fips-enable.ui"), domain="software-properties")

        self.ua_object = ua_object
        self.dialog = self.dialog_ua_fips_enable
        self.dialog.set_transient_for(parent)

    def run(self):
        result = self.dialog.run()
        self.dialog.hide()
        if result == Gtk.ResponseType.OK:
            if self.radio_fips_with_updates.get_active():
                return 'fips-updates'
            else:
                return 'fips'
        else:
            return None

    def on_continue_clicked(self, button):
        self.dialog.response(Gtk.ResponseType.OK)

    def on_cancel_clicked(self, button):
        self.dialog.response(Gtk.ResponseType.CANCEL)
