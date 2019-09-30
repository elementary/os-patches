# -*- coding: utf-8 -*-
""" Classes for recursive layout definition """

from __future__ import division, print_function, unicode_literals

from Onboard.utils import Rect, TreeItem

### Config Singleton ###
from Onboard.Config import Config
config = Config()
########################


class KeyContext(object):
    """
    Transforms logical coordinates to canvas coordinates and vice versa.
    """
    def __init__(self):
        # logical rectangle as defined by the keyboard layout,
        # never changed after loading.
        self.initial_log_rect = Rect(0.0, 0.0, 1.0, 1.0)  # includes border

        # logical rectangle as defined by the keyboard layout
        self.log_rect = Rect(0.0, 0.0, 1.0, 1.0)  # includes border

        # canvas rectangle in drawing units
        self.canvas_rect = Rect(0.0, 0.0, 1.0, 1.0)

    def __repr__(self):
        return "log={} canvas={}".format(list(self.log_rect),
                                          list(self.canvas_rect))

    def log_to_canvas(self, coord):
        return (self.log_to_canvas_x(coord[0]), \
                self.log_to_canvas_y(coord[1]))

    def log_to_canvas_rect(self, rect):
        if rect.is_empty():
            return Rect()
        return Rect(self.log_to_canvas_x(rect.x),
                    self.log_to_canvas_y(rect.y),
                    self.scale_log_to_canvas_x(rect.w),
                    self.scale_log_to_canvas_y(rect.h))

    def log_to_canvas_x(self, x):
        canvas_rect = self.canvas_rect
        log_rect = self.log_rect
        return canvas_rect.x + (x - log_rect.x) * canvas_rect.w / log_rect.w

    def log_to_canvas_y(self, y):
        canvas_rect = self.canvas_rect
        log_rect = self.log_rect
        return canvas_rect.y + (y - log_rect.y) * canvas_rect.h / log_rect.h

    def scale_log_to_canvas(self, coord):
        return (self.scale_log_to_canvas_x(coord[0]), \
                self.scale_log_to_canvas_y(coord[1]))

    def scale_log_to_canvas_x(self, x):
        return x * self.canvas_rect.w / self.log_rect.w

    def scale_log_to_canvas_y(self, y):
        return y * self.canvas_rect.h / self.log_rect.h


    def canvas_to_log(self, coord):
        return (self.canvas_to_log_x(coord[0]), \
                self.canvas_to_log_y(coord[1]))

    def canvas_to_log_rect(self, rect):
        return Rect(self.canvas_to_log_x(rect.x),
                    self.canvas_to_log_y(rect.y),
                    self.scale_canvas_to_log_x(rect.w),
                    self.scale_canvas_to_log_y(rect.h))

    def canvas_to_log_x(self, x):
        canvas_rect = self.canvas_rect
        log_rect = self.log_rect
        return (x - canvas_rect.x) * log_rect.w / canvas_rect.w + log_rect.x

    def canvas_to_log_y(self, y):
        canvas_rect = self.canvas_rect
        log_rect = self.log_rect
        return (y - canvas_rect.y) * log_rect.h / canvas_rect.h + log_rect.y


    def scale_canvas_to_log_x(self, x):
        return x * self.log_rect.w / self.canvas_rect.w

    def scale_canvas_to_log_y(self, y):
        return y * self.log_rect.h / self.canvas_rect.h

    def log_to_canvas_path(self, path):
        result = path.copy()
        log_to_canvas_x = self.log_to_canvas_x
        log_to_canvas_y = self.log_to_canvas_y
        for op, coords in result.segments:
            for i in range(0, len(coords), 2):
                coords[i]   = log_to_canvas_x(coords[i])
                coords[i+1] = log_to_canvas_y(coords[i+1])
        return result

    ##### Speed-optimized overloads #####

    def log_to_canvas(self, coord):
        canvas_rect = self.canvas_rect
        log_rect = self.log_rect
        return canvas_rect.x + (coord[0] - log_rect.x) * \
                             canvas_rect.w / log_rect.w, \
               canvas_rect.y + (coord[1] - log_rect.y) * \
                             canvas_rect.h / log_rect.h

    def log_to_canvas_rect(self, rect):
        """ ~50% faster than the above. """
        w = rect.w
        h = rect.h
        if w <= 0 or h <= 0:
            return Rect()

        canvas_rect = self.canvas_rect
        log_rect = self.log_rect
        scale_w = canvas_rect.w / log_rect.w
        scale_h = canvas_rect.h / log_rect.h

        return Rect(canvas_rect.x + (rect.x - log_rect.x) * scale_w,
                    canvas_rect.y + (rect.y - log_rect.y) * scale_h,
                    w * scale_w,
                    h * scale_h)

    def scale_log_to_canvas(self, coord):
        canvas_rect = self.canvas_rect
        log_rect = self.log_rect
        return coord[0] * canvas_rect.w / log_rect.w, \
               coord[1] * canvas_rect.h / log_rect.h


