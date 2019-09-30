# -*- coding: UTF-8 -*-
"""
KeyCommon hosts the abstract classes for the various types of Keys.
UI-specific keys should be defined in KeyGtk or KeyKDE files.
"""

from __future__ import division, print_function, unicode_literals

from math import pi
import re

from Onboard.utils import Rect, LABEL_MODIFIERS, Modifiers, \
                          polygon_to_rounded_path

from Onboard.Layout import LayoutItem

### Logging ###
import logging
_logger = logging.getLogger("KeyCommon")
###############

### Config Singleton ###
from Onboard.Config import Config
config = Config()
########################

(
    CHAR_TYPE,
    KEYSYM_TYPE,
    KEYCODE_TYPE,
    MACRO_TYPE,
    SCRIPT_TYPE,
    KEYPRESS_NAME_TYPE,
    BUTTON_TYPE,
    LEGACY_MODIFIER_TYPE,
    WORD_TYPE,
    CORRECTION_TYPE,
) = tuple(range(1, 11))

(
    SINGLE_STROKE_ACTION,  # press on button down, release on up (default)
    DELAYED_STROKE_ACTION, # press+release on button up (MENU)
    DOUBLE_STROKE_ACTION,  # press+release on button down and up, (CAPS, NMLK)
) = tuple(range(3))

actions = {
           "single-stroke"  : SINGLE_STROKE_ACTION,
           "delayed-stroke" : DELAYED_STROKE_ACTION,
           "double-stroke"  : DOUBLE_STROKE_ACTION,
          }

class StickyBehavior:
    """ enum for sticky key behaviors """
    (
        CYCLE,
        DOUBLE_CLICK,
        LATCH_ONLY,
        LOCK_ONLY,
    ) = tuple(range(4))

    values = {"cycle"    : CYCLE,
              "dblclick" : DOUBLE_CLICK,
              "latch"    : LATCH_ONLY,
              "lock"     : LOCK_ONLY,
             }

    @staticmethod
    def from_string(str_value):
        """ Raises KeyError """
        return StickyBehavior.values[str_value]

    @staticmethod
    def is_valid(value):
        return value in StickyBehavior.values.values()


class LOD:
    """ enum for level of detail """
    (
        MINIMAL,    # clearly visible reduced detail, fastest
        REDUCED,    # slightly reduced detail
        FULL,       # full detail
    ) = tuple(range(3))

class ImageSlot:
    NORMAL = 0
    ACTIVE = 1

class KeyCommon(LayoutItem):
    """
    library-independent key class. Specific rendering options
    are stored elsewhere.
    """

    # extended id for key specific theme tweaks
    # e.g. theme_id=DELE.numpad (with id=DELE)
    theme_id = None

    # extended id for layout specific tweaks
    # e.g. "hide.wordlist", for hide button in wordlist mode
    svg_id = None

    # optional id of a sublayout used as long-press popup
    popup_id = None

    # Type of action to do when key is pressed.
    action = None

    # Type of key stroke to send
    type = None

    # Data used in sending key strokes.
    code = None

    # Keys that stay stuck when pressed like modifiers.
    sticky = False

    # Behavior if sticky is enabled, see StickyBehavior.
    sticky_behavior = None

    # modifier bit
    modifier = None

    # True when key is being hovered over (not implemented yet)
    prelight = False

    # True when key is being pressed.
    pressed = False

    # True when key stays 'on'
    active = False

    # True when key is sticky and pressed twice.
    locked = False

    # True when Onboard is in scanning mode and key is highlighted
    scanned = False

    # True when action was triggered e.g. key-strokes were sent on press
    activated = False

    # Size to draw the label text in Pango units
    font_size = 1

    # Labels which are displayed by this key
    labels = None  # {modifier_mask : label, ...}

    # label that is currently displayed by this key
    label = ""

    # smaller label of a currently invisible modifier level
    secondary_label = ""

    # Images displayed by this key (optional)
    image_filenames = None

    # horizontal label alignment
    label_x_align = config.DEFAULT_LABEL_X_ALIGN

    # vertical label alignment
    label_y_align = config.DEFAULT_LABEL_Y_ALIGN

    # label margin (x, y)
    label_margin = config.LABEL_MARGIN

    # tooltip text
    tooltip = None

    # can show label popup
    label_popup = True

