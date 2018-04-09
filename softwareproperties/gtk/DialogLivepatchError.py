#
#  Copyright (c) 2017-2018 Canonical
#
#  Authors:
#       Andrea Azzarone <andrea.azzarone@canonical.com>
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

from softwareproperties.gtk.utils import (
    setup_ui,
)


class DialogLivepatchError:

    RESPONSE_SETTINGS = 100
    RESPONSE_IGNORE = 101

    def __init__(self, parent, datadir):
        """setup up the gtk dialog"""
        self.parent = parent

        setup_ui(self, os.path.join(datadir, "gtkbuilder",
                                    "dialog-livepatch-error.ui"), domain="software-properties")

        self.dialog = self.messagedialog_livepatch
        self.dialog.use_header_bar = True
        self.dialog.set_transient_for(parent)

    def run(self, error, show_settings_button):
        self.dialog.format_secondary_markup(
            "The error was: \"%s\"" % error.strip())
        self.button_settings.set_visible(show_settings_button)
        res = self.dialog.run()
        self.dialog.hide()
        return res

    def on_button_settings_clicked(self, b, d=None):
        self.dialog.response(self.RESPONSE_SETTINGS)

    def on_button_ignore_clicked(self, b, d=None):
        self.dialog.response(self.RESPONSE_IGNORE)
