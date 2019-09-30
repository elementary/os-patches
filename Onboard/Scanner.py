# -*- coding: utf-8 -*-

# Copyright © 2012, Gerd Kohlberger
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

import sys
import Onboard.osk as osk
import logging
from functools import cmp_to_key

from Onboard.Config    import Config
from Onboard.KeyCommon import KeyCommon
from Onboard.XInput    import XIDeviceManager, XIEventType, XIEventMask
from Onboard.utils     import Timer, show_new_device_dialog

logger = logging.getLogger(__name__)
config = Config()

"""
Methods and terminology from:
 - Colven, Judge, 2006: Switch access to technology. A comprehensive guide.
 - GOK: The GNOME On-screen Keyboard.
"""

class Chunker(object):
    """
    Abstract base class for all chunker objects.

    Organizes keys into groups and provides methods
    to travers and highlight them.

    Hierarchy:
      Chunker --> FlatChunker --> GroupChunker
                              --> GridChunker
    """

    def __init__(self):
        logger.debug("Chunker.__init__()")

        """ Hierarchy of keys (list). """
        self._chunks = None

        """ The index of the active chunk. """
        self._index = 0

        """ The number of chunks at the current level. """
        self._length = 0

        """ A stack of (index, len) tuples. """
        self._path = []

        """ Number of times the current level has been scanned. """
        self.cycles = 0

    def __del__(self):
        logger.debug("Chunker.__del__()")

    def chunk(self, layout, layer):
        """
        Abstract: Split the keys on a layer into chunks.
        """
        raise NotImplementedError()

    def get_current_object(self):
        """
        Get the list or key the chunker points to.
        """
        level = self._chunks

        for p in self._path:
            index = p[0]
            level = level[index]

        return level[self._index]

    def _highlight_rec(self, obj, hl, keys):
        """
        Recursively sets the highlight on all keys below obj.
        """
        if isinstance(obj, list):
            for o in obj:
                self._highlight_rec(o, hl, keys)
        else:
            if hl != obj.scanned:
                obj.scanned = hl
                keys.append(obj)

    def highlight(self, hl, root=None):
        """
        Highlight or clear the current chunk.
        """
        keys = []

        if not root:
            root = self.get_current_object()

        self._highlight_rec(root, hl, keys)

        return keys

    def highlight_all(self, hl):
        """
        Highlight or clear all chunks.
        """
        return self.highlight(hl, self._chunks)

    def next(self):
        """
        Move to the next chunk on the current level.
        """
        next = (self._index + 1) % self._length

        if next < self._index:
            self.cycles += 1

        self._index = next

    def previous(self):
        """
        Move to the previous chunk on the current level.
        """
        prev = (self._index - 1) % self._length

        if prev > self._index:
            self.cycles += 1

        self._index = prev

    def can_ascend(self):
        """
        Whether the chunker can move a level up in the hierarchy.
        """
        return len(self._path) != 0

    def ascend(self):
        """
        Move one level up in the hierarchy.
        """
        if self.can_ascend():
            self._index, self._length = self._path.pop()
            self.cycles = 0
            return True

        return False

    def can_descend(self):
        """
        Whether the chunker can move a level down in the hierarchy.
        """
        return isinstance(self.get_current_object(), list)

    def descend(self):
        """
        Move one level down in the hierarchy.
        - Skips levels that have only one element.
        """
        obj = self.get_current_object()

        while isinstance(obj, list):
            self._path.append((self._index, self._length))
            self._index = 0
            self._length = len(obj)
            self.cycles = 0

            if self._length == 1:
                obj = obj[0]
                continue
            return True

        return False

    def up(self):
        """
        Abstract: Move to key above the current.
        """
        raise NotImplementedError()

    def down(self):
        """
        Abstract: Move to key below the current.
        """
        raise NotImplementedError()

    def get_key(self):
        """
        Get the current key.
        Returns None if the object is a list.
        """
        obj = self.get_current_object()

        if not isinstance(obj, list):
            return obj

        return None

    def reset(self):
        """
        Set the chunker to its initial state.
        """
        self.cycles  = 0
        self._index  = 0
        self._length = len(self._chunks)
        self._path   = []

    def is_reset(self):
        """
        Is the chunker in its initial state.
        """
        return not self._index and \
               not self.cycles and \
               not len(self._path)


