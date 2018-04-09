#  Qt 4 based frontend to software-properties
#
#  Copyright (c) 2007 Canonical Ltd.
#
#  Author: Jonathan Riddell <jriddell@ubuntu.com>
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
#  USA

from __future__ import absolute_import, print_function

from gettext import gettext as _

from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5 import uic

from aptsources.sourceslist import SourceEntry
from .I18nHelper import *

UIDIR = '/home/jr/src/software-properties/deneutral'

class DialogEdit(QDialog):

  def __init__(self, parent, sourceslist, source_entry, datadir):
    QDialog.__init__(self, parent)
    self.sourceslist = sourceslist
    self.source_entry = source_entry
    uic.loadUi("%s/designer/dialog_edit.ui" % datadir, self)

    self.combobox_type.addItem(_("Binary"))
    self.combobox_type.addItem(_("Source code"))
    if source_entry.type == "deb":
      self.combobox_type.setCurrentIndex(0)
    elif source_entry.type == "deb-src":
      self.combobox_type.setCurrentIndex(1)
    else:
      print("Error, unknown source type: '%s'" % source_entry.type)

    # uri
    self.entry_uri.setText(source_entry.uri)
    self.entry_dist.setText(source_entry.dist)

    comps = ""
    for c in source_entry.comps:
      if len(comps) > 0:
        comps = comps + " " + c
      else:
        comps = c
    self.entry_comps.setText(comps)

    self.entry_comment.setText(source_entry.comment)

    translate_widget(self)

    self.entry_uri.textChanged.connect(self.check_line)
    self.entry_dist.textChanged.connect(self.check_line)
    self.entry_comps.textChanged.connect(self.check_line)
    self.entry_comment.textChanged.connect(self.check_line)

  def check_line(self, text):
    """Check for a valid apt line and set the enabled value of the
       button 'add' accordingly"""
    line = self.get_line()
    self.button_edit_ok = self.buttonBox.button(QDialogButtonBox.Ok)
    if line == False:
      self.button_edit_ok.setEnabled(False)
      return
    source_entry = SourceEntry(line)
    if source_entry.invalid == True:
      self.button_edit_ok.setEnabled(False)
    else:
      self.button_edit_ok.setEnabled(True)

  def get_line(self):
    """Collect all values from the entries and create an apt line"""
    if self.source_entry.disabled == True:
        line = "#"
    else:
        line = ""

    if self.combobox_type.currentIndex() == 0:
      line = line + "deb"
    else:
      line = line + "deb-src"

    text = self.entry_uri.text()
    if len(text) < 1 or text.find(" ") != -1 or text.find("#") != -1:
      return False
    line = line + " " + text

    text = self.entry_dist.text()
    if len(text) < 1 or text.find(" ") != -1 or text.find("#") != -1:
      return False
    line = line + " " + text

    text = self.entry_comps.text()
    if text.find("#") != -1:
      return False
    elif text != "":
      line = line + " " + text

    text = self.entry_comment.text()
    if text != "":
      line = line + " #" + text + "\n"
    else:
      line = line + "\n"
    return line

  def run(self):
      result = self.exec_()
      if result == QDialog.Accepted:
        line = self.get_line()

        # change repository
        index = self.sourceslist.list.index(self.source_entry)
        file = self.sourceslist.list[index].file
        self.sourceslist.list[index] = SourceEntry(line,file)
      return result
