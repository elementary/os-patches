#
#  Copyright (c) 2021 Canonical Ltd.
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
from gi.repository import Gtk,GLib
from softwareproperties.gtk.utils import setup_ui
from uaclient.api.u.pro.attach.magic.initiate.v1 import initiate
from uaclient.api.u.pro.attach.magic.wait.v1 import MagicAttachWaitOptions, wait
from uaclient.exceptions import MagicAttachTokenError
import threading

class DialogUaAttach:
    def __init__(self, parent, datadir, ua_object):
        """setup up the gtk dialog"""
        setup_ui(self, os.path.join(datadir, "gtkbuilder", "dialog-ua-attach.ui"), domain="software-properties")

        self.ua_object = ua_object
        self.dialog = self.dialog_ua_attach
        self.dialog.set_transient_for(parent)

        self.contract_token = None
        self.attaching = False
        self.poll = None

        self.start_magic_attach()

    def run(self):
        self.dialog.run()
        self.dialog.hide()

    def update_state(self, case = None):
        """
        fail   : called by the attachment callback, and it failed.
        success: called by the attachment callback, and it succeeded.
        expired: called by the token polling when the token expires.
        """
        if self.token_radio.get_active():
            self.confirm.set_sensitive(self.token_field.get_text() != "" and
                                       not self.attaching)
            icon = self.token_status_icon
            spinner = self.token_spinner
            status = self.token_status
        else:
            self.pin_label.set_text(self.pin)
            self.confirm.set_sensitive(self.contract_token != None and
                                       not self.attaching)
            icon = self.pin_status_icon
            spinner = self.pin_spinner
            status = self.pin_status

        if self.attaching:
            spinner.start()
        else:
            spinner.stop()

        def lock_radio_buttons(boolean):
            self.token_radio.set_sensitive(not boolean)
            self.magic_radio.set_sensitive(not boolean)

        lock_radio_buttons(self.attaching)
        self.token_field.set_sensitive(not self.attaching
                                       and self.token_radio.get_active())

        # Unconditionally hide the "other radio section" icon/status.
        # Show icon/status of the "current radio section" only if case is set.
        self.token_status_icon.set_visible(False)
        self.token_status.set_visible(False)
        self.pin_status_icon.set_visible(False)
        self.pin_status.set_visible(False)
        if (case != None):
            icon.set_visible(True)
            status.set_visible(True)

        if (case == "fail"):
            status.set_markup('<span foreground="red">%s</span>' % _('Invalid token'))
            icon.set_from_icon_name('emblem-unreadable', 1)
        elif (case == "success"):
            self.finish()
        elif (case == "pin_validated"):
            status.set_markup('<span foreground="green">%s</span>' % _('Valid token'))
            icon.set_from_icon_name('emblem-default', 1)
            lock_radio_buttons(True)
        elif (case == "expired"):
            status.set_markup(_('Code expired'))
            icon.set_from_icon_name('gtk-dialog-warning', 1)

    def attach(self):
        if self.attaching:
            return

        if self.token_radio.get_active():
            token = self.token_field.get_text()
        else:
            token = self.contract_token

        self.attaching = True
        def on_reply():
            self.attaching = False
            self.update_state("success")
        def on_error(error):
            self.attaching = False
            if self.magic_radio.get_active():
                self.contract_token = None
            self.update_state("fail")
        self.ua_object.Attach(token, reply_handler=on_reply, error_handler=on_error, dbus_interface='com.canonical.UbuntuAdvantage.Manager', timeout=600)
        self.update_state()

    def on_token_typing(self, entry):
        self.confirm.set_sensitive(self.token_field.get_text() != '')

    def on_token_entry_activate(self, entry):
        token = self.token_field.get_text()
        if token != '':
            self.attach()

    def on_confirm_clicked(self, button):
        self.attach()

    def on_cancel_clicked(self, button):
        self.dialog.response(Gtk.ResponseType.CANCEL)

    def poll_for_magic_token(self):
        options = MagicAttachWaitOptions(magic_token=self.req_id)
        try:
            response = wait(options)
            self.contract_token = response.contract_token
            GLib.idle_add(self.update_state, 'pin_validated')
        except MagicAttachTokenError:
            GLib.idle_add(self.update_state, 'expired')
        finally:
            self.poll = None

    def start_magic_attach(self):
        # Already polling, don't bother the server with a new request.
        if self.poll != None or self.contract_token != None:
            return

        self.contract_token = None

        # Request a magic attachment and parse relevants fields from response.
        #  userCode:  The pin the user has to type in <ubuntu.com/pro/attach>;
        #  token:     Identifies the request (used for polling for it).
        try:
            response = initiate()
            self.pin = response.user_code
            self.req_id = response.token
        except Exception as e:
            print(e)
            return
        self.update_state()
        threading.Thread(target=self.poll_for_magic_token, daemon=True).start()

    def on_radio_toggled(self, button):
        self.update_state()

    def on_magic_radio_clicked(self, button):
        self.start_magic_attach()

    def finish(self):
        self.dialog.response(Gtk.ResponseType.OK)