class FlatChunker(Chunker):
    """
    Chunks a layer based on key location.
    """
    def compare_keys(self, a, b):
        """
        Sort keys by y and then x position
        """
        rect_a = a.get_border_rect().int()
        rect_b = b.get_border_rect().int()

        y = rect_a.y - rect_b.y
        if y != 0:
            return y

        return rect_a.x - rect_b.x

    def chunk(self, layout, layer):
        """
        Create a list of scannable keys and sort it.
        """
        self._chunks = [k for k in layout.iter_layer_keys(layer) \
                        if k.is_path_scannable()]
        self._chunks.extend([k for k in layout.iter_layer_keys(None) \
                             if k.is_path_scannable()])
        self._chunks.sort(key=cmp_to_key(self.compare_keys))
        self._length = len(self._chunks)


class GroupChunker(FlatChunker):
    """
    Chunks a layer based on priority and key location.
    """
    def compare_keys(self, a, b):
        """
        Sort keys by priority and location.
        """
        p = a.get_path_scan_priority() - b.get_path_scan_priority()
        if p != 0:
            return p

        return super(GroupChunker, self).compare_keys(a, b)

    def chunk(self, layout, layer):
        """
        Create a nested list of keys.
        """
        last_priority = None
        last_y = None
        chunks = []

        # populates 'self._chunks' with a flat sorted list of keys
        # using the compare_keys method of this class
        super(GroupChunker, self).chunk(layout, layer)

        # creates a new nested chunk list with the following layout:
        # A list of 'priority groups' where each members is a
        # list of 'scan rows' in which each member is a key.
        for key in self._chunks:
            scan_priority = key.get_path_scan_priority()
            if scan_priority != last_priority:
                last_priority = scan_priority
                last_y = None
                group = []
                chunks.append(group)

            rect = key.get_border_rect().int()
            if rect.y != last_y:
                last_y = rect.y
                row = []
                group.append(row)

            row.append(key)

        # if all keys are in the same group, remove the group
        if len(chunks) == 1:
            chunks = chunks[0]

        self._chunks = chunks
        self._length = len(self._chunks)


class GridChunker(FlatChunker):
    """
    Chunks a layer into rows of keys.
    """
    def chunk(self, layout, layer):
        """
        Create a nested list of keys.
        """
        last_x = sys.maxsize
        chunks = []

        # populates 'self._chunks' with a flat sorted list of keys
        super(GridChunker, self).chunk(layout, layer)

        for key in self._chunks:
            rect = key.get_border_rect().int()
            if rect.x < last_x:
                row = []
                chunks.append(row)
            last_x = rect.x
            row.append(key)

        self._chunks = chunks
        self._length = len(self._chunks)

    def _select_neighbour(self, key, direction):
        if key is None:
            return

        kc = key.get_border_rect().get_center()
        min_x = sys.float_info.max

        self.ascend()
        direction()

        for idx, obj in enumerate(self.get_current_object()):
            oc = obj.get_border_rect().get_center()
            dx = abs(kc[0] - oc[0])
            if dx < min_x:
                min_x = dx
                neighbour = idx

        self.descend()
        self._index = neighbour

    def up(self):
        self._select_neighbour(self.get_key(), self.previous)

    def down(self):
        self._select_neighbour(self.get_key(), self.next)


