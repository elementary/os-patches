# -*- coding: utf-8 -*-

from __future__ import division, print_function, unicode_literals

import os

from gi.repository import GObject, Gtk

from Onboard.definitions import StatusIconProviderEnum
import Onboard.utils as utils

### Logging ###
import logging
_logger = logging.getLogger("Indicator")
###############

### Config Singleton ###
from Onboard.Config import Config
config = Config()
########################


class Indicator(GObject.GObject):

    __gsignals__ = {
        str('quit-onboard') : (GObject.SIGNAL_RUN_LAST, GObject.TYPE_NONE, ())
    }

    "Keyboard window managed by this indicator"
    _keyboard_window = None

    "Encapsulated appindicator instance"
    _indicator = None

    "Encapsulated GtkStatusIcon instance"
    _status_icon = None

    "Menu attached to indicator"
    _menu = None

    def __new__(cls, *args, **kwargs):
        """
        Singleton magic.
        """
        if not hasattr(cls, "self"):
            cls.self = GObject.GObject.__new__(cls, args, kwargs)
            #object.__new__(cls, args, kwargs)
            cls.self.init()
        return cls.self

    def __init__(self):
        """
        This constructor is still called multiple times.
        Do nothing here and use the singleton constructor "init()" instead.
        """
        pass

    def init(self):

        GObject.GObject.__init__(self)

        self._menu = Gtk.Menu()

        # This updates the menu in gnome-shell and gnome-classic, 
        # but not in unity or unity2D.
        self._menu.connect_object("show", Indicator.update_menu_items, self)

        self._show_label = _("_Show Onboard")
        self._hide_label = _("_Hide Onboard")

        show_item = Gtk.MenuItem.new_with_label(self._show_label)
        show_item.set_use_underline(True)
        show_item.connect_object("activate",
            Indicator._toggle_keyboard_window_state, self)
        self._menu.append(show_item)

        if not config.lockdown.disable_preferences:
            settings_item = Gtk.ImageMenuItem.new_with_label(Gtk.STOCK_PREFERENCES)
            settings_item.set_use_stock(True)
            settings_item.connect("activate", self._on_settings_clicked)
            self._menu.append(settings_item)

        if not config.lockdown.disable_quit:
            quit_item = Gtk.ImageMenuItem.new_with_label(Gtk.STOCK_QUIT)
            quit_item.set_use_stock(True)
            quit_item.connect("activate", self._emit_quit_onboard)
            self._menu.append(quit_item)
        self._menu.show_all()

        if config.status_icon_provider == StatusIconProviderEnum.GtkStatusIcon:
            self._init_status_icon()
        else:
            try:
                self._init_indicator()
            except ImportError as ex:
                _logger.info("AppIndicator not available, falling back on"
                             " GtkStatusIcon:" + utils.unicode_str(ex))
                self._init_status_icon()
        self.set_visible(False)

    def set_keyboard_window(self, keyboard_window):
        self._keyboard_window = keyboard_window

    def update_menu_items(self):
        if self._keyboard_window:
            if self._keyboard_window.is_visible():
                self._menu.get_children()[0].set_label(self._hide_label)
            else:
                self._menu.get_children()[0].set_label(self._show_label)

    def _init_indicator(self):
        from gi.repository import AppIndicator3 as AppIndicator
        self._indicator = AppIndicator.Indicator.new(
            "Onboard",
            "onboard",
            AppIndicator.IndicatorCategory.APPLICATION_STATUS)
        self._indicator.set_icon_full("onboard", _("Onboard on-screen keyboard"))

        self._indicator.set_menu(self._menu)
        self._indicator.set_secondary_activate_target( \
                                                self._menu.get_children()[0])

    def _init_status_icon(self):
        self._status_icon = Gtk.StatusIcon(icon_name="onboard")
        self._status_icon.connect_object("activate",
            Indicator._toggle_keyboard_window_state, self)
        self._status_icon.connect("popup-menu", self._on_status_icon_popup_menu)

    def set_visible(self, visible):
        if self._status_icon:
            # Then we've falled back to using GtkStatusIcon
            self._status_icon.set_visible(visible)
        else:
            self._set_indicator_active(visible)

    def _on_settings_clicked(self, widget):
        utils.run_script("sokSettings")

    def _menu_position_func(self, menu, *args):
        # Work-around for gi annotation bug in gtk-3.0:
        # gtk_status_icon_position_menu() doesn't mark 'push_in' as inout
        # which is required for any (*GtkMenuPositionFunc)
        if len(args) == 1:    # in Precise
            status_icon, = args
        elif len(args) == 2:  # in <=Oneiric?
            push_in, status_icon = args
        return Gtk.StatusIcon.position_menu(self._menu, status_icon)

    def _on_status_icon_popup_menu(self, status_icon, button, activate_time):
        """
        Callback called when status icon right clicked.  Produces menu.
        """
        self._menu.popup(None, None,
                         self._menu_position_func, status_icon,
                         button, activate_time)

    def _toggle_keyboard_window_state(self):
        self._keyboard_window.keyboard_widget.toggle_visible()

    def _set_indicator_active(self, active):
        try:
            from gi.repository import AppIndicator3 as AppIndicator
        except ImportError:
            pass
        else:
            if active:
                self._indicator.set_status(AppIndicator.IndicatorStatus.ACTIVE)
            else:
                self._indicator.set_status(AppIndicator.IndicatorStatus.PASSIVE)

    def _emit_quit_onboard(self, data=None):
        _logger.debug("Entered _emit_quit_onboard")
        self.emit("quit-onboard")

    def is_appindicator(self):
        if self._indicator:
            return True
        else:
            return False

