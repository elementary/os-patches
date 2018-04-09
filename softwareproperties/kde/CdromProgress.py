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

import apt
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from gettext import gettext as _
from .I18nHelper import *

class CdromProgress(apt.progress.base.CdromProgress):
  def __init__(self, datadir, parent, kapp):
    self.dialog_cdrom_progress = QProgressDialog(parent.userinterface)
    self.button_cdrom_close = QPushButton("Close", None)
    self.dialog_cdrom_progress.setCancelButton(self.button_cdrom_close)
    self.dialog_cdrom_progress.show()
    self.parent = parent
    self.kapp = kapp
    self.button_cdrom_close.setEnabled(False)

  def close(self):
    self.dialog_cdrom_progress.close()

  def update(self, text, step):
    """ update is called regularly so that the gui can be redrawn """
    if step > 0:
      self.dialog_cdrom_progress.setValue((step/self.totalSteps)*100)
      if step == self.totalSteps:
        self.button_cdrom_close.setEnabled(True)
    if text != "":
      self.dialog_cdrom_progress.setLabelText(text)
    self.kapp.processEvents()

  def askCdromName(self):
    (name, valid) = QInputDialog.getText(self.parent.userinterface, _("CD Name"), _("Please enter a name for the disc"))
    return (valid, name)

  def changeCdrom(self):
    result = QMessageBox.question(self.parent.userinterface, _("Insert Disk"), _("Please insert a disk in the drive:"), QMessageBox.Ok|QMessageBox.Cancel)
    if result == QMessageBox.Ok:
      print("ok")
      return True
    print("cancel")
    return False