class ScanMode(Timer):
    """
    Abstract base class for all scanning modes.

    Specifies how the scanner moves between chunks of keys
    and when to activate them. Scan mode subclasses define
    a set of actions they support and the base class translates
    input device events into scan actions.

    Hierarchy:
        ScanMode --> AutoScan --> UserScan
                              --> OverScan
                 --> StepScan
                 --> DirectScan
    """

    """ Scan actions """
    ACTION_STEP       = 0
    ACTION_LEFT       = 1
    ACTION_RIGHT      = 2
    ACTION_UP         = 3
    ACTION_DOWN       = 4
    ACTION_ACTIVATE   = 5
    ACTION_STEP_START = 6
    ACTION_STEP_STOP  = 7
    ACTION_UNHANDLED  = 8

    """ Time between key activation flashes (in sec) """
    ACTIVATION_FLASH_INTERVAL = 0.1

    """ Number of key activation flashes """
    ACTIVATION_FLASH_COUNT = 4

    def __init__(self, redraw_callback, activate_callback):
        super(ScanMode, self).__init__()

        logger.debug("ScanMode.__init__()")

        """ Activation timer instance """
        self._activation_timer = Timer()

        """ Counter for key flash animation """
        self._flash = 0

        """ Callback for key redraws """
        self._redraw_callback = redraw_callback

        """ Callback for key activation """
        self._activate_callback = activate_callback

        """ A Chunker instance """
        self.chunker = None

    def __del__(self):
        logger.debug("ScanMode.__del__()")

    def map_actions(self, detail, pressed):
        """
        Abstract: Convert input events into scan actions.
        """
        raise NotImplementedError()

    def do_action(self, action):
        """
        Abstract: Handle scan actions.
        """
        raise NotImplementedError()

    def scan(self):
        """
        Abstract: Move between chunks.
        """
        raise NotImplementedError()

    def create_chunker(self):
        """
        Abstract: Create a chunker instance.
        """
        raise NotImplementedError()

    def init_position(self):
        """
        Virtual: Called if a new layer was set or a key activated.
        """
        pass

    def handle_event(self, event):
        """
        Translate device events into scan actions.
        """
        # Ignore events during key activation
        if self._activation_timer.is_running():
            return

        event_type = event.xi_type
        if event_type == XIEventType.ButtonPress:
            button_map = config.scanner.device_button_map
            action = self.map_actions(button_map, event.button, True)

        elif event_type == XIEventType.ButtonRelease:
            button_map = config.scanner.device_button_map
            action = self.map_actions(button_map, event.button, False)

        elif event_type == XIEventType.KeyPress:
            key_map = config.scanner.device_key_map
            action = self.map_actions(key_map, event.keyval, True)

        elif event_type == XIEventType.KeyRelease:
            key_map = config.scanner.device_key_map
            action = self.map_actions(key_map, event.keyval, False)

        else:
            action = self.ACTION_UNHANDLED

        if action != self.ACTION_UNHANDLED:
            self.do_action(action)

    def on_timer(self):
        """
        Override: Timer() callback.
        """
        return self.scan()

    def max_cycles_reached(self):
        """
        Check if the maximum number of scan cycles is reached.
        """
        return self.chunker.cycles >= config.scanner.cycles

    def set_layer(self, layout, layer):
        """
        Set the layer that should be scanned.
        """
        self.reset()
        self.chunker = self.create_chunker()
        self.chunker.chunk(layout, layer)
        self.init_position()

    def _on_activation_timer(self, key):
        """
        Timer callback: Flashes the key and finally activates it.
        """
        if self._flash > 0:
            key.scanned = not key.scanned
            self._flash -= 1
            self.redraw([key])
            return True
        else:
            self._activate_callback(key)
            self.init_position()
            return False

    def activate(self):
        """
        Activates a key and triggers feedback.
        """
        key = self.chunker.get_key()
        if not key:
            return

        if config.scanner.feedback_flash:
            self._flash = self.ACTIVATION_FLASH_COUNT
            self._activation_timer.start(self.ACTIVATION_FLASH_INTERVAL,
                                         self._on_activation_timer,
                                         key)
        else:
            self._activate_callback(key)
            self.init_position()

    def reset(self):
        """
        Stop scanning and clear all highlights.
        """
        if self.is_running():
            self.stop()

        if self.chunker:
            self.redraw(self.chunker.highlight_all(False))

    def redraw(self, keys=None):
        """
        Update individual keys or the entire keyboard.
        """
        self._redraw_callback(keys)

    def finalize(self):
        """
        Clean up the ScanMode instance.
        """
        self.reset()
        self._activation_timer = None


class AutoScan(ScanMode):
    """
    Automatic scan mode for 1 switch. Starts scanning on
    switch press and moves through a hierarchy of chunks.
    """
    def create_chunker(self):
        return GroupChunker()

    def map_actions(self, dev_map, detail, is_press):
        if is_press and detail in dev_map:
            return self.ACTION_STEP

        return self.ACTION_UNHANDLED

    def scan(self):
        self.redraw(self.chunker.highlight(False))
        self.chunker.next()

        if self.max_cycles_reached():
            self.chunker.reset()
            return False
        else:
            self.redraw(self.chunker.highlight(True))
            return True

    def do_action(self, action):
        if not self.is_running():
            # Start scanning
            self.redraw(self.chunker.highlight(True))
            self.start(config.scanner.interval)
        else:
            # Subsequent clicks
            self.stop()
            self.redraw(self.chunker.highlight(False))

            if self.chunker.descend():
                # Move one level down
                self.redraw(self.chunker.highlight(True))
                self.start(config.scanner.interval)
            else:
                # Activate
                self.activate()
                self.chunker.reset()