###################

    def __init__(self):
        LayoutItem.__init__(self)

    def configure_label(self, mod_mask):
        SHIFT = Modifiers.SHIFT
        labels = self.labels

        if labels is None:
            self.label = self.secondary_label = ""
            return

        # primary label
        label = labels.get(mod_mask)
        if label is None:
            mask = mod_mask & LABEL_MODIFIERS
            label = labels.get(mask)

        # secondary label, usually the label of the shift state
        secondary_label = None
        if not label is None:
            if mod_mask & SHIFT:
                mask = mod_mask & ~SHIFT
            else:
                mask = mod_mask | SHIFT

            secondary_label = labels.get(mask)
            if secondary_label is None:
                mask = mask & LABEL_MODIFIERS
                secondary_label = labels.get(mask)

            # Only keep secondary labels that show different characters
            if not secondary_label is None and \
               secondary_label.upper() == label.upper():
                secondary_label = None

        if label is None:
            # legacy fallback for 0.98 behavior and virtkey until 0.61.0
            if mod_mask & Modifiers.SHIFT:
                if mod_mask & Modifiers.ALTGR and 129 in labels:
                    label = labels[129]
                elif 1 in labels:
                    label = labels[1]
                elif 2 in labels:
                    label = labels[2]

            elif mod_mask & Modifiers.ALTGR and 128 in labels:
                label = labels[128]

            elif mod_mask & Modifiers.CAPS:  # CAPS lock
                if 2 in labels:
                    label = labels[2]
                elif 1 in labels:
                    label = labels[1]

        if label is None:
            label = labels.get(0)

        if label is None:
            label = ""

        self.label = label
        self.secondary_label = secondary_label

    def draw_label(self, context = None):
        raise NotImplementedError()

    def set_labels(self, labels):
        self.labels = labels
        self.configure_label(0)

    def get_label(self):
        return self.label

    def get_secondary_label(self):
        return self.secondary_label

    def is_active(self):
        return not self.type is None

    def get_id(self):
        return ""

    def set_id(self, id, theme_id = None, svg_id = None):
        self.theme_id, self.id = self.split_id(id)
        if theme_id:
            self.theme_id = theme_id
        self.svg_id = self.id if not svg_id else svg_id

    @staticmethod
    def split_id(value):
        """
        The theme id has the form <id>.<arbitrary identifier>, where
        the identifier should be a description of the location of
        the key relative to its surroundings, e.g. 'DELE.next-to-backspace'.
        Don't use layout names or layer ids for the theme id, they lose
        their meaning when layouts are copied or renamed by users.
        """
        theme_id = value
        id = value.split(".")[0]
        return theme_id, id

    def build_theme_id(self, prefix = None):
        if prefix is None:
            prefix = self.id
        theme_id = prefix
        comps = self.theme_id.split(".")[1:]
        if comps:
            theme_id += "." + comps[0]
        return theme_id

    def is_layer_button(self):
        return self.id.startswith("layer")

    def is_prediction_key(self):
        return self.id.startswith("prediction")

    def is_correction_key(self):
        return self.id.startswith("correction") or \
               self.id in ["expand-corrections"]

    def is_word_suggestion(self):
        return self.is_prediction_key() or self.is_correction_key()

    def is_modifier(self):
        """
        Modifiers are all latchable/lockable non-button keys:
        "LWIN", "RTSH", "LFSH", "RALT", "LALT",
        "RCTL", "LCTL", "CAPS", "NMLK"
        """
        return bool(self.modifier)

    def is_click_type_key(self):
        return self.id in ["singleclick",
                           "secondaryclick",
                           "middleclick",
                           "doubleclick",
                           "dragclick"]
    def is_button(self):
        return self.type == BUTTON_TYPE

    def is_pressed_only(self):
        return self.pressed and not (self.active or \
                                     self.locked or \
                                     self.scanned)

    def is_text_changing(self):
        if not self.is_modifier() and \
               self.type in [KEYCODE_TYPE,
                             KEYSYM_TYPE,
                             CHAR_TYPE,
                             KEYPRESS_NAME_TYPE,
                             MACRO_TYPE,
                             WORD_TYPE,
                             CORRECTION_TYPE]:
            id = self.id
            if not (id.startswith("F") and id[1:].isdigit()) and \
               not id in set(["LEFT", "RGHT", "UP", "DOWN",
                              "HOME", "END", "PGUP", "PGDN",
                              "INS", "ESC", "MENU",
                              "Prnt", "Pause", "Scroll"]):
                return True
        return False

    def is_return(self):
        id = self.id
        return id == "RTRN" or \
               id == "KPEN"

    def get_layer_index(self):
        assert(self.is_layer_button())
        return int(self.id[5:])

    def get_popup_layout(self):
        if self.popup_id:
            return self.find_sublayout(self.popup_id)
        return None

    def can_show_label_popup(self):
        return not self.is_modifier() and \
               not self.is_layer_button() and \
               not self.type is None and \
               bool(self.label_popup)


