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

#include "mock-display-manager-seat.h"
#include "mock-login1-seat.h"

namespace
{
  const char * const DISPLAY_MANAGER_NAME = "org.freedesktop.DisplayManager";

  std::string
  next_unique_path ()
  {
    static int id = 12; // arbitrary; doesn't matter

    char * tmp;
    std::string ret;

    tmp = g_strdup_printf ("/org/freedesktop/DisplayManager/Seat%d", id++);
    ret = tmp;
    g_free (tmp);
    return ret;
  }
}

/***
****
***/

void
MockDisplayManagerSeat :: switch_to_greeter ()
{
  my_last_action = GREETER;
}

gboolean
MockDisplayManagerSeat :: handle_switch_to_greeter (DisplayManagerSeat * o,
                                                    GDBusMethodInvocation * inv,
                                                    gpointer gself)
{
  static_cast<MockDisplayManagerSeat*>(gself)->switch_to_greeter ();
  display_manager_seat_complete_switch_to_greeter (o, inv);
  return true;
}

void
MockDisplayManagerSeat :: set_guest_allowed (bool b)
{
  display_manager_seat_set_has_guest_account (my_skeleton, b);
}

gboolean
MockDisplayManagerSeat :: handle_switch_to_guest (DisplayManagerSeat    * o,
                                                  GDBusMethodInvocation * inv,
                                                  const gchar           * session_name G_GNUC_UNUSED,
                                                  gpointer                gself)
{
  static_cast<MockDisplayManagerSeat*>(gself)->switch_to_guest ();
  display_manager_seat_complete_switch_to_guest (o, inv);
  return true;
}

void
MockDisplayManagerSeat :: switch_to_guest ()
{
  g_assert (my_login1_seat != 0);

  my_last_action = GUEST;
  my_login1_seat->switch_to_guest ();
}

gboolean
MockDisplayManagerSeat :: handle_switch_to_user (DisplayManagerSeat    * o,
                                                 GDBusMethodInvocation * inv,
                                                 const gchar           * username,
                                                 const gchar           * session_name G_GNUC_UNUSED,
                                                 gpointer                gself)
{
  static_cast<MockDisplayManagerSeat*>(gself)->switch_to_user (username);
  display_manager_seat_complete_switch_to_user (o, inv);
  return true;
}

void
MockDisplayManagerSeat :: switch_to_user (const char * username)
{
  g_assert (my_login1_seat != 0);

  my_last_action = USER;
  my_login1_seat->switch_to_user (username);
}

void
MockDisplayManagerSeat :: set_login1_seat (MockLogin1Seat * seat)
{
  my_login1_seat = seat;
}

/***
****
***/

MockDisplayManagerSeat :: MockDisplayManagerSeat (GMainLoop       * loop,
                                                  GDBusConnection * connection):
  MockObject (loop, connection, DISPLAY_MANAGER_NAME, next_unique_path()),
  my_skeleton (display_manager_seat_skeleton_new ()),
  my_last_action (NONE)
{
  g_signal_connect (my_skeleton, "handle-switch-to-guest",
                    G_CALLBACK(handle_switch_to_guest), this);
  g_signal_connect (my_skeleton, "handle-switch-to-user",
                    G_CALLBACK(handle_switch_to_user), this);
  g_signal_connect (my_skeleton, "handle-switch-to-greeter",
                    G_CALLBACK(handle_switch_to_greeter), this);

  set_skeleton (G_DBUS_INTERFACE_SKELETON(my_skeleton));
}

MockDisplayManagerSeat :: ~MockDisplayManagerSeat ()
{
  //g_signal_handlers_disconnect_by_data (my_skeleton, this);
  g_clear_object (&my_skeleton);
}
