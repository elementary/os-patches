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

#ifndef MOCK_SESSION_MANAGER_H
#define MOCK_SESSION_MANAGER_H

#include "mock-object.h" // parent class
#include "backend-dbus/gnome-session-manager.h" // GnomeSessionManager

class MockSessionManager: public MockObject
{
  public:

    MockSessionManager (GMainLoop       * loop,
                        GDBusConnection * bus_connection);
    virtual ~MockSessionManager ();

  public:

    enum Action { None, LogoutNormal, LogoutQuiet, LogoutForce };
    Action last_action () { return my_last_action; }

  private:

    GnomeSessionManager * my_skeleton;
    Action my_last_action;

    static gboolean handle_logout (GnomeSessionManager *,
                                   GDBusMethodInvocation *,
                                   guint,
                                   gpointer);
};

#endif
