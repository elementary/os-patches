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

#include "mock-screen-saver.h"


gboolean
MockScreenSaver :: handle_lock (GnomeScreenSaver      * ss,
                                GDBusMethodInvocation * inv,
                                gpointer                gself)
{
  static_cast<MockScreenSaver*>(gself)->my_last_action = Lock;
  gnome_screen_saver_complete_lock (ss, inv);
  return true;
}

gboolean
MockScreenSaver :: handle_simulate_user_activity (GnomeScreenSaver      * ss,
                                                  GDBusMethodInvocation * inv,
                                                  gpointer                gself)
{
  static_cast<MockScreenSaver*>(gself)->my_last_action = UserActivity;
  gnome_screen_saver_complete_simulate_user_activity (ss, inv);
  return true;
}

/***
****
***/

namespace
{
  const char * const SCREENSAVER_NAME = "org.gnome.ScreenSaver";
  const char * const SCREENSAVER_PATH = "/org/gnome/ScreenSaver";

}

MockScreenSaver :: MockScreenSaver (GMainLoop       * loop,
                                    GDBusConnection * bus_connection):
  MockObject (loop, bus_connection, SCREENSAVER_NAME, SCREENSAVER_PATH),
  my_skeleton (gnome_screen_saver_skeleton_new ()),
  my_last_action (None)
{
  g_signal_connect (my_skeleton, "handle-lock",
                    G_CALLBACK(handle_lock), this);
  g_signal_connect (my_skeleton, "handle-simulate-user-activity",
                    G_CALLBACK(handle_simulate_user_activity), this);

  set_skeleton (G_DBUS_INTERFACE_SKELETON(my_skeleton));
}

MockScreenSaver :: ~MockScreenSaver ()
{
  g_clear_object (&my_skeleton);
}
