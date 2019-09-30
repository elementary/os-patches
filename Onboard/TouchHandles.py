# -*- coding: utf-8 -*-
""" Enlarged drag handles for resizing or moving """

from __future__ import division, print_function, unicode_literals

from math import pi, sqrt, sin, log
import cairo

from Onboard.utils       import Rect, drop_shadow
from Onboard.definitions import Handle

### Logging ###
import logging
from functools import reduce
_logger = logging.getLogger("TouchHandles")
###############

class TouchHandle(object):
    """ Enlarged drag handle for resizing or moving """

    id = None
    prelight = False
    pressed = False
    corner_radius = 0     # radius of the outer corners (window edges)

    _size = (40, 40)
    _fallback_size = (40, 40)
    _hit_proximity_factor = 1.5
    _rect = None
    _scale = 1.0   # scale of handle relative to resize handles
    _handle_alpha = 0.45
    _shadow_alpha = 0.04
    _shadow_size = 8
    _shadow_offset = (0.0, 2.0)
    _screen_dpi = 96

    _handle_angles = {}  # dictionary at class scope!

    lock_x_axis = False
    lock_y_axis = False

    def __init__(self, id):
        self.id = id

        # initialize angles
        if not self._handle_angles:
            for i, h in enumerate(Handle.EDGES):
                self._handle_angles[h] = i * pi / 2.0
            for i, h in enumerate(Handle.CORNERS):
                self._handle_angles[h] = i * pi / 2.0 + pi /  4.0
            self._handle_angles[Handle.MOVE] = 0.0

    def get_rect(self):
        rect = self._rect
        if not rect is None and \
           self.pressed:
            rect = rect.offset(1.0, 1.0)
        return rect

    def get_radius(self):
        w, h = self.get_rect().get_size()
        return min(w, h) / 2.0

    def get_shadow_rect(self):
        rect = self.get_rect()
        if rect:
            rect = rect.inflate(self._shadow_size+1)
            rect.w += self._shadow_offset[0]
            rect.h += self._shadow_offset[1]
        return rect

    def get_arrow_angle(self):
        return self._handle_angles[self.id]

    def is_edge_handle(self):
        return self.id in Handle.EDGES

    def is_corner_handle(self):
        return self.id in Handle.CORNERS

    def update_position(self, canvas_rect):
        w, h = self._size
        w = min(w, canvas_rect.w / 3.0)
        w = min(w, canvas_rect.h / 3.0)
        h = w
        self._scale = 1.0

        xc, yc = canvas_rect.get_center()
        if self.id is Handle.MOVE:  # move handle?
            d = min(canvas_rect.w - 2.0 * w, canvas_rect.h - 2.0 * h)
            self._scale = 1.4
            w = min(w * self._scale, d)
            h = min(h * self._scale, d)

        if self.id in [Handle.WEST,
                       Handle.NORTH_WEST,
                       Handle.SOUTH_WEST]:
            x = canvas_rect.left()
        if self.id in [Handle.NORTH,
                       Handle.NORTH_WEST,
                       Handle.NORTH_EAST]:
            y = canvas_rect.top()
        if self.id in [Handle.EAST,
                       Handle.NORTH_EAST,
                       Handle.SOUTH_EAST]:
            x = canvas_rect.right() - w
        if self.id in [Handle.SOUTH,
                       Handle.SOUTH_WEST,
                       Handle.SOUTH_EAST]:
            y = canvas_rect.bottom() - h

        if self.id in [Handle.MOVE, Handle.EAST, Handle.WEST]:
            y = yc - h / 2.0
        if self.id in [Handle.MOVE, Handle.NORTH, Handle.SOUTH]:
            x = xc - w / 2.0

        self._rect = Rect(x, y, w, h)

    def hit_test(self, point):
        rect   = self.get_rect().grow(self._hit_proximity_factor)
        radius = self.get_radius() * self._hit_proximity_factor

        if rect and rect.is_point_within(point):
            _win = self._window.get_window()
            if _win:
                context = _win.cairo_create()
                self._build_handle_path(context, rect, radius)
                return context.in_fill(*point)
        return False

        xc, yc = rect.get_center()
        dx = xc - point[0]
        dy = yc - point[1]
        d = sqrt(dx*dx + dy*dy)
        return d <= radius

    def draw(self, context):
        if self.pressed:
            alpha_factor = 1.5
        else:
            alpha_factor = 1.0

        context.new_path()

        self._draw_handle_shadow(context, alpha_factor)
        self._draw_handle(context, alpha_factor)
        self._draw_arrows(context)

    def _draw_handle(self, context, alpha_factor):
        radius = self.get_radius()
        line_width = radius / 15.0

        alpha = self._handle_alpha * alpha_factor
        if self.pressed:
            context.set_source_rgba(0.78, 0.33, 0.17, alpha)
        elif self.prelight:
            context.set_source_rgba(0.98, 0.53, 0.37, alpha)
        else:
            context.set_source_rgba(0.78, 0.33, 0.17, alpha)

        self._build_handle_path(context)
        context.fill_preserve()
        context.set_line_width(line_width)
        context.stroke()

    def _draw_handle_shadow(self, context, alpha_factor):
        rect = self.get_rect()

        context.save()

        # There is a massive performance boost for groups when clipping is used.
        # Integer limits are again dramatically faster (x4) then using floats.
        # for 1000x draw_drop_shadow:
        #     with clipping: ~300ms, without: ~11000ms
        context.rectangle(*self.get_shadow_rect().int())
        context.clip()

        context.push_group()

        # draw the shadow
        context.push_group_with_content(cairo.CONTENT_ALPHA)
        self._build_handle_path(context)
        context.set_source_rgba(0.0, 0.0, 0.0, 1.0)
        context.fill()
        group = context.pop_group()
        drop_shadow(context, group, rect,
                    self._shadow_size,
                    self._shadow_offset,
                    self._shadow_alpha,
                    5)

        # cut out the handle area, because the handle is transparent
        context.save()
        context.set_operator(cairo.OPERATOR_CLEAR)
        context.set_source_rgba(0.0, 0.0, 0.0, 1.0)
        self._build_handle_path(context)
        context.fill()
        context.restore()

        context.pop_group_to_source()
        context.paint()

        context.restore()


    def _draw_arrows(self, context):
        radius = self.get_radius()
        xc, yc = self.get_rect().get_center()
        scale = radius / 2.0 / self._scale * 1.2

        angle = self.get_arrow_angle()
        if self.id == Handle.MOVE:
            num_arrows = 4
            if self.lock_x_axis:
                num_arrows -= 2
                angle += pi * 0.5
            if self.lock_y_axis:
                num_arrows -= 2
        else:
            num_arrows = 2
        angle_step = 2.0 * pi / num_arrows

        for i in range(num_arrows):
            context.save()

            context.translate(xc, yc)
            context.rotate(angle + i * angle_step)
            context.scale(scale, scale)

            # arrow distance from center
            if self.id is Handle.MOVE:
                context.translate(0.9, 0)
            else:
                context.translate(0.30, 0)

            self._draw_arrow(context)

            context.restore()

    def _draw_arrow(self, context):
        context.move_to( 0.0, -0.5)
        context.line_to( 0.5,  0.0)
        context.line_to( 0.0,  0.5)
        context.close_path()

        context.set_source_rgba(1.0, 1.0, 1.0, 0.8)
        context.fill_preserve()

        context.set_source_rgba(0.0, 0.0, 0.0, 0.8)
        context.set_line_width(0)
        context.stroke()

    def _build_handle_path(self, context, rect = None, radius = None):
        if rect is None:
            rect = self.get_rect()
        if radius is None:
            radius = self.get_radius()
        xc, yc = rect.get_center()
        corner_radius = self.corner_radius

        angle = self.get_arrow_angle()
        m = cairo.Matrix()
        m.translate(xc, yc)
        m.rotate(angle)

        if self.is_edge_handle():
            p0 = m.transform_point(radius, -radius)
            p1 = m.transform_point(radius, radius)
            context.arc(xc, yc, radius, angle + pi / 2.0, angle + pi / 2.0 + pi)
            context.line_to(*p0)
            context.line_to(*p1)
            context.close_path()
        elif self.is_corner_handle():
            m.rotate(-pi / 4.0)  # rotate to SOUTH_EAST

            context.arc(xc, yc, radius, angle + 3 * pi / 4.0,
                                        angle + 5 * pi / 4.0)
            pt = m.transform_point(radius, -radius)
            context.line_to(*pt)

            if corner_radius:
                # outer corner, following the rounded window corner
                pt  = m.transform_point(radius,  radius - corner_radius)
                ptc = m.transform_point(radius - corner_radius,
                                        radius - corner_radius)
                context.line_to(*pt)
                context.arc(ptc[0], ptc[1], corner_radius,
                            angle - pi / 4.0,  angle + pi / 4.0)
            else:
                pt = m.transform_point(radius,  radius)
                context.line_to(*pt)

            pt = m.transform_point(-radius,  radius)
            context.line_to(*pt)
            context.close_path()
        else:
            context.arc(xc, yc, radius, 0, 2.0 * pi)

    def redraw(self):
        rect = self.get_shadow_rect()
        if rect:
            self._window.queue_draw_area(*rect)


