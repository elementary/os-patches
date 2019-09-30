/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MOCK_SCREENSAVER_H
#define MOCK_SCREENSAVER_H

#include "mock-object.h" // parent class
#include "backend-dbus/gnome-screen-saver.h" // GnomeScreenSaver

class MockScreenSaver: public MockObject
{
  public:

    MockScreenSaver (GMainLoop       * loop,
                     GDBusConnection * bus_connection);
    virtual ~MockScreenSaver ();

  public:

    enum Action { None, Lock, UserActivity };
    Action last_action () { return my_last_action; }

  private:

    GnomeScreenSaver * my_skeleton;
    Action my_last_action;

    static gboolean handle_lock (GnomeScreenSaver *,
                                 GDBusMethodInvocation *,
                                 gpointer);
    static gboolean handle_simulate_user_activity (GnomeScreenSaver *,
                                                   GDBusMethodInvocation *,
                                                   gpointer);

};

#endif
