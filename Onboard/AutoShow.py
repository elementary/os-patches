# -*- coding: utf-8 -*-
""" GTK specific keyboard class """

from __future__ import division, print_function, unicode_literals

from Onboard.AtspiStateTracker import AtspiStateTracker
from Onboard.utils             import Rect, Timer, unicode_str

### Logging ###
import logging
_logger = logging.getLogger("AutoShow")
###############

### Config Singleton ###
from Onboard.Config import Config
config = Config()
########################


class AutoShow(object):
    """
    Auto-show and hide Onboard.
    """

    # Delay from the last focus event until the keyboard is shown/hidden.
    # Raise it to reduce unnecessary transitions (flickering).
    # Lower it for more immediate reactions.
    SHOW_REACTION_TIME = 0.0
    HIDE_REACTION_TIME = 0.3

    _lock_visible = False
    _frozen = False
    _keyboard_widget = None
    _state_tracker = AtspiStateTracker()

    def __init__(self, keyboard_widget):
        self._keyboard_widget = keyboard_widget
        self._auto_show_timer = Timer()
        self._thaw_timer = Timer()
        self._active_accessible = None

    def cleanup(self):
        self._auto_show_timer.stop()
        self._thaw_timer.stop()

    def enable(self, enable):
        if enable:
            self._state_tracker.connect("text-entry-activated",
                                        self._on_text_entry_activated)
            self._state_tracker.connect("text-caret-moved",
                                        self._on_text_caret_moved)
        else:
            self._state_tracker.disconnect("text-entry-activated",
                                        self._on_text_entry_activated)
            self._state_tracker.disconnect("text-caret-moved",
                                        self._on_text_caret_moved)

        if enable:
            self._lock_visible = False
            self._frozen = False

    def is_frozen(self):
        return self._frozen

    def freeze(self, thaw_time = None):
        """
        Stop showing and hiding the keyboard window.
        thaw_time in seconds, None to freeze forever.
        """
        self._frozen = True
        self._thaw_timer.stop()
        if not thaw_time is None:
            self._thaw_timer.start(thaw_time, self._on_thaw)

        # Discard pending hide/show actions.
        self._auto_show_timer.stop()

    def thaw(self, thaw_time = None):
        """
        Allow hiding and showing the keyboard window again.
        thaw_time in seconds, None to thaw immediately.
        """
        self._thaw_timer.stop()
        if thaw_time is None:
            self._thaw()
        else:
            self._thaw_timer.start(thaw_time, self._on_thaw)

    def _on_thaw(self):
        self._thaw_timer.stop()
        self._frozen = False
        return False

    def lock_visible(self, lock, thaw_time = 1.0):
        """
        Lock window permanetly visible in response to the user showing it.
        Optionally freeze hiding/showing for a limited time.
        """
        # Permanently lock visible.
        self._lock_visible = lock

        # Temporarily stop showing/hiding.
        if thaw_time:
            self.freeze(thaw_time)

        # Leave the window in its current state,
        # discard pending hide/show actions.
        self._auto_show_timer.stop()

        # Stop pending auto-repositioning
        if lock:
            window = self._keyboard_widget.get_kbd_window()
            if window:
                window.stop_auto_position()

    def _on_text_caret_moved(self, event):
        """
        Show the keyboard on click of an already focused text entry
        (LP: 1078602). Do this only for single line text entries to
        still allow clicking longer documents without having onboard show up.
        """
        if config.auto_show.enabled and \
           not self._keyboard_widget.is_visible():

            accessible = self._active_accessible
            if accessible:
                if self._state_tracker.is_single_line():
                    self._on_text_entry_activated(accessible)

    def _on_text_entry_activated(self, accessible):
        window = self._keyboard_widget.get_kbd_window()
        self._active_accessible = accessible
        active = bool(accessible)

        # show/hide the keyboard window
        if not active is None:
            # Always allow to show the window even when locked.
            # Mitigates right click on unity-2d launcher hiding
            # onboard before _lock_visible is set (Precise).
            if self._lock_visible:
                active = True

            if not self.is_frozen():
                self.show_keyboard(active)

            # The active accessible changed, stop trying to
            # track the position of the previous one.
            # -> less erratic movement during quick focus changes
            if window:
                window.stop_auto_position()

        # reposition the keyboard window
        if active and \
           not accessible is None and \
           not self._lock_visible and \
           not self.is_frozen():
            if window:
                window.auto_position()

    def show_keyboard(self, show):
        """ Begin AUTO_SHOW or AUTO_HIDE transition """
        # Don't act on each and every focus message. Delay the start
        # of the transition slightly so that only the last of a bunch of
        # focus messages is acted on.
        delay = self.SHOW_REACTION_TIME if show else \
                self.HIDE_REACTION_TIME
        self._auto_show_timer.start(delay, self._begin_transition, show)

    def _begin_transition(self, show):
        self._keyboard_widget.transition_visible_to(show)
        self._keyboard_widget.commit_transition()
        return False

    def get_repositioned_window_rect(self, home, limit_rects,
                                     test_clearance, move_clearance,
                                     horizontal = True, vertical = True):
        """
        Get the alternative window rect suggested by auto-show or None if
        no repositioning is required.
        """
        accessible = self._active_accessible
        if accessible:
            rect = self._state_tracker.get_accessible_extents(accessible)
            if not rect.is_empty() and \
               not self._lock_visible:
                return self._get_window_rect_for_accessible_rect( \
                                            home, rect, limit_rects,
                                            test_clearance, move_clearance,
                                            horizontal, vertical)
        return None

    def _get_window_rect_for_accessible_rect(self, home, rect, limit_rects,
                                             test_clearance, move_clearance,
                                             horizontal = True, vertical = True):
        """
        Find new window position based on the screen rect of the accessible.
        """
        mode = "nooverlap"
        x = y = None

        if mode == "closest":
            x, y = rect.left(), rect.bottom()
        if mode == "nooverlap":
            x, y = self._find_non_occluding_position(home, rect, limit_rects,
                                                 test_clearance, move_clearance,
                                                 horizontal, vertical)
        if not x is None:
            return Rect(x, y, home.w, home.h)
        else:
            return None

    def _find_non_occluding_position(self, home, acc_rect, limit_rects,
                                     test_clearance, move_clearance,
                                     horizontal = True, vertical = True):

        # The home_rect doesn't include window decoration,
        # make sure to add decoration for correct clearance.
        rh = home.copy()
        window = self._keyboard_widget.get_kbd_window()
        if window:
            offset = window.get_client_offset()
            rh.w += offset[0]
            rh.h += offset[1]

        # Leave some clearance around the accessible to account for
        # window frames and position errors of firefox entries.
        ra = acc_rect.apply_border(*test_clearance)

        if rh.intersects(ra):

            # Leave a different clearance for the new to be found positions.
            ra = acc_rect.apply_border(*move_clearance)
            x, y = rh.get_position()

            # candidate positions
            vp = []
            if horizontal:
                vp.append([ra.left() - rh.w, y])
                vp.append([ra.right(), y])
            if vertical:
                vp.append([x, ra.top() - rh.h])
                vp.append([x, ra.bottom()])

            # limited, non-intersecting candidate rectangles
            vr = []
            for p in vp:
                pl = self._keyboard_widget.limit_position( p[0], p[1],
                                                  self._keyboard_widget.canvas_rect,
                                                  limit_rects)
                r = Rect(pl[0], pl[1], rh.w, rh.h)
                if not r.intersects(ra):
                    vr.append(r)

            # candidate with smallest center-to-center distance wins
            chx, chy = rh.get_center()
            dmin = None
            rmin = None
            for r in vr:
                cx, cy = r.get_center()
                dx, dy = cx - chx, cy - chy
                d2 = dx * dx + dy * dy
                if dmin is None or dmin > d2:
                    dmin = d2
                    rmin = r

            if not rmin is None:
                return rmin.get_position()

        return None, None