class TouchHandles(object):
    """ Full set of resize and move handles """
    active = False
    opacity = 1.0
    rect = None

    def __init__(self):
        self.handles = []
        self._handle_pool = [TouchHandle(Handle.MOVE),
                             TouchHandle(Handle.NORTH_WEST),
                             TouchHandle(Handle.NORTH),
                             TouchHandle(Handle.NORTH_EAST),
                             TouchHandle(Handle.EAST),
                             TouchHandle(Handle.SOUTH_EAST),
                             TouchHandle(Handle.SOUTH),
                             TouchHandle(Handle.SOUTH_WEST),
                             TouchHandle(Handle.WEST)]

    def set_active_handles(self, handle_ids):
        self.handles = []
        for handle in self._handle_pool:
            if handle.id in handle_ids:
                self.handles.append(handle)

    def set_window(self, window):
        for handle in self._handle_pool:
            handle._window = window

    def update_positions(self, canvas_rect):
        self.rect = canvas_rect
        for handle in self.handles:
            handle.update_position(canvas_rect)

    def draw(self, context):
        if self.opacity:
            clip_rect = Rect.from_extents(*context.clip_extents())
            for handle in self.handles:
                rect = handle.get_shadow_rect()
                if rect.intersects(clip_rect):
                    context.save()
                    context.rectangle(*rect.int())
                    context.clip()
                    context.push_group()

                    handle.draw(context)

                    context.pop_group_to_source()
                    context.paint_with_alpha(self.opacity);
                    context.restore()

    def redraw(self):
        if self.rect:
            for handle in self.handles:
                handle.redraw()

    def hit_test(self, point):
        if self.active:
            for handle in self.handles:
                if handle.hit_test(point):
                    return handle.id

    def set_prelight(self, handle_id):
        for handle in self.handles:
            prelight = handle.id == handle_id and not handle.pressed
            if handle.prelight != prelight:
                handle.prelight = prelight
                handle.redraw()

    def set_pressed(self, handle_id):
        for handle in self.handles:
            pressed = handle.id == handle_id
            if handle.pressed != pressed:
                handle.pressed = pressed
                handle.redraw()

    def set_corner_radius(self, corner_radius):
        for handle in self.handles:
            handle.corner_radius = corner_radius

    def set_monitor_dimensions(self, size_px, size_mm):
        min_monitor_mm = 50
        target_size_mm = (5, 5)
        min_size = TouchHandle._fallback_size

        if size_mm[0] < min_monitor_mm or \
           size_mm[1] < min_monitor_mm:
            w = 0
            h = 0
        else:
            w = size_px[0] / size_mm[0] * target_size_mm[0]
            h = size_px[0] / size_mm[0] * target_size_mm[0]
        size = max(w, min_size[0]), max(h, min_size[1])
        TouchHandle._size = size

    def lock_x_axis(self, lock):
        """ Set to False to constraint movement in x. """
        for handle in self.handles:
            handle.lock_x_axis = lock

    def lock_y_axis(self, lock):
        """ Set to True to constraint movement in y. """
        for handle in self.handles:
            handle.lock_y_axis = lock


