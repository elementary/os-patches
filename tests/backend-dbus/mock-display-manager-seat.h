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

#ifndef MOCK_DISPLAY_MANAGER_SEAT_H
#define MOCK_DISPLAY_MANAGER_SEAT_H

#include "mock-object.h" // parent class
#include "backend-dbus/dbus-display-manager.h"

class MockLogin1Seat;

class MockDisplayManagerSeat: public MockObject
{
  public:

    MockDisplayManagerSeat (GMainLoop       * loop,
                            GDBusConnection * bus_connection);
    virtual ~MockDisplayManagerSeat ();

    void set_guest_allowed (bool b);

    void set_login1_seat (MockLogin1Seat * login1_seat);

    void switch_to_guest ();

    void switch_to_greeter ();

    void switch_to_user (const char * username);

  public:

    enum Action { NONE, GUEST, GREETER, USER };

    Action last_action () const { return my_last_action; }

  private:

    static gboolean handle_switch_to_greeter (DisplayManagerSeat *o,
                                              GDBusMethodInvocation *inv,
                                              gpointer gself);
    static gboolean handle_switch_to_guest (DisplayManagerSeat *o,
                                            GDBusMethodInvocation *inv,
                                            const gchar *session_name,
                                            gpointer gself);
    static gboolean handle_switch_to_user (DisplayManagerSeat * o,
                                           GDBusMethodInvocation * inv,
                                           const gchar * username,
                                           const gchar * session_name,
                                           gpointer gself);

    DisplayManagerSeat * my_skeleton;
    MockLogin1Seat * my_login1_seat;
    Action my_last_action;
};

#endif // #ifndef MOCK_DISPLAY_MANAGER_SEAT_H
