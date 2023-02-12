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

import dbus
import os
from gettext import gettext as _
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import GdkPixbuf, Gio, Gtk
from softwareproperties.gtk.utils import current_distro, is_dark_theme

from .DialogUaAttach import DialogUaAttach
from .DialogUaDetach import DialogUaDetach
from .DialogUaFipsEnable import DialogUaFipsEnable

class UaService:
    def __init__(self, bus_object, name, entitled, status):
        self.bus_object = bus_object
        self.name = name
        self.entitled = entitled
        self.status = status
        self.request_in_progress = False

class UbuntuProPage(object):

    def __init__(self, parent):
        self._parent = parent

        self.stack_ua_attach = parent.stack_ua_attach
        self.box_ua_attached = parent.box_ua_attached
        self.box_ua_unattached = parent.box_ua_unattached
        self.stack_ua_main = parent.stack_ua_main
        self.box_ua_options = parent.box_ua_options
        self.box_ua_fips_setup = parent.box_ua_fips_setup
        self.switch_ua_esm_infra = parent.switch_ua_esm_infra
        self.label_ua_esm_infra = parent.label_ua_esm_infra
        self.label_ua_esm_infra_error = parent.label_ua_esm_infra_error
        self.label_ua_esm_infra_error_messages = {
            "enable": _("Could not enable ESM Infra. Please try again."),
            "disable": _("Could not disable ESM Infra. Please try again."),
        }
        self.switch_ua_esm_apps = parent.switch_ua_esm_apps
        self.label_ua_esm_apps = parent.label_ua_esm_apps
        self.label_ua_esm_apps_error = parent.label_ua_esm_apps_error
        self.label_ua_esm_apps_error_messages = {
            "enable": _("Could not enable ESM Apps. Please try again."),
            "disable": _("Could not disable ESM Apps. Please try again."),
        }
        self.switch_ua_livepatch = parent.switch_ua_livepatch
        self.checkbutton_livepatch_topbar = parent.checkbutton_livepatch_topbar
        self.label_ua_livepatch = parent.label_ua_livepatch
        self.label_ua_livepatch_error = parent.label_ua_livepatch_error
        self.label_ua_livepatch_error_messages = {
            "enable": _("Could not enable Livepatch. Please try again."),
            "disable": _("Could not disable Livepatch. Please try again."),
        }
        self.button_ua_fips = parent.button_ua_fips
        self.label_ua_fips_status = parent.label_ua_fips_status
        self.label_ua_fips_description = parent.label_ua_fips_description
        self.button_ua_usg = parent.button_ua_usg
        self.label_ua_usg_button = parent.label_ua_usg_button
        self.label_ua_usg_status = parent.label_ua_usg_status
        self.label_ua_usg_description = parent.label_ua_usg_description

        if is_dark_theme(self.stack_ua_attach):
            ubuntu_pro_logo = GdkPixbuf.Pixbuf.new_from_file_at_scale(os.path.join(parent.datadir, 'ubuntu-pro-logo-dark.svg'), -1, 50, True)
        else:
            ubuntu_pro_logo = GdkPixbuf.Pixbuf.new_from_file_at_scale(os.path.join(parent.datadir, 'ubuntu-pro-logo.svg'), -1, 50, True)
        parent.image_ubuntu_pro_logo.set_from_pixbuf(ubuntu_pro_logo)

        parent.button_ua_attach.connect('clicked', self.on_button_ua_attach_clicked)
        parent.button_ua_detach.connect('clicked', self.on_button_ua_detach_clicked)
        self.on_ua_esm_infra_changed_handler = self.switch_ua_esm_infra.connect('notify::active', self.on_ua_esm_infra_changed)
        self.on_ua_esm_apps_changed_handler = self.switch_ua_esm_apps.connect('notify::active', self.on_ua_esm_apps_changed)
        self.on_ua_livepatch_changed_handler = self.switch_ua_livepatch.connect('notify::active', self.on_ua_livepatch_changed)
        parent.button_ua_fips.connect('clicked', self.on_button_ua_fips_clicked)
        parent.button_ua_usg.connect('clicked', self.on_button_ua_usg_clicked)
        parent.expander_compliance_and_hardening.connect('notify::expanded', self.on_compliance_and_hardening_expand_changed)

        # Set date dependent labels
        distro = current_distro()
        if distro.eol_esm is not None:
            eol_year = distro.eol_esm.year
            self.label_ua_esm_infra.set_markup(_('<b>ESM Infra</b> provides security updates for over 2,300 Ubuntu Main packages until %d.') % eol_year)
            self.label_ua_esm_apps.set_markup(_('<b>ESM Apps</b>; provides security updates for over 23,000 Ubuntu Universe packages until %d.') % eol_year)
        else:
            self.label_ua_esm_infra.set_markup(_('<b>ESM Infra</b> provides security updates for over 2,300 Ubuntu Main packages.'))
            self.label_ua_esm_apps.set_markup(_('<b>ESM Apps</b>; provides security updates for over 23,000 Ubuntu Universe packages.'))

        self.update_notifier_settings = None
        source = Gio.SettingsSchemaSource.get_default()
        if source is not None:
            schema = source.lookup('com.ubuntu.update-notifier', True)
            if schema is not None:
                settings = Gio.Settings.new('com.ubuntu.update-notifier')
                if schema.has_key('show-livepatch-status-icon'):
                    self.update_notifier_settings = settings

        if self.update_notifier_settings is not None:
            self.on_checkbutton_livepatch_topbar_toggled_handler = self.checkbutton_livepatch_topbar.connect('toggled', self.on_checkbutton_livepatch_topbar_toggled)
            self.on_update_notifier_settings_changed_handler = self.update_notifier_settings.connect('changed::show-livepatch-status-icon', self.on_update_notifier_settings_changed)
            self.on_update_notifier_settings_changed(self.update_notifier_settings, 'show-livepatch-status-icon')

        bus = dbus.SystemBus()
        self.ua_object = bus.get_object('com.canonical.UbuntuAdvantage', '/com/canonical/UbuntuAdvantage/Manager')

        # Monitor services
        self.attached = False
        self.services = {}
        def on_interfaces_added(path, interfaces_and_properties):
            if path == '/com/canonical/UbuntuAdvantage/Manager':
                self.attached = interfaces_and_properties['com.canonical.UbuntuAdvantage.Manager']['Attached']
            elif path.startswith('/com/canonical/UbuntuAdvantage/Services/'):
                properties = interfaces_and_properties.get('com.canonical.UbuntuAdvantage.Service')
                bus_object = bus.get_object('com.canonical.UbuntuAdvantage', path)
                self.services[path] = UaService(bus_object, properties['Name'], properties['Entitled'], properties['Status'])
            self.update_status()
        def on_interfaces_removed(path, interfaces):
            if 'com.canonical.UbuntuAdvantage.Service' in interfaces:
                self.services.pop(path)
            self.update_status()
        def on_properties_changed(interface, changed_properties, invalidated_properties, path):
            def get_property(properties, name, default):
                value = properties.get(name)
                if value is None:
                    value = default
                return value
            if path == '/com/canonical/UbuntuAdvantage/Manager' and interface == 'com.canonical.UbuntuAdvantage.Manager':
                self.attached = get_property(changed_properties, 'Attached', self.attached)
            elif path.startswith('/com/canonical/UbuntuAdvantage/Services/') and interface == 'com.canonical.UbuntuAdvantage.Service':
                service = self.services[path]
                service.entitled = get_property(changed_properties, 'Entitled', service.entitled)
                service.status = get_property(changed_properties, 'Status', service.status)
            self.update_status()
        object_manager_object = bus.get_object('com.canonical.UbuntuAdvantage', '/')
        object_manager_object.connect_to_signal('InterfacesAdded', on_interfaces_added, dbus_interface='org.freedesktop.DBus.ObjectManager')
        object_manager_object.connect_to_signal('InterfacesRemoved', on_interfaces_removed, dbus_interface='org.freedesktop.DBus.ObjectManager')
        bus.add_signal_receiver(on_properties_changed, bus_name='com.canonical.UbuntuAdvantage', signal_name='PropertiesChanged', dbus_interface='org.freedesktop.DBus.Properties', path_keyword='path')
        objects = object_manager_object.GetManagedObjects(dbus_interface='org.freedesktop.DBus.ObjectManager')
        for path in objects:
            on_interfaces_added(path, objects[path])

    def get_service(self, name):
        for service in self.services.values():
            if service.name == name:
                return service
        return None

    def update_status(self):
        if self.attached:
            self.stack_ua_attach.set_visible_child(self.box_ua_attached)
        else:
            self.stack_ua_attach.set_visible_child(self.box_ua_unattached)

        def entitled_to_service(service):
            return service is not None and service.entitled == 'yes'
        def service_request_in_progress(service):
            return service is not None and service.request_in_progress
        def service_is_enabled(service):
            return service is not None and service.status == 'enabled'

        def update_switch(switch, service, handler):
            if service is not None and service.request_in_progress:
                return
            switch.handler_block(handler)
            switch.set_active(service_is_enabled(service))
            switch.handler_unblock(handler)

        esm_infra_service = self.get_service('esm-infra')
        for widget in [self.switch_ua_esm_infra, self.label_ua_esm_infra, self.label_ua_esm_infra_error]:
            widget.set_sensitive(entitled_to_service(esm_infra_service) and not service_request_in_progress(esm_infra_service))
        update_switch(self.switch_ua_esm_infra, esm_infra_service, self.on_ua_esm_infra_changed_handler)

        esm_apps_service = self.get_service('esm-apps')
        for widget in [self.switch_ua_esm_apps, self.label_ua_esm_apps, self.label_ua_esm_apps_error]:
            widget.set_sensitive(entitled_to_service(esm_apps_service) and not service_request_in_progress(esm_apps_service))
        update_switch(self.switch_ua_esm_apps, esm_apps_service, self.on_ua_esm_apps_changed_handler)

        livepatch_service = self.get_service('livepatch')
        for widget in [self.switch_ua_livepatch, self.label_ua_livepatch, self.label_ua_livepatch_error]:
            widget.set_sensitive(entitled_to_service(livepatch_service) and not service_request_in_progress(livepatch_service))
        update_switch(self.switch_ua_livepatch, livepatch_service, self.on_ua_livepatch_changed_handler)
        self.checkbutton_livepatch_topbar.set_sensitive(self.update_notifier_settings is not None and self.switch_ua_livepatch.get_active())

        fips_service = self.get_service('fips')
        fips_updates_service = self.get_service('fips-updates')
        fips_in_progress = service_request_in_progress(fips_service) or service_request_in_progress(fips_updates_service)
        self.button_ua_fips.set_sensitive(entitled_to_service(fips_service) and not fips_in_progress)
        if fips_in_progress:
            self.stack_ua_main.set_visible_child(self.box_ua_fips_setup)
        else:
            self.stack_ua_main.set_visible_child(self.box_ua_options)

        usg_service = self.get_service('usg')
        if not service_request_in_progress(usg_service):
            if service_is_enabled(usg_service):
                self.label_ua_usg_button.set_label(_('Disable _USG'))
            else:
                self.label_ua_usg_button.set_label(_('Enable _USG'))
        self.button_ua_usg.set_sensitive(entitled_to_service(usg_service) and not service_request_in_progress(usg_service))

    def on_button_ua_attach_clicked(self, button):
        dialog = DialogUaAttach(self._parent.window_main, self._parent.datadir, self.ua_object)
        dialog.run()

    def on_button_ua_detach_clicked(self, button):
        dialog = DialogUaDetach(self._parent.window_main, self._parent.datadir, self.ua_object)
        dialog.run()

    def set_service_enabled(self, service_name, enabled, error_label, error_label_messages):
        if error_label is not None:
            error_label.set_visible(False)
        service = self.get_service(service_name)
        if service is None:
            return
        def on_reply():
            service.request_in_progress = False
            self.update_status()
        def on_error(error):
            print(error)
            if error_label is not None:
                error_label.set_visible(True)
                if enabled:
                    error_label.set_label(error_label_messages["enable"])
                else:
                    error_label.set_label(error_label_messages["disable"])
            service.request_in_progress = False
            self.update_status()
        if enabled:
            service.bus_object.Enable(reply_handler=on_reply, error_handler=on_error, dbus_interface='com.canonical.UbuntuAdvantage.Service', timeout=600)
        else:
            service.bus_object.Disable(reply_handler=on_reply, error_handler=on_error, dbus_interface='com.canonical.UbuntuAdvantage.Service', timeout=600)
        service.request_in_progress = True
        self.update_status()

    def on_ua_esm_infra_changed(self, switch, param):
        self.set_service_enabled('esm-infra', self.switch_ua_esm_infra.get_active(), self.label_ua_esm_infra_error, self.label_ua_esm_infra_error_messages)

    def on_ua_esm_apps_changed(self, switch, param):
        self.set_service_enabled('esm-apps', self.switch_ua_esm_apps.get_active(), self.label_ua_esm_apps_error, self.label_ua_esm_apps_error_messages)

    def on_ua_livepatch_changed(self, switch, param):
        self.set_service_enabled('livepatch', self.switch_ua_livepatch.get_active(), self.label_ua_livepatch_error, self.label_ua_livepatch_error_messages)

    def on_checkbutton_livepatch_topbar_toggled(self, button):
        self.update_notifier_settings.handler_block(self.on_update_notifier_settings_changed_handler)
        self.update_notifier_settings.set_boolean('show-livepatch-status-icon', self.checkbutton_livepatch_topbar.get_active())
        self.update_notifier_settings.handler_unblock(self.on_update_notifier_settings_changed_handler)

    def on_update_notifier_settings_changed(self, settings, key):
        self.checkbutton_livepatch_topbar.handler_block(self.on_checkbutton_livepatch_topbar_toggled_handler)
        self.checkbutton_livepatch_topbar.set_active(self.update_notifier_settings.get_boolean('show-livepatch-status-icon'))
        self.checkbutton_livepatch_topbar.handler_unblock(self.on_checkbutton_livepatch_topbar_toggled_handler)

    def on_button_ua_fips_clicked(self, button):
        dialog = DialogUaFipsEnable(self._parent.window_main, self._parent.datadir, self.ua_object)
        service_name = dialog.run()
        if service_name is None:
            return

        dialog = Gtk.MessageDialog(parent=self._parent.window_main,
                                   flags=Gtk.DialogFlags.MODAL,
                                   type=Gtk.MessageType.QUESTION,
                                   message_format=None)
        dialog.add_button(_('No, go back'), Gtk.ResponseType.CANCEL)
        dialog.add_button(_('Enable FIPS'), Gtk.ResponseType.OK)
        dialog.set_markup(_('Enabling FIPS could take a few minutes. This action cannot be reversed. Are you sure you want to enable FIPS?'))
        result = dialog.run()
        dialog.destroy()
        if result != Gtk.ResponseType.OK:
            return

        self.set_service_enabled(service_name, True, None, None)

    def on_button_ua_usg_clicked(self, button):
        service = self.get_service('usg')
        is_enabled = service is not None and service.status == 'enabled'
        self.set_service_enabled('usg', not is_enabled, None, None)

    def on_compliance_and_hardening_expand_changed(self, widget, param):
        self._parent.window_main.resize(1, 1)
