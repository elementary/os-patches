#
#  Copyright (c) 2018 Canonical
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

from gettext import gettext as _
from softwareproperties.gtk.utils import (
    setup_ui,
)

import gi
gi.require_version('Goa', '1.0')
from gi.repository import Gio, GLib, Goa, GObject, Gtk
import logging


class DialogAuth:

    def __init__(self, parent, datadir):
        """setup up the gtk dialog"""
        self.parent = parent

        setup_ui(self, os.path.join(datadir, "gtkbuilder",
                                    "dialog-auth.ui"), domain="software-properties")
        self.label_title.set_max_width_chars(50)

        self.dialog = self.dialog_auth
        self.dialog.use_header_bar = True
        self.dialog.set_transient_for(parent)

        self.listboxrow_new_account.account = None

        self.account = None
        self.dispose_on_new_account = False
        self.goa_client = Goa.Client.new_sync(None)

        self.listbox_accounts.connect('row-activated', self._listbox_accounts_row_activated_cb)

        # Be ready to other accounts
        self.goa_client.connect('account-added', self._account_added_cb)
        self.goa_client.connect('account-removed', self._account_removed_cb)

        self._setup_listbox_accounts()
        self._check_ui()

    def run(self):
        res = self.dialog.run()
        self.dialog.hide()
        return res

    def _check_ui(self):
        rows = self.listbox_accounts.get_children()
        has_accounts = len(rows) > 1

        if has_accounts:
           title = _('To continue choose an Ubuntu Single Sign-On account.')
           new_account = _('Use another account…')
        else:
           title = _('To continue you need an Ubuntu Single Sign-On account.')
           new_account = _('Sign In…')

        self.label_title.set_text(title)
        self.label_new_account.set_markup('<b>{}</b>'.format(new_account))

    def _setup_listbox_accounts(self):
        for obj in self.goa_client.get_accounts():
            account = obj.get_account()
            if self._is_account_supported(account):
                self._add_account(account)

    def _is_account_supported(self, account):
        return account.props.provider_type == 'ubuntusso'

    def _add_account(self, account):
        row = self._create_row(account)
        self.listbox_accounts.prepend(row)
        self._check_ui()

    def _remove_account(self, account):
        for row in self.listbox_accounts.get_children():
            if row.account == account:
                row.destroy()
                self._check_ui()
                break

    def _build_dbus_params(self, action, arg):
        builder = GLib.VariantBuilder.new(GLib.VariantType.new('av'))

        if action is None and arg is None:
            s = GLib.Variant.new_string('')
            v = GLib.Variant.new_variant(s)
            builder.add_value(v)
        else:
            if action is not None:
                s = GLib.Variant.new_string(action)
                v = GLib.Variant.new_variant(s)
                builder.add_value(v)
            if arg is not None:
                s = GLib.Variant.new_string(arg)
                v = GLib.Variant.new_variant(s)
                builder.add_value(v)

        array = GLib.Variant.new_tuple(GLib.Variant.new_string('online-accounts'), builder.end())
        array = GLib.Variant.new_variant(array)

        param = GLib.Variant.new_tuple(
            GLib.Variant.new_string('launch-panel'),
            GLib.Variant.new_array(GLib.VariantType.new('v'), [array]),
            GLib.Variant.new_array(GLib.VariantType.new('{sv}'), None))
        return param

    def _spawn_goa_with_args(self, action, arg):
        proxy = Gio.DBusProxy.new_for_bus_sync(Gio.BusType.SESSION,
            Gio.DBusProxyFlags.NONE, None,
            'org.gnome.ControlCenter',
            '/org/gnome/ControlCenter',
            'org.gtk.Actions', None)

        param = self._build_dbus_params(action, arg)
        timeout = 10*60*1000 # 10 minutes should be enough to create an account
        proxy.call_sync('Activate', param, Gio.DBusCallFlags.NONE, timeout, None)

    def _create_row(self, account):
        identity = account.props.presentation_identity
        provider_name = account.props.provider_name

        row = Gtk.ListBoxRow.new()
        row.show()

        hbox = Gtk.Box.new(Gtk.Orientation.HORIZONTAL, 6)
        hbox.set_hexpand(True)
        hbox.show()

        image = Gtk.Image.new_from_icon_name('avatar-default', Gtk.IconSize.DIALOG)
        image.set_pixel_size(48)
        image.show()
        hbox.pack_start(image, False, False, 0)

        vbox = Gtk.Box.new(Gtk.Orientation.VERTICAL, 2)
        vbox.set_valign(Gtk.Align.CENTER)
        vbox.show()
        hbox.pack_start(vbox, False, False, 0)

        if identity:
            ilabel = Gtk.Label.new()
            ilabel.set_halign(Gtk.Align.START)
            ilabel.set_markup('<b>{}</b>'.format(identity))
            ilabel.show()
            vbox.pack_start(ilabel, True, True, 0)

        if provider_name:
            plabel = Gtk.Label.new()
            plabel.set_halign(Gtk.Align.START)
            plabel.set_markup('<small><span foreground=\"#555555\">{}</span></small>'.format(provider_name))
            plabel.show()
            vbox.pack_start(plabel, True, True, 0)

        warning_icon = Gtk.Image.new_from_icon_name('dialog-warning-symbolic', Gtk.IconSize.BUTTON)
        warning_icon.set_no_show_all(True)
        warning_icon.set_margin_end (15)
        hbox.pack_end(warning_icon, False, False, 0)

        row.add(hbox)

        account.bind_property('attention-needed', warning_icon, 'visible',
            GObject.BindingFlags.DEFAULT | GObject.BindingFlags.SYNC_CREATE)

        row.account = account
        return row

    # Signals handlers
    def _listbox_accounts_row_activated_cb(self, listbox, row):
        account = row.account

        if account is None:
            # TODO (azzar1): there is no easy way to put this to false
            # if the user close the windows without adding an account.
            # We need to discuss with goa's upstream to support such usercases
            try:
                self._spawn_goa_with_args('add', 'ubuntusso')
                self.dispose_on_new_account = True
            except GLib.Error as e:
                logging.warning ('Failed to spawing gnome-control-center: %s', e.message)
        else:
            if account.props.attention_needed:
                try:
                    self._spawn_goa_with_args(account.props.id, None)
                except GLib.Error as e:
                    logging.warning ('Failed to spawing gnome-control-center: %s', e.message)
            else:
                self.account = account
                self.dialog.response(Gtk.ResponseType.OK)

    def _account_added_cb(self, goa_client, goa_object):
        account = goa_object.get_account()
        if not self._is_account_supported(account):
            return
        if not self.dispose_on_new_account:
            self._add_account(account)
        else:
            self.account = account
            self.dialog.response(Gtk.ResponseType.OK)

    def _account_removed_cb(self, goa_client, goa_object):
        account = goa_object.get_account()
        if self._is_account_supported(account):
            self._remove_account(account)