class RectKeyCommon(KeyCommon):
    """ An abstract class for rectangular keyboard buttons """

    # optional path data for keys with arbitrary shapes
    geometry = None

    # size of rounded corners at 100% round_rect_radius
    chamfer_size = None

    # Optional key_style to override the default theme's style.
    style = None

    # Toggles for what gets drawn.
    show_face = True
    show_border = True
    show_label = True
    show_image = True

    def __init__(self, id, border_rect):
        KeyCommon.__init__(self)
        self.id = id
        self.colors = {}
        self.context.log_rect = border_rect \
                                if not border_rect is None else Rect()

    def get_id(self):
        return self.id

    def draw(self, context = None):
        pass

    def align_label(self, label_size, key_size, ltr = True):
        """ returns x- and yoffset of the aligned label """
        label_x_align = self.label_x_align
        label_y_align = self.label_y_align
        if not ltr:  # right to left script?
            label_x_align = 1.0 - label_x_align
        xoffset = label_x_align * (key_size[0] - label_size[0])
        yoffset = label_y_align * (key_size[1] - label_size[1])
        return xoffset, yoffset

    def align_secondary_label(self, label_size, key_size, ltr = True):
        """ returns x- and yoffset of the aligned label """
        label_x_align = 0.97
        label_y_align = 0.0
        if not ltr:  # right to left script?
            label_x_align = 1.0 - label_x_align
        xoffset = label_x_align * (key_size[0] - label_size[0])
        yoffset = label_y_align * (key_size[1] - label_size[1])
        return xoffset, yoffset

    def align_popup_indicator(self, label_size, key_size, ltr = True):
        """ returns x- and yoffset of the aligned label """
        label_x_align = 1.0
        label_y_align = self.label_y_align
        if not ltr:  # right to left script?
            label_x_align = 1.0 - label_x_align
        xoffset = label_x_align * (key_size[0] - label_size[0])
        yoffset = label_y_align * (key_size[1] - label_size[1])
        return xoffset, yoffset

    def get_style(self):
        if not self.style is None:
            return self.style
        return config.theme_settings.key_style

    def get_stroke_width(self):
        return config.theme_settings.key_stroke_width / 100.0

    def get_stroke_gradient(self):
        return config.theme_settings.key_stroke_gradient / 100.0

    def get_light_direction(self):
        return config.theme_settings.key_gradient_direction * pi / 180.0

    def get_fill_color(self):
        return self._get_color("fill")

    def get_stroke_color(self):
        return self._get_color("stroke")

    def get_label_color(self):
        return self._get_color("label")

    def get_secondary_label_color(self):
        return self._get_color("secondary-label")

    def get_dwell_progress_color(self):
        return self._get_color("dwell-progress")

    def get_dwell_progress_canvas_rect(self):
        rect = self.get_label_rect().inflate(0.5)
        return self.context.log_to_canvas_rect(rect)

    def _get_color(self, element):
        color_key = (element, self.prelight, self.pressed,
                              self.active, self.locked,
                              self.sensitive, self.scanned)
        rgba = self.colors.get(color_key)
        if not rgba:
            if self.color_scheme:
                rgba = self.color_scheme.get_key_rgba(self, element)
            elif element == "label":
                rgba = [0.0, 0.0, 0.0, 1.0]
            else:
                rgba = [1.0, 1.0, 1.0, 1.0]
            self.colors[color_key] = rgba
        return rgba

    def get_fullsize_rect(self):
        """ Get bounding box of the key at 100% size in logical coordinates """
        return LayoutItem.get_rect(self)

    def get_canvas_fullsize_rect(self):
        """ Get bounding box of the key at 100% size in canvas coordinates """
        return self.context.log_to_canvas_rect(self.get_fullsize_rect())

    def get_unpressed_rect(self):
        """
        Get bounding box in logical coordinates.
        Just the relatively static unpressed rect withough fake key action.
        """
        rect = self.get_fullsize_rect()
        return self._apply_key_size(rect)

    def get_rect(self):
        """ Get bounding box in logical coordinates """
        return self.get_sized_rect()

    def get_sized_rect(self, horizontal = None):
        rect = self.get_fullsize_rect()

        # fake physical key action
        if self.pressed:
            dx, dy, dw, dh = self.get_pressed_deltas()
            rect.x += dx
            rect.y += dy
            rect.w += dw
            rect.h += dh

        return self._apply_key_size(rect, horizontal)

    @staticmethod
    def _apply_key_size(rect, horizontal = None):
        """ shrink keys to key_size """
        scale = (1.0 - config.theme_settings.key_size / 100.0) * 0.5
        bx = rect.w * scale
        by = rect.h * scale

        if horizontal is None:
            horizontal = rect.h < rect.w

        if horizontal:
            # keys with aspect > 1.0, e.g. space, shift
            bx = by
        else:
            # keys with aspect < 1.0, e.g. click, move, number block + and enter
            by = bx

        return rect.deflate(bx, by)

    def get_pressed_deltas(self):
        """
        dx, dy, dw, dh for fake physical key action of pressed keys.
        Logical coordinate system.
        """
        key_style = self.get_style()
        if key_style == "gradient":
            k = 0.2
        elif key_style == "dish":
            k = 0.45
        else:
            k = 0.0
        return k, 2*k, 0.0, 0.0

    def get_label_rect(self, rect = None):
        """ Label area in logical coordinates """
        if rect is None:
            rect = self.get_rect()
        style = self.get_style()
        if style == "dish":
            stroke_width  = self.get_stroke_width()
            border_x, border_y = config.DISH_KEY_BORDER
            border_x *= stroke_width
            border_y *= stroke_width
            rect = rect.deflate(border_x, border_y)
            rect.y -= config.DISH_KEY_Y_OFFSET * stroke_width
            return rect
        else:
            return rect.deflate(*self.label_margin)

    def get_canvas_label_rect(self):
        log_rect = self.get_label_rect()
        return self.context.log_to_canvas_rect(log_rect)

    def get_border_path(self):
        """ Original path including border in logical coordinates. """
        return self.geometry.get_full_size_path()

    def get_path(self):
        """
        Path of the key geometry in logical coordinates.
        Key size and fake press movement are applied.
        """
        offset_x, offset_y, size_x, size_y = self.get_key_offset_size()
        return self.geometry.get_transformed_path(offset_x, offset_y,
                                                  size_x, size_y)

    def get_canvas_border_path(self):
        path = self.get_border_path()
        return self.context.log_to_canvas_path(path)

    def get_canvas_path(self):
        path = self.get_path()
        return self.context.log_to_canvas_path(path)

    def get_hit_path(self):
        return self.get_canvas_border_path()

    def get_chamfer_size(self, rect = None):
        """ Max size of the rounded corner areas in logical coordinates. """
        if not self.chamfer_size is None:
            return self.chamfer_size
        if not rect:
            if self.geometry:
                rect = self.get_border_path().get_bounds()
            else:
                rect = self.get_rect()
        return min(rect.w, rect.h) * 0.5

    def get_key_offset_size(self, geometry = None):
        size_x = size_y = config.theme_settings.key_size / 100.0
        offset_x = offset_y = 0.0

        if self.pressed:
            offset_x, offset_y, dw, dh = self.get_pressed_deltas()
            if dw != 0.0 or dh != 0.0:
                if geometry is None:
                    geometry = self.geometry
                dw, dh = geometry.scale_log_to_size((dw, dh))
                size_x += dw * 0.5
                size_y += dh * 0.5

        return offset_x, offset_y, size_x, size_y

    def get_canvas_polygons(self, geometry,
                          offset_x, offset_y, size_x, size_y,
                          radius_pct, chamfer_size):
        path = geometry.get_transformed_path(offset_x, offset_y, size_x, size_y)
        canvas_path = self.context.log_to_canvas_path(path)
        polygons = list(canvas_path.iter_polygons())
        polygon_paths = \
            [polygon_to_rounded_path(p, radius_pct, chamfer_size) \
            for p in polygons]
        return polygons, polygon_paths


