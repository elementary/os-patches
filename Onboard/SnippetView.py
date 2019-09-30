# -*- coding: utf-8 -*-

from __future__ import division, print_function, unicode_literals

from gi.repository import GObject, Gtk


from Onboard.utils import show_error_dialog

### Config Singleton ###
from Onboard.Config import Config
config = Config()
########################

### Logging ###
import logging
_logger = logging.getLogger("SnippetView")
###############

DEFAULT_SNIPPET_LABEL = _("<Enter label>")
DEFAULT_SNIPPET_TEXT  = _("<Enter text>")

class SnippetView(Gtk.TreeView):

    def __init__(self):
        Gtk.TreeView.__init__(self)

        self.set_headers_visible(True)

        adj = Gtk.Adjustment(step_increment=1, upper=1000)
        number_renderer = Gtk.CellRendererSpin(editable=True, adjustment=adj)
        number_renderer.connect("edited", self.on_number_edited)
        number_column = Gtk.TreeViewColumn(title=_("Button Number"),
                                           cell_renderer=number_renderer,
                                           text=0)
        self.append_column(number_column)

        text_renderer = Gtk.CellRendererText(editable=True)
        text_renderer.connect("edited", self.on_label_edited)
        text_column = Gtk.TreeViewColumn(title=_("Button Label"),
                                         cell_renderer=text_renderer,
                                         text=1)
        text_column.set_expand(True)
        text_column.set_cell_data_func(text_renderer, self.label_data_func)
        self.append_column(text_column)

        text_renderer = Gtk.CellRendererText(editable=True)
        text_renderer.connect("edited", self.on_text_edited)
        text_column = Gtk.TreeViewColumn(title=_("Snippet Text"),
                                         cell_renderer=text_renderer,
                                         text=2)
        text_column.set_expand(True)
        text_column.set_cell_data_func(text_renderer, self.text_data_func)
        self.append_column(text_column)

        self.update()

        config.snippets_notify_add(self.on_snippets_changed)

    def on_snippets_changed(self, value):
        self.update()

    def update(self):
        last_number = self.get_selected_number()

        self.list_store = Gtk.ListStore(int, str, str)
        self.set_model(self.list_store)
        self.list_store.set_sort_column_id(0, Gtk.SortType.ASCENDING)

        for number, (label, text) in sorted(config.snippets.items()):
            it = self.list_store.append((number, label, text))

        # set and scroll to selection
        self.select_number(last_number)

    def label_data_func(self, treeviewcolumn, cell_renderer, model, iter, data):
        value = model.get_value(iter, 1)
        if not value:
            value = DEFAULT_SNIPPET_LABEL
        cell_renderer.set_property('text', value)

    def text_data_func(self, treeviewcolumn, cell_renderer, model, iter, data):
        value = model.get_value(iter, 2)
        if not value:
            value = DEFAULT_SNIPPET_TEXT
        cell_renderer.set_property('text', value)

    def on_number_edited(self, cell, path, new_text, user_data=None):
        number = self.get_selected_number()

        try:
            new_number = int(new_text)
        except ValueError:
            show_error_dialog(_("Must be an integer number"))
            return

        # same number?
        if new_number == number:
            return   # nothing to do

        # Make sure number not taken
        if new_number in config.snippets:
            show_error_dialog(_("Snippet %d is already in use.") % new_number)
            return

        label, text = config.snippets[number]
        config.del_snippet(number)
        config.set_snippet(new_number, (label, text))
        self.select_number(new_number)

    def on_label_edited(self, cell, path, new_value, user_data=None):
        number = self.get_selected_number()
        if not number is None:
            label, text = config.snippets[number]
            config.set_snippet(number, (new_value, text))

    def on_text_edited(self, cell, path, new_value, user_data=None):
        number = self.get_selected_number()
        if not number is None:
            label, text = config.snippets[number]
            config.set_snippet(number, (label, new_value))

    def append(self, label, text):
        # Find the largest button number
        numbers = list(config.snippets.keys())
        max_number = max(numbers) if numbers else -1
        new_number = max_number + 1
        config.set_snippet(new_number, (label, text))
        self.select_number(new_number)

    def remove_selected(self):
        remove_number = self.get_selected_number()
        if not remove_number is None:

            # find a new item to select
            select_number = None
            numbers = sorted(config.snippets.keys())
            for i, number in enumerate(numbers):
                if number == remove_number:
                    if i < len(numbers)-1:
                        select_number = numbers[i+1]
                    else:
                        select_number = numbers[i-1]
            self.select_number(select_number)

            # delete snippet
            config.del_snippet(remove_number)

    def get_selected_number(self):
        sel = self.get_selection()
        if sel:
            (model, iter) = sel.get_selected()
            if iter:
                return self.list_store.get_value(iter, 0)
        return None

    def select_number(self, number):
        model = self.get_model()
        iter = model.get_iter_first()
        while (iter):
            if number == model.get_value(iter, 0):
                self.get_selection().select_iter(iter)
                path = self.list_store.get_path(iter)
                self.scroll_to_cell(path)
                break
            iter = model.iter_next(iter)


