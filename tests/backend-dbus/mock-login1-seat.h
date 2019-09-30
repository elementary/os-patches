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

#ifndef MOCK_LOGIN1_SEAT_H
#define MOCK_LOGIN1_SEAT_H

#include <cstring> /* strrchr */
#include <map>
#include <set>
#include <string>
#include "backend-dbus/dbus-login1-seat.h"
#include "mock-object.h"

class MockUser;
class MockLogin1Session;

class MockLogin1Seat: public MockObject
{
  public:

    MockLogin1Seat (GMainLoop       * loop,
                    GDBusConnection * bus_connection,
                    bool              can_activate_sessions);

    virtual ~MockLogin1Seat ();

    const char * seat_id() const { return strrchr(path(),'/')+1; }

    int add_session (MockUser * user);
    void remove_session (int session_tag);
    std::set<int> sessions () const;
    int active_session () const { return my_active_session; }

    std::string user_state (unsigned int uid) const;

    bool can_activate_sessions () const { return my_can_multi_session; }
    void activate_session (int session_tag);
    void switch_to_guest ();
    void switch_to_user (const char * username);

    //const char * sid() { return path(); }
    //MockLogin1Session * find (const char * ssid);

    GVariant * list_sessions ();

    static void get_session_id_and_path_for_tag (int tag, std::string& id, std::string& path);

  private:
    void update_sessions_property ();
    void update_active_session_property ();
    void update_can_multi_session_property ();

  private:
    Login1Seat * my_skeleton;
    std::map<int,MockUser*> my_sessions;
    int my_active_session;
    bool my_can_multi_session;
};

#endif // #ifndef MOCK_LOGIN1_SEAT_H