class LayoutRoot:
    """
    Decorator class wrapping the root item.
    Implements extensive caching to avoid most of the expensive
    (for python) traversal of the layout tree.
    """
    def __init__(self, item):
        self.__dict__['_item'] = item    # item to decorate
        self.invalidate_caches()
        self.init_chamfer_sizes()

    def __getattr__(self, name):
        return getattr(self._item, name)

    def __setattr__(self, name, value):
        self._item.__setattr__(name, value)

    def invalidate_caches(self):
        self.invalidate_traversal_caches()
        self.invalidate_geometry_caches()

    def invalidate_traversal_caches(self):
        # speed up iterating the tree
        self._cached_items = {}
        self._cached_keys = {}
        self._cached_visible_items = {}
        self._cached_layer_items = {}
        self._cached_layer_keys = {}
        self._cached_key_groups = {}

        # cache available layers
        self._cached_layer_ids = None

    def invalidate_geometry_caches(self):
        # speed up hit testing
        self._cached_hit_rects = {}
        self._last_hit_args = None
        self._last_hit_key = None

    def fit_inside_canvas(self, canvas_border_rect):
        self._item.fit_inside_canvas(canvas_border_rect)

        # rects likely changed
        # -> invalidate geometry related caches
        self.invalidate_geometry_caches()

    def do_fit_inside_canvas(self, canvas_border_rect):
        self._item.do_fit_inside_canvas(canvas_border_rect)

        # rects likely changed
        # -> invalidate geometry related caches
        self.invalidate_geometry_caches()

    def set_visible_layers(self, layer_ids):
        """
        Show all items of layer "layer", hide all items of the other layers.
        """
        self.invalidate_caches()
        self._item.set_visible_layers(layer_ids)

    def set_item_visible(self, item, visible):
        if item.visible != visible:
            item.set_visible(visible)
            self.invalidate_caches()

    def iter_items(self):
        items = self._cached_items
        if not items:
            items = tuple(self._item.iter_items())
            self._cached_items = items
        return items

    def iter_keys(self, group_name = None):
        items = self._cached_keys.get(group_name)
        if not items:
            items = tuple(self._item.iter_keys(group_name))
            self._cached_keys[group_name] = items
        return items

    def iter_visible_items(self):
        items = self._cached_visible_items
        if not items:
            items = tuple(self._item.iter_visible_items())
            self._cached_visible_items = items
        return items

    def iter_layer_keys(self, layer_id):
        """
        Returns cached visible keys per layer, re-creates cache if necessary.
        Use iter_layer_keys if performance doesn't matter.
        """
        items = self._cached_layer_keys.get(layer_id)
        if not items:
            items = tuple(self._item.iter_layer_keys(layer_id))
            self._cached_layer_keys[layer_id] = items
        return items

    def iter_layer_items(self, layer_id = None, only_visible = True):
        args = (layer_id, only_visible)
        items = self._cached_layer_items.get(args)
        if not items:
            items = tuple(self._item.iter_layer_items(*args))
            self._cached_layer_items[args] = items
        return items

    def get_layer_ids(self):
        layer_ids = self._cached_layer_ids
        if not layer_ids:
            layer_ids = self._item.get_layer_ids()
            self._cached_layer_ids = layer_ids
        return layer_ids

    def get_key_groups(self):
        """
        Return all keys sorted by group.
        """
        key_groups = self._cached_key_groups
        if not key_groups:
            key_groups = self._item.get_key_groups()
            self._cached_key_groups = key_groups
        return key_groups

    def get_key_at(self, point, active_layer):
        """
        Find the topmost key at point.
        """
        # After motion-notify-event the query-tooltit event calls this
        # a second time with the same point. Avoid re-searching in that case.
        args = (point, active_layer)
        if self._last_hit_args == args:
            return self._last_hit_key

        key = None
        x, y = point
        hit_rects = self._get_hit_rects(active_layer)
        for x0, y0, x1, y1, k in hit_rects:
            # Inlined test, not using Rect.is_point_within for speed.
            if x >= x0 and x < x1 and \
               y >= y0 and y < y1:
                if k.geometry is None or \
                   k.get_hit_path().is_point_within(point):
                    key = k
                    break

        self._last_hit_args = args
        self._last_hit_key = key

        return key

    def _get_hit_rects(self, active_layer):
        try:
            hit_rects = self._cached_hit_rects[active_layer]
        except KeyError:
            # All visible and sensitive key items sorted in z-order.
            # Keys of the active layer have priority over non-layer keys
            # (layer switcher, hide, etc.).
            iter_layer_keys = self.iter_layer_keys
            items = list(reversed(list(iter_layer_keys(active_layer)))) + \
                    list(reversed(list(iter_layer_keys(None))))

            hit_rects = [item.get_hit_rect().to_extents() + (item,) \
                     for item in items]
            self._cached_hit_rects[active_layer] = hit_rects

        return hit_rects

    def init_chamfer_sizes(self):
        chamfer_sizes = self._calc_chamfer_sizes()
        for key in self.iter_global_keys():
            if key.chamfer_size is None:
                layer_id = key.get_layer()
                chamfer_size = chamfer_sizes.get(layer_id)
                if not chamfer_size is None:
                    key.chamfer_size = chamfer_size

    def _calc_chamfer_sizes(self):
        chamfer_sizes = {}
        for layer_id in [None] + self.get_layer_ids():
            # find the most frequent key width or height of the layer
            hist = {}
            for key in self.iter_layer_keys(layer_id):
                r = key.get_border_rect()
                s = min(r.w, r.h)
                hist[s] = hist.get(s, 0) + 1
            most_frequent_size = \
                max(list(zip(list(hist.values()), list(hist.keys()))))[1] \
                if hist else None
            chamfer_size = most_frequent_size * 0.5 \
                if not most_frequent_size is None else None
            chamfer_sizes[layer_id] = chamfer_size
        return chamfer_sizes


