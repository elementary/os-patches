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

#ifndef MOCK_ACCOUNTS_H
#define MOCK_ACCOUNTS_H

#include <set>
#include <string>
#include <map>
#include "mock-object.h"
#include "backend-dbus/dbus-accounts.h" // struct Accounts

class MockUser;

class MockAccounts: public MockObject
{
  public:

    MockAccounts (GMainLoop       * loop,
                  GDBusConnection * bus_connection);
    virtual ~MockAccounts ();

    void add_user (MockUser * user);
    void remove_user (MockUser * user);
    size_t size() const { return my_users.size(); }
    MockUser * find_by_uid (guint64 uid);
    MockUser * find_by_username (const char * username);

  private:

    Accounts * my_skeleton;

    typedef std::set<MockUser*> users_t;
    users_t my_users;

    typedef std::map<guint,MockUser*> uid_to_user_t;
    uid_to_user_t my_uid_to_user;

    typedef std::map<std::string,MockUser*> path_to_user_t;
    path_to_user_t my_path_to_user;

    typedef std::map<std::string,MockUser*> username_to_user_t;
    username_to_user_t my_username_to_user;

  private:

    static gboolean on_find_user_by_id_static (Accounts *,
                                               GDBusMethodInvocation *,
                                               guint64,
                                               gpointer);

    static gboolean on_list_cached_users_static (Accounts *,
                                                 GDBusMethodInvocation *,
                                                 gpointer);
};

#endif // #ifndef MOCK_ACCOUNTS_H
