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

#ifndef MOCK_LOGIN1_MANAGER_H
#define MOCK_LOGIN1_MANAGER_H

#include <set>
#include <string>
#include "mock-object.h"
#include "backend-dbus/dbus-login1-manager.h"

class MockLogin1Seat;
class MockUser;

class MockLogin1Manager: public MockObject
{
  public:

    MockLogin1Manager (GMainLoop       * loop,
                           GDBusConnection * bus_connection);
    virtual ~MockLogin1Manager ();

    int add_session (MockLogin1Seat * seat, MockUser * user);
    void remove_session (MockLogin1Seat * seat, int session_tag);

    void add_seat (MockLogin1Seat * seat);

    const std::string& can_suspend () const;
    const std::string& can_hibernate () const;

    const std::string& last_action () const { return my_last_action; }
    void clear_last_action () { my_last_action.clear(); }

  private:

    void emit_session_new (MockLogin1Seat * seat, int tag) const;
    void emit_session_removed (MockLogin1Seat * seat, int tag) const;

    GVariant * list_sessions () const;

    static gboolean handle_list_sessions (Login1Manager *, GDBusMethodInvocation *, gpointer);
    static gboolean handle_can_suspend   (Login1Manager *, GDBusMethodInvocation *, gpointer);
    static gboolean handle_can_hibernate (Login1Manager *, GDBusMethodInvocation *, gpointer);
    static gboolean handle_reboot        (Login1Manager *, GDBusMethodInvocation *, gboolean, gpointer);
    static gboolean handle_power_off     (Login1Manager *, GDBusMethodInvocation *, gboolean, gpointer);
    static gboolean handle_suspend       (Login1Manager *, GDBusMethodInvocation *, gboolean, gpointer);
    static gboolean handle_hibernate     (Login1Manager *, GDBusMethodInvocation *, gboolean, gpointer);

  private:

    Login1Manager * my_skeleton;
    std::set<MockLogin1Seat*> my_seats;
    std::string my_can_suspend;
    std::string my_can_hibernate;
    std::string my_last_action;
};

#endif // #ifndef MOCK_LOGIN1_MANAGER_H
