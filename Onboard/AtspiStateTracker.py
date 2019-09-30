# -*- coding: utf-8 -*-
""" GTK specific keyboard class """

from __future__ import division, print_function, unicode_literals

import logging
_logger = logging.getLogger(__name__)

try:
    from gi.repository import Atspi
except ImportError as e:
    _logger.warning("Atspi typelib missing, auto-show unavailable")

from Onboard.utils        import Rect, EventSource, Process, unicode_str


class AsyncEvent:
    """
    Decouple AT-SPI events from D-Bus calls to reduce the risk for deadlocks.
    """
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)
        self._kwargs = kwargs

    def __repr__(self):
        return type(self).__name__ + "(" + \
           ", ".join(str(key) + "=" + repr(val) \
                     for key, val in self._kwargs.items()) \
           + ")"


class AtspiStateTracker(EventSource):
    """
    Keeps track of the currently active accessible by listening
    to AT-SPI focus events.
    """

    _focus_event_names      = ("text-entry-activated",)
    _text_event_names       = ("text-changed", "text-caret-moved")
    _key_stroke_event_names = ("key-pressed",)
    _async_event_names      = ("async-focus-changed",
                               "async-text-changed",
                               "async-text-caret-moved")
    _event_names = _async_event_names + \
                   _focus_event_names + \
                   _text_event_names + \
                   _key_stroke_event_names

    _focus_listeners_registered = False
    _keystroke_listeners_registered = False
    _text_listeners_registered = False

    _keystroke_listener = None

    # asynchronously accessible members
    _focused_accessible = None   # any currently focused accessible
    _active_accessible = None    # editable focused accessible
    _last_active_accessible = None
    _state = None                # cache of various accessible properties

    def __new__(cls, *args, **kwargs):
        """
        Singleton magic.
        """
        if not hasattr(cls, "self"):
            cls.self = object.__new__(cls, *args, **kwargs)
            cls.self.construct()
        return cls.self

    def __init__(self):
        """
        Called multiple times, don't use this.
        """
        pass

    def construct(self):
        """
        Singleton constructor, runs only once.
        """
        EventSource.__init__(self, self._event_names)

        self._state = {}
        self._frozen = False

    def cleanup(self):
        EventSource.cleanup(self)
        self._register_atspi_listeners(False)

    def connect(self, event_name, callback):
        EventSource.connect(self, event_name, callback)
        self._update_listeners()

    def disconnect(self, event_name, callback):
        EventSource.disconnect(self, event_name, callback)
        self._update_listeners()

    def _update_listeners(self):
        register = self.has_listeners(self._focus_event_names)
        self._register_atspi_focus_listeners(register)

        register = self.has_listeners(self._text_event_names)
        self._register_atspi_text_listeners(register)

        register = self.has_listeners(self._key_stroke_event_names)
        self._register_atspi_keystroke_listeners(register)

    def _register_atspi_listeners(self, register):
        self._register_atspi_focus_listeners(register)
        self._register_atspi_text_listeners(register)
        self._register_atspi_keystroke_listeners(register)

    def _register_atspi_focus_listeners(self, register):
        if not "Atspi" in globals():
            return

        if self._focus_listeners_registered != register:

            if register:
                self.atspi_connect("_listener_focus",
                                   "focus",
                                   self._on_atspi_global_focus)
                self.atspi_connect("_listener_object_focus",
                                   "object:state-changed:focused",
                                   self._on_atspi_object_focus)

                # private asynchronous events
                for name in self._async_event_names:
                    handler = "_on_" + name.replace("-", "_")
                    EventSource.connect(self, name, getattr(self, handler))
            else:
                self.atspi_disconnect("_listener_focus",
                                      "focus")
                self.atspi_disconnect("_listener_object_focus",
                                      "object:state-changed:focused")

                for name in self._async_event_names:
                    handler = "_on_" + name.replace("-", "_")
                    EventSource.disconnect(self, name, getattr(self, handler))

            self._focus_listeners_registered = register

    def _register_atspi_text_listeners(self, register):
        if not "Atspi" in globals():
            return

        if self._text_listeners_registered != register:
            if register:
                self.atspi_connect("_listener_text_changed",
                                   "object:text-changed",
                                   self._on_atspi_text_changed)
                self.atspi_connect("_listener_text_caret_moved",
                                   "object:text-caret-moved",
                                   self._on_atspi_text_caret_moved)
            else:
                self.atspi_disconnect("_listener_text_changed",
                                      "object:text-changed")
                self.atspi_disconnect("_listener_text_caret_moved",
                                      "object:text-caret-moved")

        self._text_listeners_registered = register

    def _register_atspi_keystroke_listeners(self, register):
        if not "Atspi" in globals():
            return

        if self._keystroke_listeners_registered != register:
            modifier_masks = range(16)

            if register:
                if not self._keystroke_listener:
                    self._keystroke_listener = \
                       Atspi.DeviceListener.new(self._on_atspi_keystroke, None)

                for modifier_mask in modifier_masks:
                    Atspi.register_keystroke_listener( \
                                        self._keystroke_listener,
                                        None,        # key set, None=all
                                        modifier_mask,
                                        Atspi.KeyEventType.PRESSED,
                                        Atspi.KeyListenerSyncType.SYNCHRONOUS)
            else:
                # Apparently any single deregister call will turn off
                # all the other registered modifier_masks too. Since
                # deregistering takes extremely long (~2.5s for 16 calls)
                # seize the opportunity and just pick a single arbitrary
                # mask (Quantal).
                modifier_masks = [2]

                for modifier_mask in modifier_masks:
                    Atspi.deregister_keystroke_listener(
                                        self._keystroke_listener,
                                        None, # key set, None=all
                                        modifier_mask,
                                        Atspi.KeyEventType.PRESSED)

        self._keystroke_listeners_registered = register

    def atspi_connect(self, attribute, event, callback):
        """
        Start listening to an AT-SPI event.
        Creates a new event listener for each event, since this seems
        to be the only way to allow reliable deregistering of events.
        """
        if hasattr(self, attribute):
            listener = getattr(self, attribute)
        else:
            listener = None

        if listener is None:
            listener = Atspi.EventListener.new(callback, None)
            setattr(self, attribute, listener)
        listener.register(event)

    def atspi_disconnect(self, attribute, event):
        """
        Stop listening to AT-SPI event.
        """
        listener = getattr(self, attribute)
        listener.deregister(event)

    def freeze(self):
        """
        Freeze AT-SPI message processing, e.g. while displaying
        a dialog or popoup menu.
        """
        self._register_atspi_listeners(False)
        self._frozen = True

    def thaw(self):
        """
        Resume AT-SPI message processing.
        """
        self._update_listeners()
        self._frozen = False

    def emit_async(self, event_name, *args, **kwargs):
        if not self._frozen:
            EventSource.emit_async(self, event_name, *args, **kwargs)

    ########## synchronous handlers ##########

    def _on_atspi_global_focus(self, event, user_data):
        self._on_atspi_focus(event, True)

    def _on_atspi_object_focus(self, event, user_data):
        self._on_atspi_focus(event)

    def _on_atspi_focus(self, event, focus_received = False):
        focused = bool(focus_received) or bool(event.detail1) # received focus?
        ae = AsyncEvent(accessible = event.source,
                        focused = focused)
        self.emit_async("async-focus-changed", ae)

    def _on_atspi_text_changed(self, event, user_data):
        #print("_on_atspi_text_changed", event.detail1, event.detail2, event.source, event.type, event.type.endswith("delete"))
        ae = AsyncEvent(accessible = event.source,
                        type = event.type,
                        pos = event.detail1,
                        length = event.detail2)
        self.emit_async("async-text-changed", ae)
        return False

    def _on_atspi_text_caret_moved(self, event, user_data):
        #print("_on_atspi_text_caret_moved", event.detail1, event.detail2, event.source, event.type, event.source.get_name(), event.source.get_role())
        ae = AsyncEvent(accessible = event.source,
                        caret = event.detail1)
        self.emit_async("async-text-caret-moved", ae)
        return False

    def _on_atspi_keystroke(self, event, user_data):
        if event.type == Atspi.EventType.KEY_PRESSED_EVENT:
            _logger.debug("key-stroke {} {} {} {}" \
                          .format(event.modifiers,
                                  event.hw_code, event.id, event.is_text))
            #keysym = event.id # What is this? Not XK_ keysyms apparently.
            ae = AsyncEvent(hw_code   = event.hw_code,
                            modifiers = event.modifiers)
            self.emit_async("key-pressed", ae)

        return False # don't consume event

    ########## asynchronous handlers ##########

    def _on_async_focus_changed(self, event):
        accessible = event.accessible
        focused = event.focused
        self._state = {}

        # Don't access the accessible while frozen. This leads to deadlocks
        # while displaying Onboard's own dialogs/popup menu's.
        if not self._frozen:
            self._log_accessible(accessible, focused)

            if not accessible is None:
                try:
                    self._state = \
                            self._read_initial_accessible_state(accessible)
                except Exception as ex: # Private exception gi._glib.GError when
                                        # gedit became unresponsive.
                    _logger.info("_on_focus_changed(): "
                                 "invalid accessible, failed to read state: " \
                                 + unicode_str(ex))

                editable = self._is_accessible_editable(self._state)
                activate = focused and editable

                if focused:
                    self._focused_accessible = accessible
                elif not focused and self._focused_accessible == accessible:
                    self._focused_accessible = None
                else:
                    activate = None

                if not activate is None:
                    if activate:
                        active_accessible = self._focused_accessible
                    else:
                        active_accessible = None

                    self._set_active_accessible(active_accessible)

    def _set_active_accessible(self, accessible):
        self._active_accessible = accessible

        if not self._active_accessible is None or \
           not self._last_active_accessible is None:

            if not accessible is None:
                try:
                    self._state.update( \
                                self._read_remaining_accessible_state(accessible))
                except Exception as ex: # Private exception gi._glib.GError when
                                        # gedit became unresponsive.
                    _logger.info("_set_active_accessible(): "
                            "invalid accessible, failed to read remaining state: " \
                            + unicode_str(ex))

            # notify listeners
            self.emit("text-entry-activated",
                      self._active_accessible)

            self._last_active_accessible = self._active_accessible

    def _on_async_text_changed(self, event):
        if event.accessible is self._active_accessible:
            type = event.type
            insert = type.endswith(("insert", "insert:system"))
            delete = type.endswith(("delete", "delete:system"))
            #print(event.accessible.get_id(), type, insert)
            if insert or delete:
                event.insert = insert
                self.emit("text-changed", event)
            else:
                _logger.warning("_on_async_text_changed: "
                                "unknown event type '{}'" \
                                .format(event.type))

    def _on_async_text_caret_moved(self, event):
        if event.accessible is self._active_accessible:
            self.emit("text-caret-moved", event)

    def get_state(self):
        """ All available state of the active accessible """
        return self._state

    def get_role(self):
        """ Role of the active accessible """
        return self._state.get("role")

    def get_state_set(self):
        """ State set of the active accessible """
        return self._state.get("state-set")

    def is_single_line(self):
        """ Is active accessible a single line text entry? """
        state_set = self.get_state_set()
        return state_set and state_set.contains(Atspi.StateType.SINGLE_LINE)

    def get_extents(self):
        """ Screen rect of the active accessible """
        return self._state.get("extents", Rect())

    @staticmethod
    def get_accessible_extents(accessible):
        """ Screen rect of the given accessible, no caching """
        try:
            ext = accessible.get_extents(Atspi.CoordType.SCREEN)
        except Exception as ex: # private exception gi._glib.GError when
                # right clicking onboards unity2d launcher (Precise)
            _logger.info("Invalid accessible,"
                         " failed to get extents: " + unicode_str(ex))
            return Rect()

        return Rect(ext.x, ext.y, ext.width, ext.height)

    @staticmethod
    def get_accessible_text(accessible, begin, end):
        """ Text of the given accessible, no caching """
        try:
            text = Atspi.Text.get_text(accessible, begin, end)
        except Exception as ex: # private exception
                                # gi._glib.GError: timeout from dbind
                                # with web search in firefox.
            _logger.info("Invalid accessible,"
                         " failed to get text: " + unicode_str(ex))
            return None

        return text

    def _is_accessible_editable(self, acc_state):
        """ Is this an accessible onboard should be shown for? """
        role      = acc_state.get("role")
        state_set = acc_state.get("state-set")
        if not state_set is None:

            if role in [Atspi.Role.TEXT,
                        Atspi.Role.TERMINAL,
                        Atspi.Role.DATE_EDITOR,
                        Atspi.Role.PASSWORD_TEXT,
                        Atspi.Role.EDITBAR,
                        Atspi.Role.ENTRY,
                        Atspi.Role.DOCUMENT_TEXT,
                        Atspi.Role.DOCUMENT_FRAME,
                        Atspi.Role.DOCUMENT_EMAIL,
                        Atspi.Role.SPIN_BUTTON,
                        Atspi.Role.COMBO_BOX,
                        Atspi.Role.DATE_EDITOR,
                        Atspi.Role.PARAGRAPH,      # LibreOffice Writer
                        Atspi.Role.HEADER,
                        Atspi.Role.FOOTER,
                       ]:
                if role in [Atspi.Role.TERMINAL] or \
                   (not state_set is None and \
                    state_set.contains(Atspi.StateType.EDITABLE)):
                    return True
        return False

    def _read_initial_accessible_state(self, accessible):
        """
        Read just enough to find out if we are intereseted in this accessible.
        """
        state = {}
        state["role"] = accessible.get_role()
        state["state-set"] = accessible.get_state_set()
        return state

    def _read_remaining_accessible_state(self, accessible):
        """
        Read more attributes and find out as much as we
        can about the accessibles purpose.
        """
        state = {}

        state["attributes"] = accessible.get_attributes()
        state["interfaces"] = accessible.get_interfaces()

        ext = accessible.get_extents(Atspi.CoordType.SCREEN)
        state["extents"] = Rect(ext.x, ext.y, ext.width, ext.height)

        # These are currently used only in debug output
        if _logger.isEnabledFor(logging.DEBUG):
            state["id"] = accessible.get_id()
            state["name"] = accessible.get_name()
            pid = accessible.get_process_id()
            state["process-id"] = pid
            if pid != -1:
                state["process-name"] = Process.get_process_name(pid)

            app = accessible.get_application()
            if app:
                state["app-name"] = app.get_name()
                state["app-description"] = app.get_description()

        return state

    def _log_accessible(self, accessible, focused):
        if _logger.isEnabledFor(logging.DEBUG):
            msg = "AT-SPI focus event: focused={}, ".format(focused)
            if not accessible:
                msg += "accessible={}".format(accessible)
            else:
                name = "unknown"
                role = None
                role_name = None
                editable = None
                states = None
                extents = None

                try:
                    name = accessible.get_name()
                    role = accessible.get_role()
                    role_name = accessible.get_role_name()
                    state_set = accessible.get_state_set()
                    states = state_set.states
                    editable = state_set.contains(Atspi.StateType.EDITABLE) \
                               if state_set else None
                    ext = accessible.get_extents(Atspi.CoordType.SCREEN)
                    extents   = Rect(ext.x, ext.y, ext.width, ext.height)
                except: # private exception gi._glib.GError when gedit became unresponsive
                    pass

                msg += "name={name}, role={role}({role_name}), " \
                       "editable={editable}, states={states}, " \
                       "extents={extents}]" \
                        .format(name=name,
                                role = role,
                                role_name = role_name,
                                editable = editable,
                                states = states,
                                extents = extents \
                               )
            _logger.debug(msg)


