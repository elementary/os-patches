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

#include "mock-accounts.h"
#include "mock-user.h"

namespace
{
  const char * const DBUS_ACCOUNTS_NAME = "org.freedesktop.Accounts";

  const char * const DBUS_ACCOUNTS_PATH = "/org/freedesktop/Accounts";
}

/***
****
***/

void
MockAccounts :: add_user (MockUser * user)
{
  g_assert (my_users.count(user) == 0);

  my_users.insert (user);
  my_uid_to_user[user->uid()] = user;
  my_path_to_user[user->path()] = user;
  my_username_to_user[user->username()] = user;

  accounts_emit_user_added (my_skeleton, user->path());
}

void
MockAccounts :: remove_user (MockUser * user)
{
  g_assert (my_users.count(user) == 1);

  my_users.erase (user);
  my_uid_to_user.erase (user->uid());
  my_path_to_user.erase (user->path());
  my_username_to_user.erase (user->username());

  accounts_emit_user_deleted (my_skeleton, user->path());
}

MockUser *
MockAccounts :: find_by_uid (guint64 uid)
{
  const uid_to_user_t::iterator it (my_uid_to_user.find(uid));

  if (it != my_uid_to_user.end())
    return it->second;

  g_warn_if_reached ();
  return 0;
}

MockUser *
MockAccounts :: find_by_username (const char * username)
{
  const username_to_user_t::iterator it (my_username_to_user.find(username));

  if (it != my_path_to_user.end())
    return it->second;

  g_warn_if_reached ();
  return 0;
}

/***
****
***/

gboolean
MockAccounts :: on_find_user_by_id_static (Accounts              * a,
                                           GDBusMethodInvocation * invocation,
                                           guint64                 uid,
                                           gpointer                gself)
{
  MockUser * user = static_cast<MockAccounts*>(gself)->find_by_uid (uid);
  accounts_complete_find_user_by_id (a, invocation, user ? user->path() : "");
  return true;
}

gboolean
MockAccounts :: on_list_cached_users_static (Accounts              * a,
                                             GDBusMethodInvocation * invocation,
                                             gpointer                gself)
{
  int i;
  const char ** paths;
  const users_t& users = static_cast<MockAccounts*>(gself)->my_users;

  i = 0;
  paths = g_new0 (const char*, users.size() + 1);
  for (auto it : users)
    paths[i++] = it->path();
  accounts_complete_list_cached_users (a, invocation, paths);
  g_free (paths);

  return true;
}

/***
****
***/

MockAccounts :: MockAccounts (GMainLoop       * loop,
                              GDBusConnection * bus_connection):
  MockObject (loop, bus_connection, DBUS_ACCOUNTS_NAME, DBUS_ACCOUNTS_PATH),
  my_skeleton (accounts_skeleton_new ())
{
  g_signal_connect (my_skeleton, "handle-list-cached-users",
                    G_CALLBACK(on_list_cached_users_static), this);
  g_signal_connect (my_skeleton, "handle-find-user-by-id",
                    G_CALLBACK(on_find_user_by_id_static), this);

  set_skeleton (G_DBUS_INTERFACE_SKELETON(my_skeleton));
}

MockAccounts :: ~MockAccounts ()
{
  for (users_t::iterator it(my_users.begin()),
                        end(my_users.end()); it!=end; ++it)
    delete *it;

  g_signal_handlers_disconnect_by_data (my_skeleton, this);
  g_clear_object (&my_skeleton);
}