class UserScan(AutoScan):
    """
    Automatic scan mode for 1 switch. Like AutoScan but
    the scanner progresses only during switch press.
    """
    def map_actions(self, dev_map, detail, is_press):
        if detail in dev_map:
            if is_press:
                return self.ACTION_STEP_START
            else:
                return self.ACTION_STEP_STOP

        return self.ACTION_UNHANDLED

    def do_action(self, action):
        if action == self.ACTION_STEP_START:
            if not self.chunker.is_reset():
                # Every press except the initial
                self.redraw(self.chunker.highlight(False))
                self.chunker.descend()

            self.redraw(self.chunker.highlight(True))
            self.start(config.scanner.interval)

        elif action == self.ACTION_STEP_STOP:
            # Every release
            self.stop()
            if not self.chunker.can_descend():
                # Activate
                self.redraw(self.chunker.highlight(False))
                self.activate()
                self.chunker.reset()


class OverScan(AutoScan):
    """
    Automatic scan mode for 1 switch. Does fast forward
    scanning in a flat hierarchy with slow backtracking.
    """
    def __init__(self, redraw_callback, activate_callback):
        super(OverScan, self).__init__(redraw_callback, activate_callback)

        self._step = -1
        self._fast = True

    def create_chunker(self):
        return FlatChunker()

    def scan(self):
        self.redraw(self.chunker.highlight(False))
        if self._step > 0:
            # Backtrack
            self.chunker.previous()
            self._step -= 1
            self.redraw(self.chunker.highlight(True))
        else:
            # Fast forward
            self.chunker.next()

            if self.max_cycles_reached():
                # Abort
                self.chunker.reset()
                return False

            self.redraw(self.chunker.highlight(True))

            if not self._fast:
                self.stop()
                self.do_action(None)

        return True

    def do_action(self, action):
        if not self.is_running():
            # Start
            self._fast = True
            self._step = -1
            self.redraw(self.chunker.highlight(True))
            self.start(config.scanner.interval_fast)
        else:
            # Subsequent clicks
            if self._step >= 0:
                # Activate
                self.stop()
                self.redraw(self.chunker.highlight(False))
                self.activate()
                self.chunker.reset()
            else:
                # Backtrack
                self._step = config.scanner.backtrack
                self._fast = False
                self.chunker.cycles = 0
                self.start(config.scanner.interval)


class StepScan(ScanMode):
    """
    Directed scan mode for 2 switches.
    """
    def __init__(self, redraw_callback, activate_callback):
        super(StepScan, self).__init__(redraw_callback, activate_callback)

        self.swapped = False

    def create_chunker(self):
        return GroupChunker()

    def init_position(self):
        self.chunker.reset()
        self.redraw(self.chunker.highlight(True))

    def map_actions(self, dev_map, detail, is_press):
        if is_press and detail in dev_map:
            return dev_map[detail]

        return self.ACTION_UNHANDLED

    def get_alternate(self, action):
        if config.scanner.alternate and self.swapped:
            if action == self.ACTION_STEP:
                return self.ACTION_ACTIVATE
            else:
                return self.ACTION_STEP

        return action

    def do_action(self, action):
        if action == self.get_alternate(self.ACTION_STEP):
            self.redraw(self.chunker.highlight(False))
            self.chunker.next()
            if self.max_cycles_reached():
                self.init_position()
            else:
                self.redraw(self.chunker.highlight(True))
        else:
            self.redraw(self.chunker.highlight(False))
            self.swapped = not self.swapped
            if self.chunker.descend():
                self.redraw(self.chunker.highlight(True))
            else:
                self.activate()


class DirectScan(ScanMode):
    """
    Directed scan mode for 3 or 5 switches.
    """
    def create_chunker(self):
        return GridChunker()

    def init_position(self):
        self.chunker.descend()
        self.redraw(self.chunker.highlight(True))

    def map_actions(self, dev_map, detail, is_press):
        if is_press and detail in dev_map:
            return dev_map[detail]

        return self.ACTION_UNHANDLED

    def do_action(self, action):
        keys = self.chunker.highlight(False)

        if action == self.ACTION_LEFT:
            self.chunker.previous()
        elif action == self.ACTION_RIGHT:
            self.chunker.next()
        elif action == self.ACTION_UP:
            self.chunker.up()
        elif action == self.ACTION_DOWN:
            self.chunker.down()
        else:
            self.activate()

        keys.extend(self.chunker.highlight(True))
        self.redraw(keys)