class InputlineKeyCommon(RectKeyCommon):
    """ An abstract class for InputLine keyboard buttons """

    line = ""
    word_infos = None
    cursor = 0

    def __init__(self, name, border_rect):
        RectKeyCommon.__init__(self, name, border_rect)

    def get_label(self):
        return ""


class KeyGeometry:
    """
    Full description of a key's shape.

    This class generates path variants for a given key_size by path
    interpolation. This allows for key_size dependent shape changes,
    controlled solely by a SVG layout file. See 'Return' key in
    'Full Keyboard' layout for an example.
    """

    path0 = None          # KeyPath at 100% size
    path1 = None          # KepPath at 50% size, optional

    @staticmethod
    def from_paths(paths):
        assert(len(paths) >= 1)

        path0 = paths[0]
        path1 = None
        if len(paths) >= 2:
            path1 = paths[1]

            # Equal number of path segments?
            if len(path0.segments) != len(path1.segments):
                raise ValueError(
                    "paths to interpolate differ in number of segments "
                    "({} vs. {})" \
                        .format(len(path0.segments), len(path1.segments)))

            # Same operations in all path segments?
            for i in range(len(path0.segments)):
                op0, coords0 = path0.segments[i]
                op1, coords1 = path1.segments[i]
                if op0 != op1:
                    raise ValueError(
                        "paths to interpolate have different operations "
                        "at segment {} (op. {} vs. op. {})" \
                            .format(i, op0, op1))

        geometry = KeyGeometry()
        geometry.path0 = path0
        geometry.path1 = path1
        return geometry

    @staticmethod
    def from_rect(rect):
        geometry = KeyGeometry()
        geometry.path0 = KeyPath.from_rect(rect)
        return geometry

    def get_transformed_path(self, offset_x = 0.0, offset_y = 0.0,
                             size_x = 1.0, size_y = 1.0):
        """
        Everything in the logical coordinate system.
        size: 1.0 => path0, 0.5 => path1
        """
        path0 = self.path0
        path1 = self.path1
        if path1:
            pos_x = (1 - size_x) * 2.0
            pos_y = (1 - size_y) * 2.0
            return path0.linint(path1, pos_x, pos_y, offset_x, offset_y)
        else:
            r0 = self.get_full_size_bounds()
            r1 = self.get_half_size_bounds()
            rect = r1.inflate((size_x - 0.5) * (r0.w - r1.w),
                              (size_y - 0.5) * (r0.h - r1.h))
            rect.x += offset_x
            rect.y += offset_y
            return path0.fit_in_rect(rect)

    def get_full_size_path(self):
        return self.path0

    def get_full_size_bounds(self):
        """
        Bounding box at size 1.0.
        """
        return self.path0.get_bounds()

    def get_half_size_bounds(self):
        """
        Bounding box at size 0.5.
        """
        path1 = self.path1
        if path1:
            rect = path1.get_bounds()
        else:
            rect = self.path0.get_bounds()
            if rect.h < rect.w:
                dx = dy = rect.h * 0.25
            else:
                dy = dx = rect.w * 0.25
            rect = rect.deflate(dx, dy)
        return rect

    def scale_log_to_size(self, v):
        """ Scale from logical distances to key size. """
        r0 = self.get_full_size_bounds()
        r1 = self.get_half_size_bounds()
        log_h = (r0.h - r1.h) * 2.0
        log_w = (r0.w - r1.w) * 2.0
        return (v[0] / log_h,
                v[1] / log_w)

    def scale_size_to_log(self, v):
        """ Scale from logical distances to key size. """
        r0 = self.get_full_size_bounds()
        r1 = self.get_half_size_bounds()
        log_h = (r0.h - r1.h) * 2.0
        log_w = (r0.w - r1.w) * 2.0
        return (v[0] * log_h,
                v[1] * log_w)


