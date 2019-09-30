# -*- coding: utf-8 -*-

# Copyright © 2013, marmuta
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

"""
Global definitions.
"""

from __future__ import division, print_function, unicode_literals

from gi.repository import Gdk

class StatusIconProviderEnum:
    (
        GtkStatusIcon,
        AppIndicator,
    ) = range(2)

class InputEventSourceEnum:
    (
        GTK,
        XINPUT,
    ) = range(2)


class TouchInputEnum:
    (
        NONE,
        SINGLE,
        MULTI,
    ) = range(3)

# window corners
class Handle:
    NORTH_WEST = Gdk.WindowEdge.NORTH_WEST
    NORTH = Gdk.WindowEdge.NORTH
    NORTH_EAST = Gdk.WindowEdge.NORTH_EAST
    WEST = Gdk.WindowEdge.WEST
    EAST = Gdk.WindowEdge.EAST
    SOUTH_WEST = Gdk.WindowEdge.SOUTH_WEST
    SOUTH = Gdk.WindowEdge.SOUTH
    SOUTH_EAST   = Gdk.WindowEdge.SOUTH_EAST
    class MOVE: pass

Handle.EDGES  =   (Handle.EAST,
                   Handle.SOUTH,
                   Handle.WEST,
                   Handle.NORTH)

Handle.CORNERS =  (Handle.SOUTH_EAST,
                   Handle.SOUTH_WEST,
                   Handle.NORTH_WEST,
                   Handle.NORTH_EAST)

Handle.RESIZERS = (Handle.EAST,
                   Handle.SOUTH_EAST,
                   Handle.SOUTH,
                   Handle.SOUTH_WEST,
                   Handle.WEST,
                   Handle.NORTH_WEST,
                   Handle.NORTH,
                   Handle.NORTH_EAST)

Handle.TOP_RESIZERS = (
                   Handle.EAST,
                   Handle.WEST,
                   Handle.NORTH_WEST,
                   Handle.NORTH,
                   Handle.NORTH_EAST)

Handle.BOTTOM_RESIZERS = (
                   Handle.EAST,
                   Handle.SOUTH_EAST,
                   Handle.SOUTH,
                   Handle.SOUTH_WEST,
                   Handle.WEST)

Handle.ALL = Handle.RESIZERS + (Handle.MOVE, )

Handle.CURSOR_TYPES = {
    Handle.NORTH_WEST : Gdk.CursorType.TOP_LEFT_CORNER,
    Handle.NORTH      : Gdk.CursorType.TOP_SIDE,
    Handle.NORTH_EAST : Gdk.CursorType.TOP_RIGHT_CORNER,
    Handle.WEST       : Gdk.CursorType.LEFT_SIDE,
    Handle.EAST       : Gdk.CursorType.RIGHT_SIDE,
    Handle.SOUTH_WEST : Gdk.CursorType.BOTTOM_LEFT_CORNER,
    Handle.SOUTH      : Gdk.CursorType.BOTTOM_SIDE,
    Handle.SOUTH_EAST : Gdk.CursorType.BOTTOM_RIGHT_CORNER,
    Handle.MOVE       : Gdk.CursorType.FLEUR}

Handle.IDS = {
    Handle.EAST       : "E",
    Handle.SOUTH_WEST : "SW",
    Handle.SOUTH      : "S",
    Handle.SOUTH_EAST : "SE",
    Handle.WEST       : "W",
    Handle.NORTH_WEST : "NW",
    Handle.NORTH      : "N",
    Handle.NORTH_EAST : "NE",
    Handle.MOVE       : "M"}

Handle.RIDS = {
    "E"  : Handle.EAST,
    "SW" : Handle.SOUTH_WEST,
    "S"  : Handle.SOUTH,
    "SE" : Handle.SOUTH_EAST,
    "W"  : Handle.WEST,
    "NW" : Handle.NORTH_WEST,
    "N"  : Handle.NORTH,
    "NE" : Handle.NORTH_EAST,
    "M"  : Handle.MOVE}


class DockingEdge:
    TOP = 0
    BOTTOM = 3



