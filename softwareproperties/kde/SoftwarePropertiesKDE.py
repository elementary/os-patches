# -*- coding: utf-8 -*-
#
#  Qt 4 based frontend to software-properties
#
#  Copyright © 2009 Harald Sitter <apachelogger@ubuntu.com>
#  Copyright © 2007 Canonical Ltd.
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

# FIXME: why does this pass kapps around?????

from __future__ import absolute_import, print_function

import apt_pkg
import tempfile
from gettext import gettext as _
import os
import subprocess

from PyQt5 import uic
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *

import softwareproperties
import softwareproperties.distro
from softwareproperties.SoftwareProperties import SoftwareProperties
import softwareproperties.SoftwareProperties
from .I18nHelper import *

from aptsources.sourceslist import SourceEntry
from .DialogEdit import DialogEdit
from .DialogAdd import DialogAdd
from .DialogMirror import DialogMirror
from .CdromProgress import CdromProgress

class SoftwarePropertiesKDEUI(QWidget):
  def __init__(self, datadir):
      QWidget.__init__(self)
      uic.loadUi("%s/designer/main.ui" % datadir, self)

class SoftwarePropertiesKDE(SoftwareProperties):
  def __init__(self, datadir=None, options=None, parent=None, file=None, attachWinID=None):
    """ Provide a KDE based graphical user interface to configure
        the used software repositories, corresponding authentication keys
        and update automation """
    SoftwareProperties.__init__(self, options=options, datadir=datadir)

    self.options = options
    self.datadir = datadir

    global kapp
    kapp = QApplication.instance()
    kapp.setWindowIcon(QIcon.fromTheme("applications-other"))

    self.userinterface = SoftwarePropertiesKDEUI(datadir)
    self.userinterface.setWindowIcon(QIcon.fromTheme("applications-other"))
    self.userinterface.button_auth_restore.setIcon(QIcon.fromTheme("edit-undo"))
    self.userinterface.button_add_auth.setIcon(QIcon.fromTheme("list-add"))
    self.userinterface.button_auth_remove.setIcon(QIcon.fromTheme("list-remove"))
    self.userinterface.button_remove.setIcon(QIcon.fromTheme("list-remove"))
    self.userinterface.button_edit.setIcon(QIcon.fromTheme("document-edit"))
    self.userinterface.button_add.setIcon(QIcon.fromTheme("list-add"))
    self.userinterface.button_add_cdrom.setIcon(QIcon.fromTheme("media-optical"))
    translate_widget(self.userinterface)
    self.userinterface.show()
    # FIXME: winid not handled
    #if attachWinID is not None:
        #KWindowSystem.setMainWindow(self.userinterface, int(attachWinID))

    # rejected() signal from Close button
    self.userinterface.buttonBox.rejected.connect(self.on_close_button)

    self.userinterface.buttonBox.button(QDialogButtonBox.Reset).setEnabled(False)

    # Put some life into the user interface:
    self.init_server_chooser()
    self.init_popcon()
    self.init_auto_update()
    self.init_release_upgrades()
    self.show_auto_update_level()
    # Setup the key list
    self.init_keys()
    self.show_keys()
    # Setup the ISV sources list
    self.init_isv_sources()
    self.show_isv_sources()
    self.show_cdrom_sources()

    self.userinterface.checkbutton_source_code.clicked.connect(self.on_checkbutton_source_code_toggled)
    self.userinterface.button_auth_restore.clicked.connect(self.on_restore_clicked)
    self.userinterface.button_add_auth.clicked.connect(self.add_key_clicked)
    self.userinterface.button_auth_remove.clicked.connect(self.remove_key_clicked)
    self.userinterface.checkbutton_popcon.toggled.connect(self.on_checkbutton_popcon_toggled)
    self.userinterface.checkbutton_auto_update.toggled.connect(self.on_auto_update_toggled)
    self.userinterface.combobox_update_interval.currentIndexChanged.connect(self.on_combobox_update_interval_changed)
    self.userinterface.button_remove.clicked.connect(self.on_remove_clicked)
    self.userinterface.button_edit.clicked.connect(self.on_edit_clicked)
    self.userinterface.button_add_cdrom.clicked.connect(self.on_button_add_cdrom_clicked)
    self.userinterface.button_add.clicked.connect(self.on_add_clicked)
    self.userinterface.treeview_sources.itemChanged.connect(self.on_isv_source_toggled)
    self.userinterface.treeview_sources.itemClicked.connect(self.on_treeview_sources_cursor_changed)
    self.userinterface.treeview_cdroms.itemChanged.connect(self.on_cdrom_source_toggled)
    self.userinterface.treeview2.itemClicked.connect(self.on_treeview_keys_cursor_changed)
    button_close = self.userinterface.buttonBox.button(QDialogButtonBox.Close)
    button_close.setIcon(QIcon.fromTheme("dialog-close"))
    button_revert = self.userinterface.buttonBox.button(QDialogButtonBox.Reset)
    button_revert.setIcon(QIcon.fromTheme("edit-undo"))
    button_revert.clicked.connect(self.on_button_revert_clicked)

    self.init_distro()
    self.show_distro()

  def init_popcon(self):
    """ If popcon is enabled show the statistics tab and an explanation
        corresponding to the used distro """
    is_helpful = self.get_popcon_participation()
    if is_helpful != None:
      text = softwareproperties.distro.get_popcon_description(self.distro)
      text = text.replace("\n", "<br />") #silly GTK mixes HTML and normal white space
      self.userinterface.label_popcon_desc.setText(text)
      self.userinterface.checkbutton_popcon.setChecked(is_helpful)
    else:
      self.userinterface.tabWidget.removeTab(4)

  def init_server_chooser(self):
    """ Set up the widgets that allow to choose an alternate download site """
    # nothing to do here, set up signal in show_distro()
    pass

  def init_release_upgrades(self):
    " setup the widgets that allow configuring the release upgrades "
    i = self.get_release_upgrades_policy()

    self.userinterface.combobox_release_upgrades.setCurrentIndex(i)
    self.userinterface.combobox_release_upgrades.currentIndexChanged.connect(self.on_combobox_release_upgrades_changed)

  def init_auto_update(self):
    """ Set up the widgets that allow to configure the update automation """
    # this maps the key (combo-box-index) to the auto-update-interval value
    # where (-1) means, no key
    self.combobox_interval_mapping = { 0 : 1,
                                       1 : 2,
                                       2 : 7,
                                       3 : 14 }

    self.userinterface.combobox_update_interval.insertItem(99, _("Daily"))
    self.userinterface.combobox_update_interval.insertItem(99, _("Every two days"))
    self.userinterface.combobox_update_interval.insertItem(99, _("Weekly"))
    self.userinterface.combobox_update_interval.insertItem(99, _("Every two weeks"))

    update_days = self.get_update_interval()

    # If a custom period is defined add a corresponding entry
    if not update_days in self.combobox_interval_mapping.values():
        if update_days > 0:
            self.userinterface.combobox_update_interval.insertItem(99, _("Every %s days") % update_days)
            self.combobox_interval_mapping[4] = update_days

    for key in self.combobox_interval_mapping:
      if self.combobox_interval_mapping[key] == update_days:
        self.userinterface.combobox_update_interval.setCurrentIndex(key)
        break

    if update_days >= 1:
      self.userinterface.checkbutton_auto_update.setChecked(True)
      self.userinterface.combobox_update_interval.setEnabled(True)
      self.userinterface.radiobutton_updates_inst_sec.setEnabled(True)
      self.userinterface.radiobutton_updates_download.setEnabled(True)
      self.userinterface.radiobutton_updates_notify.setEnabled(True)
    else:
      self.userinterface.checkbutton_auto_update.setChecked(False)
      self.userinterface.combobox_update_interval.setEnabled(False)
      self.userinterface.radiobutton_updates_inst_sec.setEnabled(False)
      self.userinterface.radiobutton_updates_download.setEnabled(False)
      self.userinterface.radiobutton_updates_notify.setEnabled(False)

    self.userinterface.radiobutton_updates_download.toggled.connect(self.set_update_automation_level)
    self.userinterface.radiobutton_updates_inst_sec.toggled.connect(self.set_update_automation_level)
    self.userinterface.radiobutton_updates_notify.toggled.connect(self.set_update_automation_level)

  def show_auto_update_level(self):
    """Represent the level of update automation in the user interface"""
    level = self.get_update_automation_level()
    if level == None:
        self.userinterface.radiobutton_updates_inst_sec.setChecked(False)
        self.userinterface.radiobutton_updates_download.setChecked(False)
        self.userinterface.radiobutton_updates_notify.setChecked(False)
    if level == softwareproperties.UPDATE_MANUAL or\
       level == softwareproperties.UPDATE_NOTIFY:
        self.userinterface.radiobutton_updates_notify.setChecked(True)
    elif level == softwareproperties.UPDATE_DOWNLOAD:
        self.userinterface.radiobutton_updates_download.setChecked(True)
    elif level == softwareproperties.UPDATE_INST_SEC:
        self.userinterface.radiobutton_updates_inst_sec.setChecked(True)

  def init_distro(self):
    # TRANS: %s stands for the distribution name e.g. Debian or Ubuntu
    text = _("%s updates") % self.distro.id
    text = text.replace("Ubuntu", "Kubuntu")
    self.userinterface.groupBox_updates.setTitle(text)
    # TRANS: %s stands for the distribution name e.g. Debian or Ubuntu
    text = _("%s Software") % self.distro.id
    text = text.replace("Ubuntu", "Kubuntu")
    self.userinterface.tabWidget.setTabText(0, text)

    # Setup the checkbuttons for the components
    for child in self.userinterface.vbox_dist_comps_frame.children():
        if isinstance(child, QGridLayout):
            self.vbox_dist_comps = child
        elif isinstance(child, QObject):
            print("passing")
        else:
            print("child: " + str(type(child)))
            child.hide()
            del child
    self.checkboxComps = {}
    for comp in self.distro.source_template.components:
        # TRANSLATORS: Label for the components in the Internet section
        #              first %s is the description of the component
        #              second %s is the code name of the comp, eg main, universe
        label = _("%s (%s)") % (comp.get_description(), comp.name)
        checkbox = QCheckBox(label, self.userinterface.vbox_dist_comps_frame)
        checkbox.setObjectName(comp.name)
        self.checkboxComps[checkbox] = comp

        # setup the callback and show the checkbutton
        checkbox.clicked.connect(self.on_component_toggled)
        self.vbox_dist_comps.addWidget(checkbox)

    # Setup the checkbuttons for the child repos / updates
    for child in self.userinterface.vbox_updates_frame.children():
        if isinstance(child, QGridLayout):
            self.vbox_updates = child
        else:
            del child
    if len(self.distro.source_template.children) < 1:
        self.userinterface.vbox_updates_frame.hide()
    self.checkboxTemplates = {}
    for template in self.distro.source_template.children:
        # Do not show source entries in there
        if template.type == "deb-src":
            continue

        checkbox = QCheckBox(template.description, 
                             self.userinterface.vbox_updates_frame)
        checkbox.setObjectName(template.name)
        self.checkboxTemplates[checkbox] = template
        # setup the callback and show the checkbutton
        checkbox.clicked.connect(self.on_checkbutton_child_toggled)
        self.vbox_updates.addWidget(checkbox)

    # If no components are enabled there will be no need for updates
    if len(self.distro.enabled_comps) < 1:
        self.userinterface.vbox_updates_frame.setEnabled(False)
    else:
        self.userinterface.vbox_updates_frame.setEnabled(True)

    self.init_mirrors()
    self.userinterface.combobox_server.activated.connect(self.on_combobox_server_changed)

  def init_mirrors(self):
    # Intiate the combobox which allows the user to specify a server for all
    # distro related sources
    seen_server_new = []
    self.mirror_urls = []
    self.userinterface.combobox_server.clear()
    for (name, uri, active) in self.distro.get_server_list():
        ##server_store.append([name, uri, False])
        self.userinterface.combobox_server.addItem(name)
        self.mirror_urls.append(uri)
        if [name, uri] in self.seen_server:
            self.seen_server.remove([name, uri])
        elif uri != None:
            seen_server_new.append([name, uri])
        if active == True:
            self.active_server = self.userinterface.combobox_server.count() - 1
            self.userinterface.combobox_server.setCurrentIndex(self.userinterface.combobox_server.count() - 1)
    for [name, uri] in self.seen_server:
        self.userinterface.combobox_server.addItem(name)
        self.mirror_urls.append(uri)
    self.seen_server = seen_server_new
    # add a separator and the option to choose another mirror from the list
    ##FIXME server_store.append(["sep", None, True])
    self.userinterface.combobox_server.addItem(_("Other..."))
    self.other_mirrors_index = self.userinterface.combobox_server.count() - 1

  def show_distro(self):
    """
    Represent the distro information in the user interface
    """
    # Enable or disable the child source checkbuttons
    for child in self.userinterface.vbox_updates_frame.children():
        if isinstance(child, QCheckBox):
            template = self.checkboxTemplates[child]
            (active, inconsistent) = self.get_comp_child_state(template)
            if inconsistent:
                child.setCheckState(Qt.PartiallyChecked)
            elif active:
                child.setCheckState(Qt.Checked)
            else:
                child.setCheckState(Qt.Unchecked)

    # Enable or disable the component checkbuttons
    for child in self.userinterface.vbox_dist_comps_frame.children():
        if isinstance(child, QCheckBox):
            template = self.checkboxComps[child]
            (active, inconsistent) = self.get_comp_download_state(template)
            if inconsistent:
                child.setCheckState(Qt.PartiallyChecked)
            elif active:
                child.setCheckState(Qt.Checked)
            else:
                child.setCheckState(Qt.Unchecked)

    # If no components are enabled there will be no need for updates
    # and source code
    if len(self.distro.enabled_comps) < 1:
        self.userinterface.vbox_updates_frame.setEnabled(False)
        self.userinterface.checkbutton_source_code.setEnabled(False)
    else:
        self.userinterface.vbox_updates_frame.setEnabled(True)
        self.userinterface.checkbutton_source_code.setEnabled(True)

    # Check for source code sources
    source_code_state = self.get_source_code_state()
    if source_code_state == True:
        self.userinterface.checkbutton_source_code.setCheckState(Qt.Checked)
    elif source_code_state == None:
        self.userinterface.checkbutton_source_code.setCheckState(Qt.PartiallyChecked)
    else:
        self.userinterface.checkbutton_source_code.setCheckState(Qt.Unchecked)

    # Will show a short explanation if no CDROMs are used
    if len(self.get_cdrom_sources()) == 0:
        self.userinterface.treeview_cdroms.hide()
        self.userinterface.textview_no_cd.show()
        self.userinterface.groupBox_cdrom.hide()
    else:
        self.userinterface.treeview_cdroms.show()
        self.userinterface.textview_no_cd.hide()
        self.userinterface.groupBox_cdrom.show()

    # Output a lot of debug stuff
    if self.options.debug == True or self.options.massive_debug == True:
        print("ENABLED COMPS: %s" % self.distro.enabled_comps)
        print("INTERNET COMPS: %s" % self.distro.download_comps)
        print("MAIN SOURCES")
        for source in self.distro.main_sources:
            self.print_source_entry(source)
        print("CHILD SOURCES")
        for source in self.distro.child_sources:
            self.print_source_entry(source)
        print("CDROM SOURCES")
        for source in self.distro.cdrom_sources:
            self.print_source_entry(source)
        print("SOURCE CODE SOURCES")
        for source in self.distro.source_code_sources:
            self.print_source_entry(source)
        print("DISABLED SOURCES")
        for source in self.distro.disabled_sources:
            self.print_source_entry(source)
        print("ISV")
        for source in self.sourceslist_visible:
            self.print_source_entry(source)

  def set_update_automation_level(self, selected):
    """Call the backend to set the update automation level to the given 
       value"""
    if self.userinterface.radiobutton_updates_download.isChecked():
        SoftwareProperties.set_update_automation_level(self, softwareproperties.UPDATE_DOWNLOAD)
    elif self.userinterface.radiobutton_updates_inst_sec.isChecked():
        SoftwareProperties.set_update_automation_level(self, softwareproperties.UPDATE_INST_SEC)
    elif self.userinterface.radiobutton_updates_notify.isChecked():
        SoftwareProperties.set_update_automation_level(self, softwareproperties.UPDATE_NOTIFY)
    self.set_modified_config()

  def on_combobox_release_upgrades_changed(self, combobox):
    """ set the release upgrades policy """
    i = self.userinterface.combobox_release_upgrades.currentIndex()

    self.set_release_upgrades_policy(i)

  def on_combobox_server_changed(self, combobox):
    """
    Replace the servers used by the main and update sources with
    the selected one
    """
    combobox = self.userinterface.combobox_server
    index = combobox.currentIndex()
    if index == self.active_server:
        return
    elif index == self.other_mirrors_index:
        dialogue = DialogMirror(self.userinterface, self.datadir, self.distro, self.custom_mirrors)
        res = dialogue.run()
        if res != None:
            self.distro.change_server(res)
            self.set_modified_sourceslist()
            self.init_mirrors() # update combobox
        else:
            combobox.setCurrentIndex(self.active_server)
    else:
        uri = self.mirror_urls[index]
        if uri != None and len(self.distro.used_servers) > 0:
            self.active_server = index
            self.distro.change_server(uri)
            self.set_modified_sourceslist()
        else:
            self.distro.default_server = uri

  def on_component_toggled(self):
    """
    Sync the components of all main sources (excluding cdroms),
    child sources and source code sources
    """
    # FIXME: I find it rather questionable whether sender will work with pyqt doing weird signal handling
    tickBox = kapp.sender()
    if tickBox.checkState() == Qt.Checked:
        self.enable_component(str(tickBox.objectName()))
    elif tickBox.checkState() == Qt.Unchecked:
        self.disable_component(str(tickBox.objectName()))
    self.set_modified_sourceslist()

    # no way to set back to mixed state so turn off tristate
    tickBox.setTristate(False)

  def on_checkbutton_child_toggled(self):
    """
    Enable or disable a child repo of the distribution main repository
    """
    tickBox = kapp.sender()
    name = str(tickBox.objectName())

    if tickBox.checkState() == Qt.Checked:
        self.enable_child_source(name)
    elif tickBox.checkState() == Qt.Unchecked:
        self.disable_child_source(name)
    # no way to set back to mixed state so turn off tristate
    tickBox.setTristate(False)

  def on_checkbutton_source_code_toggled(self):
    """ Disable or enable the source code for all sources """
    if self.userinterface.checkbutton_source_code.checkState() == Qt.Checked:
        self.enable_source_code_sources()
    elif self.userinterface.checkbutton_source_code.checkState() == Qt.Unchecked:
        self.disable_source_code_sources()
    self.userinterface.checkbutton_source_code.setTristate(False)

  def on_checkbutton_popcon_toggled(self, state):
    """ The user clicked on the popcon paritipcation button """
    self.set_popcon_pariticipation(state)

  def init_isv_sources(self):
    """Read all valid sources into our ListStore"""
    """##FIXME
    # drag and drop support for sources.list
    self.treeview_sources.drag_dest_set(gtk.DEST_DEFAULT_ALL, \
                                        [('text/uri-list',0, 0)], \
                                        gtk.gdk.ACTION_COPY)
    self.treeview_sources.connect("drag_data_received",\
                                  self.on_sources_drag_data_received)
    """

  def on_isv_source_activate(self, treeview, path, column):
    """Open the edit dialog if a channel was double clicked"""
    ##FIXME TODO
    ##self.on_edit_clicked(treeview)

  def on_treeview_sources_cursor_changed(self, item, column):
    """set the sensitiveness of the edit and remove button
       corresponding to the selected channel"""
    # allow to remove the selected channel
    self.userinterface.button_remove.setEnabled(True)
    # disable editing of cdrom sources
    index = self.userinterface.treeview_sources.indexOfTopLevelItem(item)
    source_entry = self.isv_sources[index]
    if source_entry.uri.startswith("cdrom:"):
        self.userinterface.button_edit.setEnabled(False)
    else:
        self.userinterface.button_edit.setEnabled(True)

  def on_cdrom_source_toggled(self, item):
    """Enable or disable the selected channel"""
    self.toggle_source(self.cdrom_sources, self.userinterface.treeview_cdroms, item)

  def on_isv_source_toggled(self, item):
    """Enable or disable the selected channel"""
    self.toggle_source(self.isv_sources, self.userinterface.treeview_sources, item)

  def toggle_source(self, sources, treeview, item):
    """Enable or disable the selected channel"""
    index = treeview.indexOfTopLevelItem(item)
    source_entry = sources[index]
    self.toggle_source_use(source_entry)
    item = treeview.topLevelItem(index) # old item was destroyed
    if item != 0:
      treeview.setCurrentItem(item) # reselect entry after refresh
    else:
      self.userinterface.button_remove.setEnabled(False)
      self.userinterface.button_edit.setEnabled(False)

  def init_keys(self):
    """Setup the user interface parts needed for the key handling"""
    self.userinterface.treeview2.setColumnCount(1)

  def on_treeview_keys_cursor_changed(self, item, column):
    """set the sensitiveness of the edit and remove button
       corresponding to the selected channel"""
    # allow to remove the selected channel
    self.userinterface.button_auth_remove.setEnabled(True)
    # disable editing of cdrom sources
    index = self.userinterface.treeview2.indexOfTopLevelItem(item)

  #FIXME revert automation settings too
  def on_button_revert_clicked(self):
    """Restore the source list from the startup of the dialog"""
    SoftwareProperties.revert(self)
    self.set_modified_sourceslist()
    self.show_auto_update_level()
    self.userinterface.buttonBox.button(QDialogButtonBox.Reset).setEnabled(False)
    self.modified_sourceslist = False

  def set_modified_config(self):
    """The config was changed and now needs to be saved and reloaded"""
    SoftwareProperties.set_modified_config(self)
    self.userinterface.buttonBox.button(QDialogButtonBox.Reset).setEnabled(True)

  def set_modified_sourceslist(self):
    """The sources list was changed and now needs to be saved and reloaded"""
    SoftwareProperties.set_modified_sourceslist(self)
    self.show_distro()
    self.show_isv_sources()
    self.show_cdrom_sources()
    self.userinterface.buttonBox.button(QDialogButtonBox.Reset).setEnabled(True)

  def show_isv_sources(self):
    """ Show the repositories of independent software vendors in the
        third-party software tree view """
    self.isv_sources = self.get_isv_sources()[:]
    self.show_sources(self.isv_sources, self.userinterface.treeview_sources)

    if not self.isv_sources or len(self.isv_sources) < 1:
        self.userinterface.button_remove.setEnabled(False)
        self.userinterface.button_edit.setEnabled(False)

  def show_cdrom_sources(self):
    """ Show CD-ROM/DVD based repositories of the currently used distro in
        the CDROM based sources list """
    self.cdrom_sources = self.get_cdrom_sources()[:]
    self.show_sources(self.cdrom_sources, self.userinterface.treeview_cdroms)

  def show_sources(self, sources, treeview):
    # this workaround purposely replaces treeview.clear() (LP #102792)
    while treeview.topLevelItemCount() > 0:
      treeview.takeTopLevelItem(0)

    items = []
    for source in sources:
        contents = self.render_source(source)
        contents = contents.replace("<b>", "")
        contents = contents.replace("</b>", "")
        item = QTreeWidgetItem([contents])
        if not source.disabled:
            item.setCheckState(0, Qt.Checked)
        else:
            item.setCheckState(0, Qt.Unchecked)
        items.append(item)
    treeview.addTopLevelItems(items)

  def show_keys(self):
    self.userinterface.treeview2.clear()
    for key in self.apt_key.list():
      self.userinterface.treeview2.addTopLevelItem(QTreeWidgetItem([key]))

    if self.userinterface.treeview_sources.topLevelItemCount() < 1:
      self.userinterface.button_auth_remove.setEnabled(False)

  def on_combobox_update_interval_changed(self, index):
    #FIXME! move to backend
    if index != -1:
        value = self.combobox_interval_mapping[index]
        # Only write the key if it has changed
        if not value == apt_pkg.config.find_i(softwareproperties.CONF_MAP["autoupdate"]):
            apt_pkg.config.set(softwareproperties.CONF_MAP["autoupdate"], str(value))
            self.write_config()

  def on_auto_update_toggled(self):
    """Enable or disable automatic updates and modify the user interface
       accordingly"""
    if self.userinterface.checkbutton_auto_update.checkState() == Qt.Checked:
      self.userinterface.combobox_update_interval.setEnabled(True)
      self.userinterface.radiobutton_updates_inst_sec.setEnabled(True)
      self.userinterface.radiobutton_updates_download.setEnabled(True)
      self.userinterface.radiobutton_updates_notify.setEnabled(True)
      # if no frequency was specified use daily
      i = self.userinterface.combobox_update_interval.currentIndex()
      if i == -1:
          i = 0
          self.userinterface.combobox_update_interval.setCurrentIndex(i)
      value = self.combobox_interval_mapping[i]
    else:
      self.userinterface.combobox_update_interval.setEnabled(False)
      self.userinterface.radiobutton_updates_inst_sec.setEnabled(False)
      self.userinterface.radiobutton_updates_download.setEnabled(False)
      self.userinterface.radiobutton_updates_notify.setEnabled(False)
      SoftwareProperties.set_update_automation_level(self, None)
      value = 0
    self.set_update_interval(str(value))

  def on_add_clicked(self):
    """Show a dialog that allows to enter the apt line of a to be used repo"""
    dialog = DialogAdd(self.userinterface, self.sourceslist,
                       self.datadir, self.distro)
    line = dialog.run()
    if line != None:
      self.add_source_from_line(line)
      self.set_modified_sourceslist()

  def on_edit_clicked(self):
    """Show a dialog to edit an ISV source"""
    item = self.userinterface.treeview_sources.currentItem()
    if item is not None:
        index = self.userinterface.treeview_sources.indexOfTopLevelItem(item)
        dialogue = DialogEdit(self.userinterface, self.sourceslist, self.isv_sources[index], self.datadir)
        result = dialogue.run()
        if result == QDialog.Accepted:
            self.set_modified_sourceslist()
            self.show_isv_sources()

  # FIXME:outstanding from merge
  def on_isv_source_activated(self, treeview, path, column):
     """Open the edit dialog if a channel was double clicked"""
     ##FIXME TODO
     # check if the channel can be edited
     if self.button_edit.get_property("sensitive") == True:
         self.on_edit_clicked(treeview)

  def on_remove_clicked(self):
    """Remove the selected source"""
    item = self.userinterface.treeview_sources.currentItem()
    if item is not None:
        index = self.userinterface.treeview_sources.indexOfTopLevelItem(item)
        self.remove_source(self.isv_sources[index])
        self.show_isv_sources()

  def add_key_clicked(self):
    """Provide a file chooser that allows to add the gnupg of a trusted
       software vendor"""
    home = QDir.homePath()
    if "SUDO_USER" in os.environ:
        home = os.path.expanduser("~%s" % os.environ["SUDO_USER"])
    url = KUrl.fromPath(home)
    filename = KFileDialog.getOpenFileName(url, 'application/pgp-keys', self.userinterface, _("Import key"))
    if filename:
      if not self.add_key(filename):
        title = _("Error importing selected file")
        text = _("The selected file may not be a GPG key file " \
                "or it might be corrupt.")
        #KMessageBox.sorry(self.userinterface, text, title)
        QMessageBox.warning(self.userinterface, title, text)
      self.show_keys()

  def remove_key_clicked(self):
    """Remove a trusted software vendor key"""
    item = self.userinterface.treeview2.currentItem()
    if item == None:
        return
    key = item.text(0)
    if not self.remove_key(key[:8]):
      title = _("Error removing the key")
      text = _("The key you selected could not be removed. "
               "Please report this as a bug.")
      #KMessageBox.sorry(self.userinterface, text, title)
      QMessageBox.warning(self.userinterface, title, text)
    self.show_keys()

  def on_restore_clicked(self):
    """Restore the original keys"""
    self.apt_key.update()
    self.show_keys()

  def on_close_button(self):
    """Show a dialog that a reload of the channel information is required
       only if there is no parent defined"""
    if self.modified_sourceslist == True and self.options.no_update == False:
        messageBox = QMessageBox(self.userinterface)
        messageBox.setIcon(QMessageBox.Information)
        reloadButton = messageBox.addButton(_("Reload"), QMessageBox.AcceptRole)
        messageBox.addButton(QMessageBox.Close)
        primary = _("Your local copy of the software catalog is out of date.")
        secondary = _("A new copy will be downloaded.")
        text = "%s<br /><br /><small>%s</small>" % (primary, secondary)
        messageBox.setText(text)
        messageBox.exec_()
        if (messageBox.clickedButton() == reloadButton):
                cmd = ["/usr/bin/qapt-batch", "--update"]
                subprocess.call(cmd)
    kapp.quit()

  def on_button_add_cdrom_clicked(self):
    '''Show a dialog that allows to add a repository located on a CDROM
       or DVD'''
    # testing
    #apt_pkg.config.set("APT::CDROM::Rename","true")

    saved_entry = apt_pkg.config.find("Dir::Etc::sourcelist")
    tmp = tempfile.NamedTemporaryFile()
    apt_pkg.config.set("Dir::Etc::sourcelist",tmp.name)
    progress = CdromProgress(self.datadir, self, kapp)
    cdrom = apt_pkg.Cdrom()
    # if nothing was found just return
    try:
      res = cdrom.add(progress)
    except SystemError as msg:
      title = _("CD Error")
      primary = _("Error scanning the CD")
      text = "%s\n\n<small>%s</small>" % (primary, msg)
      #KMessageBox.sorry(self.userinterface, text, title)
      QMessageBox.warning(self.userinterface, title, text)
      return
    finally:
      apt_pkg.config.set("Dir::Etc::sourcelist",saved_entry)
      progress.close()

    if res == False:
      return
    # read tmp file with source name (read only last line)
    line = ""
    for x in open(tmp.name):
      line = x
    if line != "":
      full_path = "%s%s" % (apt_pkg.config.find_dir("Dir::Etc"),saved_entry)
      # insert cdrom source first, so that it has precedence over network sources
      self.sourceslist.list.insert(0, SourceEntry(line,full_path))
      self.set_modified_sourceslist()

  def run(self):
    kapp.exec_()