class LayoutItem(TreeItem):
    """ Abstract base class for layoutable items """

    # group string of the item, label size group for keys
    group = None

    # name of the layer the item is to be shown on, None for all layers
    layer_id = None

    # filename of the svg file where the key geometry is defined
    filename = None

    # key context for transformation between logical and canvas coordinates
    context = None

    # State of visibility. Also determines if drawing space will be
    # assigned to this item and its children.
    visible = True

    # sensitivity, aka. greying; False to stop interaction with the item
    sensitive = True

    # Border around the item. The border "shrinks" the item and
    # is invisible but still sensitive to clicks.
    border = 0.0

    # Expand item in LayoutBoxes
    # "True"  expands the item into the space of invisible siblings.
    # "False" keeps it at the size of the even distribution of all siblings.
    #         Usually this will lock the key to the aspect ratio of its
    #         svg geometry.
    expand = True

    # sublayout sub-trees
    sublayouts = None

    # parent item of sublayout roots
    sublayout_parent = None

    # override switching back to layer 0 on key press
    # True:  do switch to layer 0 on press
    # False: dont't
    # None:  maybe, hard-coded default-behavior for compatibility with <0.99
    unlatch_layer = None

    # False if the key should be ignored by the scanner
    scannable = True

    # Determines scanning order
    scan_priority = None

    # parsing helpers, only valid while loading a layout
    templates = None
    keysym_rules = None

    def __init__(self):
        self.context = KeyContext()

    def __repr__(self):
        return "{}({})".format(type(self).__name__, repr(self.id))

    def dumps(self):
        """
        Recursively dumps the layout (sub-) tree starting from self.
        Returns a multi-line string.
        """
        global _level
        if not "_level" in globals():
            _level = -1
        _level += 1
        s = "   "*_level + "{} id={} layer_id={} fn={} vis={}\n".format(
                                  object.__repr__(self),
                                  repr(self.id),
                                  repr(self.layer_id),
                                  repr(self.filename),
                                  repr(self.visible),
                                  ) + \
               "".join(item.dumps() for item in self.items)
        _level -= 1
        return s

    def set_id(self, id):
        self.id = id

    def get_rect(self):
        """ Get bounding box in logical coordinates """
        return self.get_border_rect().deflate(self.border)

    def get_border_rect(self):
        """ Get bounding rect including border in logical coordinates """
        return self.context.log_rect

    def set_border_rect(self, border_rect):
        """ Set bounding rect including border in logical coordinates """
        self.context.log_rect = border_rect

    def get_initial_border_rect(self):
        """
        Get initial bounding rect including border in logical coordinates
        """
        return self.context.initial_log_rect

    def set_initial_border_rect(self, border_rect):
        """
        Set initial bounding rect including border in logical coordinates.
        """
        self.context.initial_log_rect = border_rect

    def get_canvas_rect(self):
        """ Get bounding box in canvas coordinates """
        return self.context.log_to_canvas_rect(self.get_rect())

    def get_canvas_border_rect(self):
        """ Get bounding rect including border in canvas coordinates """
        return self.context.canvas_rect

    def get_log_aspect_ratio(self):
        """
        Return the aspect ratio of the visible logical extents
        of the layout tree.
        """
        size = self.get_log_extents()
        return size[0] / float(size[1])

    def get_log_extents(self):
        """
        Get the logical extents of the layout tree.
        Extents ignore invisible, "collapsed" items,
        ie. an invisible click column is not included.
        """
        return self.get_border_rect().get_size()

    def get_canvas_extents(self):
        """
        Get the canvas extents of the layout tree.
        """
        size = self.get_log_extents()
        return self.context.scale_log_to_canvas(size)

    def get_extra_render_size(self):
        """ Account for stroke width and antialiasing of keys and bars"""
        root = self.get_layout_root()
        return root.context.scale_log_to_canvas((2.0, 2.0))

    def fit_inside_canvas(self, canvas_border_rect):
        """
        Scale item and its children to fit inside the given canvas_rect.
        """
        # recursively update item's bounding boxes
        self.update_log_rect()

        # recursively fit inside canvas
        self.do_fit_inside_canvas(canvas_border_rect)

    def do_fit_inside_canvas(self, canvas_border_rect):
        """
        Scale item and its children to fit inside the given canvas_rect.
        """
        self.context.canvas_rect = canvas_border_rect

    def update_log_rect(self):
        for item in self.iter_depth_first():
            item._update_log_rect()

    def _update_log_rect(self):
        """
        Override this for layout items that have to calculate their
        logical rectangle.
        """
        pass

    def get_hit_rect(self):
        """ Returns true if the point lies within the items borders. """
        return self.get_canvas_border_rect().inflate(1)

    def is_point_within(self, canvas_point):
        """ Returns true if the point lies within the items borders. """
        rect = self.get_hit_rect()
        return rect.is_point_within(canvas_point)

    def set_visible(self, visible):
        self.visible = visible

    def is_visible(self):
        """ Returns visibility status """
        return self.visible

    def is_path_visible(self):
        """ Are all items in the path to the root visible? """
        item = self
        while item:
            if not item.visible:
                return False
            item = item.parent
        return True

    def has_visible_key(self):
        """
        Checks if there is any visible key in the
        subtree starting at self.
        """
        for item in self.iter_visible_items():
            if item.is_key():
                return True
        return False

    def is_path_scannable(self):
        """ Are all items in the path to the root scannable? """
        item = self
        while item:
            if not item.scannable:
                return False
            item = item.parent
        return True

    def get_path_scan_priority(self):
        """ Return the closeset scan_priority in the path to the root. """
        item = self
        while item:
            if not item.scan_priority is None:
                return item.scan_priority
            item = item.parent
        return 0

    def get_layout_root(self):
        """ Return the root layout item """
        item = self
        while item:
            if item.parent is None:
                return item
            item = item.parent

    def get_global_layout_root(self):
        """ Return the root layout item """
        item = self
        while item:
            if item.parent is None:
                return item
            item = item.parent

    def get_layer(self):
        """ Return the first layer_id on the path from the tree root to self """
        layer_id = None
        item = self
        while item:
            if not item.layer_id is None:
                layer_id = item.layer_id
            item = item.parent
        return layer_id

    def set_visible_layers(self, layer_ids):
        """
        Show all items of layers <layer_ids>, hide all items of the other layers.
        """
        if not self.layer_id is None:
            if not self.is_key():
                self.visible = self.layer_id in layer_ids

        for item in self.items:
            item.set_visible_layers(layer_ids)

    def get_layer_ids(self, _layer_ids=None):
        """
        Search the tree for layer ids and return them in order of appearance
        """
        if _layer_ids is None:
            _layer_ids = []

        if not self.layer_id is None and \
           not self.layer_id in _layer_ids:
            _layer_ids.append(self.layer_id)

        for item in self.items:
            item.get_layer_ids(_layer_ids)

        return _layer_ids

    def get_key_groups(self):
        """
        Traverse the tree and return all keys sorted by group.
        """
        key_groups = {}
        for key in self.iter_keys():
            keys = key_groups.get(key.group, [])
            keys.append(key)
            key_groups[key.group] = keys
        return key_groups

    def raise_to_top(self):
        """ raise self to the top of its siblings """
        if self.parent:
            self.parent.items.remove(self)
            self.parent.items.append(self)

    def lower_to_bottom(self):
        """ lower self to the bottom of its siblings """
        if self.parent:
            self.parent.items.remove(self)
            self.parent.items.insert(0, self)

    def raise_to_top(self):
        if self.parent:
            self.parent.items.remove(self)
            #self.parent.items.insert(0, self)
            self.parent.items.append(self)

    def get_filename(self):
        """
        Recursively finds the closeset definition of the svg filename.
        """
        if self.filename:
            return self.filename
        if self.parent:
            return self.parent.get_filename()
        return None

    def can_unlatch_layer(self):
        """
        Recursively finds the closeset definition of the
        unlatch_layer attribute.
        """
        if not self.unlatch_layer is None:
            return self.unlatch_layer
        if self.parent:
            return self.parent.can_unlatch_layer()
        return None

    def is_key(self):
        """ Returns true if self is a key. """
        return False

    def iter_visible_items(self):
        """
        Traverses top to bottom all visible layout items of the
        layout tree. Invisible paths are cut short.
        """
        if self.visible:

            yield self

            for item in self.items:
                for visible_item in item.iter_visible_items():
                    yield visible_item

    def iter_keys(self, group_name = None):
        """
        Iterates through all keys of the layout tree.
        """
        if self.is_key():
            if group_name is None or key.group == group_name:
                yield self

        for item in self.items:
            for key in item.iter_keys(group_name):
                yield key

    def iter_global_items(self):
        """
        Iterates through all items of the tree including sublayouts.
        """
        yield self

        for item in self.items:
            for child in item.iter_global_items():
                yield child

        if self.sublayouts:
            for item in self.sublayouts:
                for child in item.iter_global_items():
                    yield child

    def iter_global_keys(self, group_name = None):
        """
        Iterates through all keys of the layout tree including sublayouts.
        """
        if self.is_key():
            if group_name is None or key.group == group_name:
                yield self

        for item in self.items:
            for key in item.iter_global_keys(group_name):
                yield key

        if self.sublayouts:
            for item in self.sublayouts:
                for key in item.iter_global_keys(group_name):
                    yield key

    def iter_layer_keys(self, layer_id = None):
        """
        Iterates through all keys of the given layer.
        """
        for item in self.iter_layer_items(layer_id):
            if item.is_key():
                yield item

    def iter_layer_items(self, layer_id = None, only_visible = True,
                              _found_layer_id = None):
        """
        Iterate through all items of the given layer.
        The first layer definition found in the path to each key wins.
        layer=None iterates through all keys that don't have a layer
        specified anywhere in their path.
        """
        if only_visible and not self.visible:
            return

        if self.layer_id == layer_id:
            _found_layer_id = layer_id

        if self.layer_id and self.layer_id != _found_layer_id:
            return

        if _found_layer_id == layer_id:
            yield self

        for item in self.items:
            for item in item.iter_layer_items(layer_id, only_visible,
                                              _found_layer_id):
                yield item

    def find_instance_in_path(self, classinfo):
        """ Find an item of a certain type in the path from self to the root. """
        item = self
        while item:
            if isinstance(item, classinfo):
                return item
            item = item.parent
        return None

    def update_templates(self, templates):
        if templates:
            if self.templates is None:
                self.templates = templates
            else:
                self.templates.update(templates)

    def update_keysym_rules(self, keysym_rules):
        if keysym_rules:
            if self.keysym_rules is None:
                self.keysym_rules = keysym_rules
            else:
                self.keysym_rules.update(keysym_rules)

    def append_sublayout(self, sublayout):
        if sublayout:
            if self.sublayouts is None:
                self.sublayouts = []
            self.sublayouts.append(sublayout)

    def find_sublayout(self, id):
        """
        Look for a sublayout item upwards from self to the root.
        """
        for item in self.iter_to_root():
            sublayouts = item.sublayouts
            if sublayouts:
                for sublayout in sublayouts:
                    if sublayout.id == id:
                        return sublayout
        return None

    def iter_to_global_root(self):
        """
        Iterate through sublayouts all the way to the global layout root.
        LayoutLoader needs this to access key templates from inside of
        sublayouts.
        """
        item = self
        while item:
            yield item
            item = item.parent or item.sublayout_parent


