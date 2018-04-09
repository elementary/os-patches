#  i18n reusable helper class
#
#  Copyright (c) 2009 Canonical Ltd.
#
#  Author: Amichai Rothman <amichai2@amichais.net>
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

#
# This is a reusable helper component containing various i18n related
# utilities. It also helps narrow the gap between GTK and KDE frontends
# to a given application, by providing the translate_widget function to
# translate both language and GTK accelerators and styles to QT widgets.
# I hope to see this improve and grow into an abstract dual-gui helper
# framework, to make it much easier to port Ubuntu/GTK apps to Kubuntu
# as well as maintain them as they grow.
#

from gettext import gettext as _
import re

# FIXME: what's the point of these functions????
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *

def strip_html(str):
  regex = re.compile('<[^>]*>')
  return regex.sub('', str)

def translate_string(text, nohtml=False, accelerators='replace'):
  if not text:
    return text
  translated = _(str(text)) # text might be QString, unicode or str
  if nohtml:
    translated = strip_html(translated)
  if accelerators == 'replace':
    translated = translated.replace('_', '&') # convert accelerators
  elif accelerators == 'remove':
    translated = translated.replace('_', '') # convert accelerators
  return translated

def apply_styles(widget, str):
  # this is obviously just a start :-)
  if str.startswith("<b>"):
      widget.setStyleSheet("QGroupBox {font-weight:bold;}");

def translate_widget(widget, recursive=True):
  """ Translate a widget recursively, while converting GTK
      accelerators and styles to appropriate QT equivalents.
      The given widget is typically a dialog or main window,
      translated after its components are initialized but
      before being shown.
      """
  if isinstance(widget, QTabWidget):
    for i in range(0, widget.count()):
      widget.setTabText(i, translate_string(widget.tabText(i)))
  elif isinstance(widget, QGroupBox):
    apply_styles(widget, widget.title())
    widget.setTitle(translate_string(widget.title(), nohtml=True))
  elif isinstance(widget, QLineEdit):
    widget.setText(widget.text()) # user-editable text: utf8 without translation
  elif isinstance(widget, QWidget):
    widget.setWindowTitle(translate_string(widget.windowTitle(), accelerators='remove'))
    try:
      widget.setText(translate_string(widget.text()))
    except AttributeError:
      pass

  if recursive:
    for child in widget.children():
      translate_widget(child)
