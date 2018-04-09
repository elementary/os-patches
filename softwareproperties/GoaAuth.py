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

import gi
gi.require_version('Goa', '1.0')
gi.require_version('Secret', '1')
from gi.repository import GLib, Goa, GObject, Secret
import logging

SECRETS_SCHEMA = Secret.Schema.new('com.ubuntu.SotwareProperties',
                                   Secret.SchemaFlags.NONE,
                                   {'key': Secret.SchemaAttributeType.STRING})

class GoaAuth(GObject.GObject):

    # Properties
    logged = GObject.Property(type=bool, default=False)

    def __init__(self):
        GObject.GObject.__init__(self)

        self.goa_client = Goa.Client.new_sync(None)
        self.account = None
        self._load()

    def login(self, account):
        assert(account)
        self._update_state(account)
        self._store()

    def logout(self):
        self._update_state(None)
        self._store()

    @GObject.Property
    def username(self):
        if self.account is None:
            return None
        return self.account.props.presentation_identity

    @GObject.Property
    def token(self):
        if self.account is None:
            return None

        obj = self.goa_client.lookup_by_id(self.account.props.id)
        if obj is None:
            return None

        pbased = obj.get_password_based()
        if pbased is None:
            return None

        return pbased.call_get_password_sync('livepatch')

    def _update_state_from_account_id(self, account_id):
        if account_id:
            # Make sure the account-id is valid
            obj = self.goa_client.lookup_by_id(account_id)
            if obj is None:
                self._update_state(None)
                return

            account = obj.get_account()
            if account is None:
                self._update_state(None)
                return

            self._update_state(account)
        else:
            self._update_state(None)

    def _update_state(self, account):
        self.account = account
        if self.account is None:
            self.logged = False
        else:
            try:
                account.call_ensure_credentials_sync(None)
            except Exception:
                self.logged = False
            else:
                self.account.connect('notify::attention-needed', lambda o, v: self.logout())
                self.logged = True

    def _load(self):
        # Retrieve the stored account-id
        try:
            account_id = Secret.password_lookup_sync(SECRETS_SCHEMA, {'key': 'account-id'}, None)
            self._update_state_from_account_id(account_id)
        except GLib.GError as e:
            logging.warning ("Connection to session keyring failed: {}".format(e))

    def _store(self):
        if self.logged:
            account_id = self.account.props.id
            Secret.password_store(SECRETS_SCHEMA, {'key': 'account-id'}, None, 'com.ubuntu.SoftwareProperties', account_id)
        else:
            Secret.password_clear(SECRETS_SCHEMA, {'key': 'account-id'}, None, None, None)