class LayoutBox(LayoutItem):
    """
    Container for distributing items along a single horizontal or
    vertical axis. Items touch, but don't overlap.
    """

    # Spread out child items horizontally or vertically.
    horizontal = True

    # distance between items
    spacing = 1

    # Don't extend bounding box into invisibles
    compact = False

    def __init__(self, horizontal = True):
        super(LayoutBox, self).__init__()
        if self.horizontal != horizontal:
            self.horizontal = horizontal

    def _update_log_rect(self):
        self.context.log_rect = self._calc_bounds()

    def _calc_bounds(self):
        """
        Calculate the bounding rectangle over all items of this panel.
        Include invisible items to stretch the visible ones into their
        space too.
        """
        compact = self.compact
        bounds = None
        for item in self.items:
            if not compact or item.visible:
                rect = item.get_border_rect()
                if not rect.is_empty():
                    if bounds is None:
                        bounds = rect
                    else:
                        bounds = bounds.union(rect)

        if bounds is None:
            return Rect()
        return bounds

    def do_fit_inside_canvas(self, canvas_border_rect):
        """ Scale items to fit inside the given canvas_rect """

        LayoutItem.do_fit_inside_canvas(self, canvas_border_rect)

        axis = 0 if self.horizontal else 1
        items = self.items

        # get canvas rectangle without borders
        canvas_rect = self.get_canvas_rect()

        # Find the combined length of all items, including
        # invisible ones (logical coordinates).
        length = 0.0
        for i, item in enumerate(items):
            rect = item.get_border_rect()
            if not rect.is_empty():
                if i:
                    length += self.spacing
                length += rect[axis+2]

        # Find the stretch factor, that fills the available canvas space with
        # evenly distributed, all visible items.
        fully_visible_scale = canvas_rect[axis+2] / length \
                              if length else 1.0
        canvas_spacing = fully_visible_scale * self.spacing

        # Transform items into preliminary canvas space, drop invisibles
        # and find the total lengths of expandable and non-expandable
        # items (preliminary canvas coordinates).
        length_expandables = 0.0
        num_expandables = 0
        length_nonexpandables = 0.0
        num_nonexpandables = 0
        for i, item in enumerate(items):
            length = item.get_border_rect()[axis+2]
            if length and item.has_visible_key():
                length *= fully_visible_scale
                if item.expand:
                    length_expandables += length
                    num_expandables += 1
                else:
                    length_nonexpandables += length
                    num_nonexpandables += 1

        # Calculate a second stretch factor for expandable and actually
        # visible items. This takes care of the part of the canvas_rect,
        # that isn't covered by the first factor yet.
        # All calculation is done in preliminary canvas coordinates.
        length_target = canvas_rect[axis+2] - length_nonexpandables - \
                   canvas_spacing * (num_nonexpandables + num_expandables - 1)
        expandable_scale = length_target / length_expandables \
                           if length_expandables else 1.0

        # Calculate the final canvas rectangles and traverse
        # the tree recursively.
        position = 0.0
        for i, item in enumerate(items):
            rect = item.get_border_rect()
            if item.has_visible_key():
                length  = rect[axis+2]
                spacing = canvas_spacing
            else:
                length  = 0.0
                spacing = 0.0

            scale = fully_visible_scale
            if item.expand:
                scale *= expandable_scale
            canvas_length = length * scale

            # set the final canvas rect
            r = Rect(*canvas_rect)
            r[axis]   = canvas_rect[axis] + position
            r[axis+2] = canvas_length
            item.do_fit_inside_canvas(r)

            position += canvas_length + spacing

    def get_log_extents(self):
        """
        Get the logical extents of the layout tree.
        Extents ignore invisible, "collapsed" items,
        ie. an invisible click column is not included.
        """
        rect = None
        for item in self.items:
            r = item.get_border_rect()
            if rect is None:
                rect = r.copy()
            else:
                if self.horizontal:
                    rect.w += r.w
                else:
                    rect.h += r.h

        return rect.get_size()