class AtspiStateType:
    states = ['ACTIVE',
              'ANIMATED',
              'ARMED',
              'BUSY',
              'CHECKED',
              'COLLAPSED',
              'DEFUNCT',
              'EDITABLE',
              'ENABLED',
              'EXPANDABLE',
              'EXPANDED',
              'FOCUSABLE',
              'FOCUSED',
              'HAS_TOOLTIP',
              'HORIZONTAL',
              'ICONIFIED',
              'INDETERMINATE',
              'INVALID',
              'INVALID_ENTRY',
              'IS_DEFAULT',
              'LAST_DEFINED',
              'MANAGES_DESCENDANTS',
              'MODAL',
              'MULTISELECTABLE',
              'MULTI_LINE',
              'OPAQUE',
              'PRESSED',
              'REQUIRED',
              'RESIZABLE',
              'SELECTABLE',
              'SELECTABLE_TEXT',
              'SELECTED',
              'SENSITIVE',
              'SHOWING',
              'SINGLE_LINE',
              'STALE',
              'SUPPORTS_AUTOCOMPLETION',
              'TRANSIENT',
              'TRUNCATED',
              'VERTICAL',
              'VISIBLE',
              'VISITED',
             ]

    @staticmethod
    def to_strings(state_set):
        result = []
        for s in AtspiStateType.states:
            if state_set.contains(getattr(Atspi.StateType, s)):
                result.append(s)
        return result


