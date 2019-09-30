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

#include "mock-session-manager.h"

gboolean
MockSessionManager :: handle_logout (GnomeSessionManager   * gsm,
                                     GDBusMethodInvocation * inv,
                                     guint                   arg,
                                     gpointer                gself)
{
  Action action;
  switch (arg) {
    case 0: action = LogoutNormal; break;
    case 1: action = LogoutQuiet; break;
    case 2: action = LogoutForce; break;
    default: action = None; break;
  }
  static_cast<MockSessionManager*>(gself)->my_last_action = action;
  gnome_session_manager_complete_logout (gsm, inv);
  return true;
}

 /***
****
***/

namespace
{
  const char * const SESSION_MANAGER_NAME = "org.gnome.SessionManager";
  const char * const SESSION_MANAGER_PATH = "/org/gnome/SessionManager";

}

MockSessionManager :: MockSessionManager (GMainLoop       * loop,
                                          GDBusConnection * bus_connection):
  MockObject (loop, bus_connection, SESSION_MANAGER_NAME, SESSION_MANAGER_PATH),
  my_skeleton (gnome_session_manager_skeleton_new ()),
  my_last_action (None)
{
  g_signal_connect (my_skeleton, "handle-logout",
                    G_CALLBACK(handle_logout), this);

  set_skeleton (G_DBUS_INTERFACE_SKELETON(my_skeleton));
}

MockSessionManager :: ~MockSessionManager ()
{
  g_clear_object (&my_skeleton);
}