class Scanner(object):
    """
    Main controller class for keyboard scanning. Manages
    ScanMode and ScanDevices objects and provides the
    public interface for the scanner.
    """

    """ Scan modes """
    MODE_AUTOSCAN  = 0
    MODE_OVERSCAN  = 1
    MODE_STEPSCAN  = 2
    MODE_DIRECTED3 = 3
    MODE_DIRECTED5 = 4

    def __init__(self, redraw_callback, activate_callback):
        logger.debug("Scanner.__init__()")

        """ A scan mode instance """
        self.mode = self._get_scan_mode(config.scanner.mode,
                                        redraw_callback,
                                        activate_callback)

        """ A scan device instance """
        self.device = ScanDevice(self.mode.handle_event)

        """ A keyboard layout """
        self.layout = None

        """ The active layer of the layout """
        self.layer = None

        config.scanner.mode_notify_add(self._mode_notify)
        config.scanner.user_scan_notify_add(self._user_scan_notify)

    def __del__(self):
        logger.debug("Scanner.__del__()")

    def _mode_notify(self, mode):
        """
        Callback for scanner.mode configuration changes.
        """
        rcb = self.mode._redraw_callback
        acb = self.mode._activate_callback

        self.mode.finalize()
        self.mode = self._get_scan_mode(mode, rcb, acb)
        self.mode.set_layer(self.layout, self.layer)

        self.device._event_handler = self.mode.handle_event

    def _user_scan_notify(self, user_scan):
        """
        Callback for scanner.user_scan configuration changes.
        """
        if config.scanner.mode == self.MODE_AUTOSCAN:
            self._mode_notify(config.scanner.mode)

    def _get_scan_mode(self, mode, redraw_callback, activate_callback):
        """
        Get the ScanMode instance for the current profile.
        """
        profiles = [ AutoScan, OverScan, StepScan, DirectScan ]

        if mode == self.MODE_AUTOSCAN and config.scanner.user_scan:
            return UserScan(redraw_callback, activate_callback)

        return profiles[mode](redraw_callback, activate_callback)

    def update_layer(self, layout, layer, force_update = False):
        """
        Notify the scanner about layer or layout changes.
        """
        changed = False

        if self.layout != layout:
            self.layout = layout
            changed = True

        if self.layer != layer:
            self.layer = layer
            changed = True

        if changed or force_update:
            self.mode.set_layer(self.layout, self.layer)

    def finalize(self):
        """
        Clean up all objects related to scanning.
        """
        config.scanner.mode_notify_remove(self._mode_notify)
        config.scanner.user_scan_notify_remove(self._user_scan_notify)
        self.device.finalize()
        self.mode.finalize()


