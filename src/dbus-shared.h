/*
DBus defaults for everyone to share in the project.

Copyright 2009 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#define INDICATOR_APPLICATION_DBUS_ADDR        "com.canonical.indicator.application"
#define INDICATOR_APPLICATION_DBUS_OBJ         "/com/canonical/indicator/application/service"
#define INDICATOR_APPLICATION_DBUS_IFACE       "com.canonical.indicator.application.service"
#define INDICATOR_APPLICATION_SERVICE_VERSION  2

#define NOTIFICATION_WATCHER_DBUS_ADDR    "org.kde.StatusNotifierWatcher"
#define NOTIFICATION_WATCHER_DBUS_OBJ     "/StatusNotifierWatcher"
#define NOTIFICATION_WATCHER_DBUS_IFACE   "org.kde.StatusNotifierWatcher"

#define NOTIFICATION_ITEM_DBUS_IFACE      "org.kde.StatusNotifierItem"
#define NOTIFICATION_ITEM_DEFAULT_OBJ     "/StatusNotifierItem"