class KeyPath:
    """
    Cairo-friendly path description for non-rectangular keys.
    Can handle straight line-loops/polygons, but not arcs and splines.
    """
    (
        MOVE_TO,
        LINE_TO,
        CLOSE_PATH,
    ) = range(3)

    _last_abs_pos = (0.0, 0.0)
    _bounds = None           # cached bounding box

    def __init__(self):
        self.segments = []   # normalized list of path segments (all absolute)

    @staticmethod
    def from_svg_path(path_str):
        path = KeyPath()
        path.append_svg_path(path_str)
        return path

    @staticmethod
    def from_rect(rect):
        x0 = rect.x
        y0 = rect.y
        x1 = rect.right()
        y1 = rect.bottom()
        path = KeyPath()
        path.segments = [[KeyPath.MOVE_TO, [x0, y0]],
                         [KeyPath.LINE_TO, [x1, y0, x1, y1, x0, y1]],
                         [KeyPath.CLOSE_PATH, []]]
        path._bounds = rect.copy()
        return path

    _svg_path_pattern = re.compile("([+-]?[0-9.]*)")

    def copy(self):
        result = KeyPath()
        for op, coords in self.segments:
            result.segments.append([op, coords[:]])
        return result

    def append_svg_path(self, path_str):
        """
        Append a SVG path data string to the path.

        Doctests:
        # absolute move_to command
        >>> p = KeyPath.from_svg_path("M 100 200 120 -220")
        >>> print(p.segments)
        [[0, [100.0, 200.0]], [1, [120.0, -220.0]]]

        # relative move_to command
        >>> p = KeyPath.from_svg_path("m 100 200 10 -10")
        >>> print(p.segments)
        [[0, [100.0, 200.0]], [1, [110.0, 190.0]]]

        # relative move_to and close_path segments
        >>> p = KeyPath.from_svg_path("m 100 200 10 -10 z")
        >>> print(p.segments)
        [[0, [100.0, 200.0]], [1, [110.0, 190.0]], [2, []]]

        # spaces and commas and are optional where possible
        >>> p = KeyPath.from_svg_path("m100,200 10-10z")
        >>> print(p.segments)
        [[0, [100.0, 200.0]], [1, [110.0, 190.0]], [2, []]]
        """

        cmd_str = ""
        coords = []
        tokens = self._tokenize_svg_path(path_str)
        for token in tokens:
            try:
                val = float(token)   # raises value error
                coords.append(val)
            except ValueError:
                if token.isalpha():
                    if cmd_str:
                        self.append_command(cmd_str, coords)
                    cmd_str = token
                    coords = []

                elif token == ",":
                    pass

                else:
                    raise ValueError(
                          "unexpected token '{}' in svg path data" \
                          .format(token))

        if cmd_str:
            self.append_command(cmd_str, coords)

    def append_command(self, cmd_str, coords):
        """
        Append a single command and it's coordinate data to the path.

        Doctests:
        # first lowercase move_to position is absolute
        >>> p = KeyPath()
        >>> p.append_command("m", [100, 200])
        >>> print(p.segments)
        [[0, [100, 200]]]

        # move_to segments become line_to segments after the first position
        >>> p = KeyPath()
        >>> p.append_command("M", [100, 200, 110, 190])
        >>> print(p.segments)
        [[0, [100, 200]], [1, [110, 190]]]

        # further lowercase move_to positions are relative, must become absolute
        >>> p = KeyPath()
        >>> p.append_command("m", [100, 200, 10, -10, 10, -10])
        >>> print(p.segments)
        [[0, [100, 200]], [1, [110, 190, 120, 180]]]

        # further lowercase segments must still be become absolute
        >>> p = KeyPath()
        >>> p.append_command("m", [100, 200, 10, -10, 10, -10])
        >>> p.append_command("l", [1, -1, 1, -1])
        >>> print(p.segments)
        [[0, [100, 200]], [1, [110, 190, 120, 180]], [1, [121, 179, 122, 178]]]
        """

        # Convert lowercase segments from relative to absolute coordinates.
        if cmd_str in ("m", "l"):

            # Don't convert the very first coordinate, it is already absolute.
            if self.segments:
                start = 0
                x, y = self._last_abs_pos
            else:
                start = 2
                x, y = coords[0], coords[1]

            for i in range(start, len(coords), 2):
                x += coords[i]
                y += coords[i+1]
                coords[i]   = x
                coords[i+1] = y

        cmd = cmd_str.lower()
        if cmd == "m":
            self.segments.append([self.MOVE_TO, coords[:2]])
            if len(coords) > 2:
                self.segments.append([self.LINE_TO, coords[2:]])

        elif cmd == "l":
            self.segments.append([self.LINE_TO, coords])

        elif cmd == "z":
            self.segments.append([self.CLOSE_PATH, []])

        # remember last absolute position
        if len(coords) >= 2:
            self._last_abs_pos = coords[-2:]

    @staticmethod
    def _tokenize_svg_path(path_str):
        """
        Split SVG path date into command and coordinate tokens.

        Doctests:
        >>> KeyPath._tokenize_svg_path("m 10,20")
        ['m', '10', ',', '20']
        >>> KeyPath._tokenize_svg_path("   m   10  , \\n  20 ")
        ['m', '10', ',', '20']
        >>> KeyPath._tokenize_svg_path("m 10,20 30,40 z")
        ['m', '10', ',', '20', '30', ',', '40', 'z']
        >>> KeyPath._tokenize_svg_path("m10,20 30,40z")
        ['m', '10', ',', '20', '30', ',', '40', 'z']
        >>> KeyPath._tokenize_svg_path("M100.32 100.09 100. -100.")
        ['M', '100.32', '100.09', '100.', '-100.']
        >>> KeyPath._tokenize_svg_path("m123+23 20,-14L200,200")
        ['m', '123', '+23', '20', ',', '-14', 'L', '200', ',', '200']
        >>> KeyPath._tokenize_svg_path("m123+23 20,-14L200,200")
        ['m', '123', '+23', '20', ',', '-14', 'L', '200', ',', '200']
        """
        tokens = [token.strip() \
                  for token in KeyPath._svg_path_pattern.split(path_str)]
        return [token for token in tokens if token]

    def get_bounds(self):
        bounds = self._bounds
        if bounds is None:
            bounds = self._calc_bounds()
            self._bounds = bounds
        return bounds

    def _calc_bounds(self):
        """
        Compute the bounding box of the path.

        Doctests:
        # Simple move_to path, something inkscape would create.
        >>> p = KeyPath.from_svg_path("m 100,200 10,-10 z")
        >>> print(p.get_bounds())
        Rect(x=100.0 y=190.0 w=10.0 h=10.0)
        """

        try:
            xmin = xmax = self.segments[0][1][0]
            ymin = ymax = self.segments[0][1][1]
        except IndexError:
            return Rect()

        for command in self.segments:
            coords = command[1]
            for i in range(0, len(coords), 2):
                x = coords[i]
                y = coords[i+1]
                if xmin > x:
                    xmin = x
                if xmax < x:
                    xmax = x
                if ymin > y:
                    ymin = y
                if ymax < y:
                    ymax = y

        return Rect(xmin, ymin, xmax - xmin, ymax - ymin)

    def inflate(self, dx, dy = None):
        """
        Returns a new path which is larger by dx and dy on all sides.
        """
        rect = self.get_bounds().inflate(dx, dy)
        return self.fit_in_rect(rect)

    def fit_in_rect(self, rect):
        """
        Scales and translates the path so that rect
        becomes its new bounding box.
        """
        result = self.copy()
        bounds = self.get_bounds()
        scalex = rect.w / bounds.w
        scaley = rect.h / bounds.h
        dorgx, dorgy = bounds.get_center()
        dx = rect.x - (dorgx + (bounds.x - dorgx) * scalex)
        dy = rect.y - (dorgy + (bounds.y - dorgy) * scaley)

        for op, coords in result.segments:
            for i in range(0, len(coords), 2):
                coords[i] = dx + dorgx + (coords[i] - dorgx) * scalex
                coords[i+1] = dy + dorgy + (coords[i+1] - dorgy) * scaley

        return result

    def linint(self, path1, pos_x = 1.0, pos_y = 1.0,
               offset_x = 0.0, offset_y = 0.0):
        """
        Interpolate between self and path1.
        Paths must have the same structure (length and operations).
        pos: 0.0 = self, 1.0 = path1.
        """
        result = self.copy()
        segments = result.segments
        segments1 = path1.segments
        for i in range(len(segments)):
            op, coords = segments[i]
            op1, coords1 = segments1[i]
            for j in range(0, len(coords), 2):
                x = coords[j]
                y = coords[j+1]
                x1 = coords1[j]
                y1 = coords1[j+1]
                dx = x1 - x
                dy = y1 - y
                coords[j] = x + pos_x * dx + offset_x
                coords[j+1] = y + pos_y * dy + offset_y

        return result

    def iter_polygons(self):
        """
        Loop through all independent polygons in the path.
        Can't handle splines and arcs, everything has to
        be polygons from here.
        """
        polygon = []

        for op, coords in self.segments:

            if op == self.LINE_TO:
                polygon.extend(coords)

            elif op == self.MOVE_TO:
                polygon = []
                polygon.extend(coords)

            elif op == self.CLOSE_PATH:
                yield polygon

    def is_point_within(self, point):
        for polygon in self.iter_polygons():
            if self.is_point_in_polygon(polygon, point[0], point[1]):
                return True

    @staticmethod
    def is_point_in_polygon(vertices, x, y):
        c = False
        n = len(vertices)

        try:
            x0 = vertices[n - 2]
            y0 = vertices[n - 1]
        except IndexError:
            return False

        for i in range(0, n, 2):
            x1 = vertices[i]
            y1 = vertices[i+1]
            if (y1 <= y and y < y0 or y0 <= y and y < y1) and \
               (x < (x0 - x1) * (y - y1) / (y0 - y1) + x1):
                c = not c
            x0 = x1
            y0 = y1

        return c