class ScanDevice(object):
    """
    Input device manager class.

    Manages input devices on the system and deals with
    PnP related event. The actual press/release events
    are forwarded to a ScanMode instance.
    """

    """ Default device name (virtual core pointer) """
    DEFAULT_NAME = "Default"

    """ Device id's of the primary masters """
    DEFAULT_VCP_ID = 2
    DEFAULT_VCK_ID = 3

    """ Device name blacklist """
    blacklist = ["Virtual core pointer",
                 "Virtual core keyboard",
                 "Virtual core XTEST pointer",
                 "Virtual core XTEST keyboard",
                 "Power Button"]

    def __init__(self, event_handler):
        logger.debug("ScanDevice.__init__()")

        """ Selected device tuple (device id, master id) """
        self._active_device_ids = None

        """ Whether the active device is detached """
        self._floating = False

        """ Event handler for device events """
        self._event_handler = event_handler

        """ The manager for osk XInput devices """
        self._device_manager = XIDeviceManager()  # singleton
        self._device_manager.connect("device-event", self._device_event_handler)

        config.scanner.device_name_notify_add(self._device_name_notify)
        config.scanner.device_detach_notify_add(self._device_detach_notify)

        self._device_name_notify(config.scanner.device_name)

    def __del__(self):
        logger.debug("ScanDevice.__del__()")

    def _device_event_handler(self, event):
        """
        Handler for XI2 events.
        """
        event_type = event.xi_type
        device_id  = event.device_id

        if event_type == XIEventType.DeviceAdded:
            device = self._device_manager.lookup_device_id(device_id)
            show_new_device_dialog(device.name,
                                   device.get_config_string(),
                                   device.is_pointer(),
                                   self._on_new_device_accepted)

        elif event_type == XIEventType.DeviceRemoved:
            # If we are currently using this device,
            # close it and fall back to 'Default'
            if self._active_device_ids and \
               self._active_device_ids[0] == device_id:
                self._active_device_ids = None
                self._floating = False
                config.scanner.device_detach = False
                config.scanner.device_name = self.DEFAULT_NAME

        else:
            # Never handle VCK events.
            if device_id != self.DEFAULT_VCK_ID:
                # Forward VCP events only if 'Default' is selected.
                # Else only handle devices we selected.
                if (device_id == self.DEFAULT_VCP_ID and \
                    config.scanner.device_name == self.DEFAULT_NAME) or \
                   (self._active_device_ids and \
                    device_id == self._active_device_ids[0]):

                    self._event_handler(event)

    def _on_new_device_accepted(self, config_string):
        """
        Callback for the 'New device' dialog.
        Called only if 'Use device' was chosen.
        """
        config.scanner.device_name = config_string
        config.scanner.device_detach = True

    def _device_detach_notify(self, detach):
        """
        Callback for the scanner.device_detach configuration changes.
        """
        if self._active_device_ids is None:
            return

        if detach:
            if not self._floating:
                self.detach(self._active_device_ids[0])
        else:
            if self._floating:
                self.attach(*self._active_device_ids)

    def _device_name_notify(self, name):
        """
        Callback for the scanner.device_name configuration changes.
        """
        self.close()

        if name == self.DEFAULT_NAME:
            return

        for device in self._device_manager.get_devices():
            if self.is_useable(device) and \
               name == device.get_config_string():
                self.open(device)
                break

        if self._active_device_ids is None:
            logger.debug("Unknown device-name in configuration.")
            config.scanner.device_detach = False
            config.scanner.device_name = self.DEFAULT_NAME

    def open(self, device):
        """
        Select for events and optionally detach the device.
        """
        if device.is_pointer():
            event_mask = XIEventMask.ButtonPressMask | \
                         XIEventMask.ButtonReleaseMask
        else:
            event_mask = XIEventMask.KeyPressMask | \
                         XIEventMask.KeyReleaseMask
        try:
            self._device_manager.select_events(None, device, event_mask)
            self._active_device_ids = (device.id, device.master)
        except Exception as ex:
            logger.warning("Failed to open device {id}: {ex}"
                           .format(id = device.id, ex = ex))

        if config.scanner.device_detach and not device.is_master():
            self.detach(device.id)

    def close(self):
        """
        Stop using the current device.
        """
        if self._floating:
            self.attach(*self._active_device_ids)

        if self._active_device_ids:
            device = self._device_manager.lookup_device_id( \
                                            self._active_device_ids[0])
            try:
                self._device_manager.unselect_events(None, device)
                self._active_device_ids = None
            except Exception as ex:
                logger.warning("Failed to close device {id}: {ex}"
                               .format(id = self._active_device_ids[0],
                                       ex = ex))

    def attach(self, dev_id, master):
        """
        Attach the device to a master.
        """
        try:
            self._device_manager.attach_device_id(dev_id, master)
            self._floating = False
        except:
            logger.warning("Failed to attach device {id} to {master}"
                           .format(id = dev_id, master = master))

    def detach(self, dev_id):
        """
        Detach the device from its master.
        """
        try:
            self._device_manager.detach_device_id(dev_id)
            self._floating = True
        except:
            logger.warning("Failed to detach device {id}".format(id = dev_id))

    def finalize(self):
        """
        Clean up the ScanDevice instance.
        """
        self._device_manager.disconnect("device-event",
                                        self._device_event_handler)
        config.scanner.device_name_notify_remove(self._device_name_notify)
        config.scanner.device_detach_notify_remove(self._device_detach_notify)
        self.close()
        self._event_handler = None
        self.devices = None

    @staticmethod
    def is_useable(device):
        """
        Check whether this device is useable for scanning.
        """
        return device.name not in ScanDevice.blacklist \
               and device.enabled \
               and not device.is_floating()