class LayoutPanel(LayoutItem):
    """
    Group of keys layed out at fixed positions relative to each other.
    """

    # Don't extend bounding box into invisibles
    compact = False

    def do_fit_inside_canvas(self, canvas_border_rect):
        """
        Scale panel to fit inside the given canvas_rect.
        """
        LayoutItem.do_fit_inside_canvas(self, canvas_border_rect)

        # Setup children's transformations, take care of the border.
        if self.get_border_rect().is_empty():
            # Clear all item's transformations if there are no visible items.
            for item in self.items:
                item.context.canvas_rect = Rect()
        else:
            context = KeyContext()
            context.log_rect = self.get_border_rect()
            context.canvas_rect = self.get_canvas_rect() # exclude border

            for item in self.items:
                rect = context.log_to_canvas_rect(item.context.log_rect)
                item.do_fit_inside_canvas(rect)

    def _update_log_rect(self):
        self.context.log_rect = self._calc_bounds()

    def _calc_bounds(self):
        """ Calculate the bounding rectangle over all items of this panel """
        # If there is no visible item return an empty rect
        if all(not item.is_visible() for item in self.items):
            return Rect()

        compact = self.compact
        bounds = None
        for item in self.items:
            if not compact or item.visible:
                rect = item.get_border_rect()
                if not rect.is_empty():
                    if bounds is None:
                        bounds = rect
                    else:
                        bounds = bounds.union(rect)

        if bounds is None:
            return Rect()
        return bounds

