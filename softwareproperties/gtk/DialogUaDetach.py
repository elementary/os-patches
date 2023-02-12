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
from gettext import gettext as _
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk

from softwareproperties.gtk.utils import (
    setup_ui,
)

class DialogUaDetach:
    def __init__(self, parent, datadir, ua_object):
        """setup up the gtk dialog"""
        setup_ui(self, os.path.join(datadir, "gtkbuilder", "dialog-ua-detach.ui"), domain="software-properties")

        self.ua_object = ua_object
        self.dialog = self.dialog_ua_detach
        self.dialog.set_transient_for(parent)

        self.detaching = False

    def run(self):
        self.dialog.run()
        self.dialog.hide()

    def update_state(self):
        self.button_detach.set_sensitive(not self.detaching)

    def detach(self):
        if self.detaching:
            return

        self.detaching = True
        self.label_detach_error.set_text('')
        def on_reply():
            self.dialog.response(Gtk.ResponseType.OK)
        def on_error(error):
            # FIXME
            print(error)
            self.label_detach_error.set_text(_('Failed to detach. Please try again'))
            self.detaching = False
            self.update_state()
        self.ua_object.Detach(reply_handler=on_reply, error_handler=on_error, dbus_interface='com.canonical.UbuntuAdvantage.Manager', timeout=600)
        self.update_state()

    def on_token_entry_changed(self, entry):
        self.update_state()

    def on_detach_clicked(self, button):
        self.detach()

    def on_cancel_clicked(self, button):
        self.dialog.response(Gtk.ResponseType.CANCEL)
