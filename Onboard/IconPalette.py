#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright © 2008 Francesco Fumanti <francesco.fumanti@gmx.net>
#
# This file is part of Onboard.
#
# Onboard is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# Onboard is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

from __future__ import division, print_function, unicode_literals

from os.path import join
from traceback import print_exc

from gi.repository import GObject, Gdk, Gtk

import cairo

from Onboard.utils       import CallOnce, Rect, round_corners, roundrect_arc, \
                                hexstring_to_float, Timer, Fade
from Onboard.WindowUtils import WindowManipulator, WindowRectPersist, \
                                Orientation, set_unity_property, \
                                DwellProgress
from Onboard.TouchInput  import InputSequence, POINTER_SEQUENCE
from Onboard.KeyGtk      import RectKey

### Logging ###
import logging
_logger = logging.getLogger("IconPalette")
###############

### Config Singleton ###
from Onboard.Config import Config
config = Config()
########################


class IconPalette(WindowRectPersist, WindowManipulator, Gtk.Window):
    """
    Class that creates a movable and resizable floating window without
    decorations. The window shows the icon of onboard scaled to fit to the
    window and a resize grip that honors the desktop theme in use.

    Onboard offers an option to the user to make the window appear
    whenever the user hides the onscreen keyboard. The user can then
    click on the window to hide it and make the onscreen keyboard
    reappear.
    """

    __gsignals__ = {
        str('activated') : (GObject.SIGNAL_RUN_LAST, GObject.TYPE_NONE, ())
    }

    """ Minimum size of the IconPalette """
    MINIMUM_SIZE = 20

    _layout_view = None

    def __init__(self):

        self._visible = False
        self._force_to_top = False
        self._last_pos = None

        self._dwell_progress = DwellProgress()
        self._dwell_begin_timer = None
        self._dwell_timer = None
        self._no_more_dwelling = False

        Gtk.Window.__init__(self,
                            type_hint=self._get_window_type_hint(),
                            skip_taskbar_hint=True,
                            skip_pager_hint=True,
                            has_resize_grip=False,
                            urgency_hint=False,
                            decorated=False,
                            accept_focus=False,
                            opacity=0.75,
                            width_request=self.MINIMUM_SIZE,
                            height_request=self.MINIMUM_SIZE)

        WindowRectPersist.__init__(self)
        WindowManipulator.__init__(self)

        self.set_keep_above(True)

        # use transparency if available
        visual = Gdk.Screen.get_default().get_rgba_visual()
        if visual:
            self.set_visual(visual)

        # set up event handling
        self.add_events(Gdk.EventMask.BUTTON_PRESS_MASK |
                        Gdk.EventMask.BUTTON_RELEASE_MASK |
                        Gdk.EventMask.POINTER_MOTION_MASK)

        self.connect("button-press-event",   self._on_button_press_event)
        self.connect("motion-notify-event",  self._on_motion_notify_event)
        self.connect("button-release-event", self._on_button_release_event)
        self.connect("draw",                 self._on_draw)
        self.connect("configure-event",      self._on_configure_event)
        self.connect("realize",              self._on_realize_event)
        self.connect("unrealize",            self._on_unrealize_event)
        self.connect("enter-notify-event",   self._on_mouse_enter)
        self.connect("leave-notify-event",   self._on_mouse_leave)

        # default coordinates of the iconpalette on the screen
        self.set_min_window_size(self.MINIMUM_SIZE, self.MINIMUM_SIZE)
        #self.set_default_size(1, 1)  # no flashing on left screen edge in unity
        self.restore_window_rect()

        # Realize the window. Test changes to this in all supported
        # environments. It's all too easy to have the icp not show up reliably.
        self.update_window_options()
        self.hide()

        once = CallOnce(100).enqueue  # call at most once per 100ms
        rect_changed = lambda x: once(self._on_config_rect_changed)
        config.icp.position_notify_add(rect_changed)
        config.icp.size_notify_add(rect_changed)

        config.icp.resize_handles_notify_add(lambda x: self.update_resize_handles())

        self.update_sticky_state()
        self.update_resize_handles()

    def cleanup(self):
        WindowRectPersist.cleanup(self)

    def set_layout_view(self, view):
        self._layout_view = view
        self.queue_draw()

    def get_color_scheme(self):
        if self._layout_view:
            return self._layout_view.get_color_scheme()
        return None

    def _on_configure_event(self, widget, event):
        self.update_window_rect()

    def on_drag_initiated(self):
        self.stop_save_position_timer()
        self._stop_dwelling()

    def on_drag_done(self):
        self.update_window_rect()
        self.start_save_position_timer()
        self._no_more_dwelling = True

    def _on_realize_event(self, user_data):
        """ Gdk window created """
        set_unity_property(self)
        if config.is_force_to_top():
            self.set_override_redirect(True)
        self.restore_window_rect(True)

    def _on_unrealize_event(self, user_data):
        """ Gdk window destroyed """
        self.set_type_hint(self._get_window_type_hint())

    def _get_window_type_hint(self):
        if config.is_force_to_top():
            return Gdk.WindowTypeHint.NORMAL
        else:
            return Gdk.WindowTypeHint.UTILITY

    def update_window_options(self, startup = False):
        if not config.xid_mode:   # not when embedding

            # (re-)create the gdk window?
            force_to_top = config.is_force_to_top()

            if self._force_to_top != force_to_top:
                self._force_to_top = force_to_top

                visible = self._visible # visible before?

                if self.get_realized(): # not starting up?
                    self.hide()
                    self.unrealize()

                self.realize()

                if visible:
                    self.show()

    def update_sticky_state(self):
        if not config.xid_mode:
            if config.get_sticky_state():
                self.stick()
            else:
                self.unstick()

    def update_resize_handles(self):
        """ Tell WindowManipulator about the active resize handles """
        self.set_drag_handles(config.icp.resize_handles)

    def get_drag_threshold(self):
        """ Overload for WindowManipulator """
        return config.get_drag_threshold()

    def _on_button_press_event(self, widget, event):
        """
        Save the pointer position.
        """
        if event.button == 1 and event.window == self.get_window():
            self.enable_drag_protection(True)
            sequence = InputSequence()
            sequence.init_from_button_event(event)
            self.handle_press(sequence, move_on_background = True)
            if self.is_moving():
                self.reset_drag_protection() # force threshold
        return False

    def _on_motion_notify_event(self, widget, event):
        """
        Move the window if the pointer has moved more than the DND threshold.
        """
        sequence = InputSequence()
        sequence.init_from_motion_event(event)
        self.handle_motion(sequence, fallback = True)
        self.set_drag_cursor_at((event.x, event.y))

        # start dwelling if nothing else is going on
        point = (event.x, event.y)
        hit = self.hit_test_move_resize(point)
        if hit is None:
            if not self.is_drag_initiated() and \
               not self._is_dwelling() and \
               not self._no_more_dwelling and \
               not config.is_hover_click_active() and \
               not config.lockdown.disable_dwell_activation:
                self._start_dwelling()
        else:
            self._stop_dwelling()  # allow resizing in peace

        return False

    def _on_button_release_event(self, widget, event):
        """
        Save the window geometry, hide the IconPalette and
        emit the "activated" signal.
        """
        result = False

        if event.button == 1 and \
           event.window == self.get_window() and \
           not self.is_drag_active():
            self.emit("activated")
            result = True

        self.stop_drag()
        self.set_drag_cursor_at((event.x, event.y))

        return result

    def _on_mouse_enter(self, widget, event):
        pass

    def _on_mouse_leave(self, widget, event):
        self._stop_dwelling()
        self._no_more_dwelling = False

    def _on_draw(self, widget, cr):
        """
        Draw the onboard icon.
        """
        if not Gtk.cairo_should_draw_window(cr, self.get_window()):
            return False

        rect = Rect(0.0, 0.0, float(self.get_allocated_width()),
                              float(self.get_allocated_height()))
        color_scheme = self.get_color_scheme()

        # clear background
        cr.save()
        cr.set_operator(cairo.OPERATOR_CLEAR)
        cr.paint()
        cr.restore()

        # draw background color
        background_rgba = list(color_scheme.get_icon_rgba("background"))

        if Gdk.Screen.get_default().is_composited():
            background_rgba[3] *= 0.75
            cr.set_source_rgba(*background_rgba)

            corner_radius = min(rect.w, rect.h) * 0.1

            roundrect_arc(cr, rect, corner_radius)
            cr.fill()

            # decoration frame
            line_rect = rect.deflate(2)
            cr.set_line_width(2)
            roundrect_arc(cr, line_rect, corner_radius)
            cr.stroke()
        else:
            cr.set_source_rgba(*background_rgba)
            cr.paint()

        # draw themed icon
        self._draw_themed_icon(cr, rect, color_scheme)

        # draw dwell progress
        rgba = [0.8, 0.0, 0.0, 0.5]
        bg_rgba = [0.1, 0.1, 0.1, 0.5]
        if color_scheme:
            key  = RectKey("icon0") # take dwell color from the first icon "key"
            rgba = color_scheme.get_key_rgba(key, "dwell-progress")
            rgba[3] = min(0.75, rgba[3]) # more transparency

            key  = RectKey("icon1")
            bg_rgba = color_scheme.get_key_rgba(key, "fill")
            bg_rgba[3] = min(0.75, rgba[3]) # more transparency

        dwell_rect = rect.grow(0.5)
        self._dwell_progress.draw(cr, dwell_rect, rgba, bg_rgba)

        return True

    def _draw_themed_icon(self, cr, icon_rect, color_scheme):
        """ draw themed icon """
        keys = [RectKey("icon" + str(i)) for i in range(4)]

        # Default colors for the case when none of the icon keys
        # are defined in the color scheme.
        background_rgba =  [1.0, 1.0, 1.0, 1.0]
        fill_rgbas      = [[0.9, 0.7, 0.0, 0.75],
                           [1.0, 1.0, 1.0, 1.0],
                           [1.0, 1.0, 1.0, 1.0],
                           [0.0, 0.54, 1.0, 1.0]]
        stroke_rgba     =  [0.0, 0.0, 0.0, 1.0]
        label_rgba      =  [0.0, 0.0, 0.0, 1.0]

        themed = False
        if color_scheme:
            if any(color_scheme.is_key_in_scheme(key) for key in keys):
                themed = True

        # four rounded rectangles
        rects = Rect(0.0, 0.0, 100.0, 100.0).deflate(5) \
                                            .subdivide(2, 2, 6)
        cr.save()
        cr.scale(icon_rect.w / 100., icon_rect.h / 100.0)
        cr.translate(icon_rect.x, icon_rect.y)
        cr.select_font_face ("sans-serif")
        cr.set_line_width(2)

        for i, key in enumerate(keys):
            rect = rects[i]

            if themed:
                fill_rgba   = color_scheme.get_key_rgba(key, "fill")
                stroke_rgba = color_scheme.get_key_rgba(key, "stroke")
                label_rgba  = color_scheme.get_key_rgba(key, "label")
            else:
                fill_rgba   = fill_rgbas[i]

            roundrect_arc(cr, rect, 5)
            cr.set_source_rgba(*fill_rgba)
            cr.fill_preserve()

            cr.set_source_rgba(*stroke_rgba)
            cr.stroke()

            if i == 0 or i == 3:
                if i == 0:
                    letter = "O"
                else:
                    letter = "B"

                cr.set_font_size(25)
                x_bearing, y_bearing, _width, _height, \
                x_advance, y_advance = cr.text_extents(letter)
                r = rect.align_rect(Rect(0, 0, _width, _height),
                                         0.3, 0.33)
                cr.move_to(r.x - x_bearing, r.y - y_bearing)
                cr.set_source_rgba(*label_rgba)
                cr.show_text(letter)
                cr.new_path()

        cr.restore()

    def show(self):
        """
        Override Gtk.Widget.hide() to save the window geometry.
        """
        Gtk.Window.show(self)
        self.move_resize(*self.get_rect()) # sync with WindowRectTracker
        self._visible = True

    def hide(self):
        """
        Override Gtk.Widget.hide() to save the window geometry.
        """
        Gtk.Window.hide(self)
        self._visible = False

    def _on_config_rect_changed(self):
        """ Gsettings position or size changed """
        orientation = self.get_screen_orientation()
        rect = self.read_window_rect(orientation)
        if self.get_rect() != rect:
            self.restore_window_rect()

    def read_window_rect(self, orientation):
        """
        Read orientation dependent rect.
        Overload for WindowRectPersist.
        """
        if orientation == Orientation.LANDSCAPE:
            co = config.icp.landscape
        else:
            co = config.icp.portrait
        rect = Rect(co.x, co.y, co.width, co.height)
        return rect

    def write_window_rect(self, orientation, rect):
        """
        Write orientation dependent rect.
        Overload for WindowRectPersist.
        """
        # There are separate rects for normal and rotated screen (tablets).
        if orientation == Orientation.LANDSCAPE:
            co = config.icp.landscape
        else:
            co = config.icp.portrait

        co.delay()
        co.x, co.y, co.width, co.height = rect
        co.apply()

    def _is_dwelling(self):
        return bool(self._dwell_begin_timer) and \
               (self._dwell_begin_timer.is_running() or \
               self._dwell_progress.is_dwelling())

    def _start_dwelling(self):
        self._stop_dwelling()
        self._dwell_begin_timer = Timer(1.5, self._on_dwell_begin_timer)
        self._no_more_dwelling = True

    def _stop_dwelling(self):
        if self._dwell_begin_timer:
            self._dwell_begin_timer.stop()
            if self._dwell_timer:
                self._dwell_timer.stop()
                self._dwell_progress.stop_dwelling()
                self.queue_draw()

    def _on_dwell_begin_timer(self):
        self._dwell_progress.start_dwelling()
        self._dwell_timer = Timer(0.025, self._on_dwell_timer)
        return False

    def _on_dwell_timer(self):
        self._dwell_progress.opacity, done = \
            Fade.sin_fade(self._dwell_progress.dwell_start_time, 0.3, 0, 1.0)
        self.queue_draw()
        if self._dwell_progress.is_done():
            if not self.is_drag_active():
                self.emit("activated")
                self.stop_drag()
            return False
        return True


def icp_activated(self):
    Gtk.main_quit()

if __name__ == "__main__":
    icp = IconPalette()
    icp.show()
    icp.connect("activated", icp_activated)
    Gtk.main()


