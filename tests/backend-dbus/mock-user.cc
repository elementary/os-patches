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

#include "mock-user.h"

/***
****
***/

const char *
MockUser :: username () const
{
  return accounts_user_get_user_name (my_skeleton);
}

const char * 
MockUser :: realname () const
{
  return accounts_user_get_real_name (my_skeleton);
}

void
MockUser :: set_realname (const char * realname)
{
  accounts_user_set_real_name (my_skeleton, realname);
  accounts_user_emit_changed (my_skeleton);
}

guint
MockUser :: uid () const
{
  return accounts_user_get_uid (my_skeleton);
}

guint64
MockUser :: login_frequency () const
{
  return accounts_user_get_login_frequency (my_skeleton);
}

void
MockUser :: set_system_account (gboolean b)
{
  accounts_user_set_system_account (my_skeleton, b);
}

bool
MockUser :: is_guest () const
{
  // a guest will look like this:
  // username:[guest-jjbEVV] realname:[Guest] system:[1]
  return accounts_user_get_system_account (my_skeleton)
    && !g_ascii_strcasecmp (accounts_user_get_real_name(my_skeleton), "Guest");
}

/***
****
***/

namespace
{
  const char * const DBUS_ACCOUNTS_NAME = "org.freedesktop.Accounts";

  static guint next_uid = 1000;

  std::string path_for_uid (guint uid)
  {
    char * tmp;
    std::string ret;
    const char * const DBUS_ACCOUNTS_PATH = "/org/freedesktop/Accounts";
    tmp = g_strdup_printf ("%s/User%u", DBUS_ACCOUNTS_PATH, uid);
    ret = tmp;
    g_free (tmp);
    return ret;
  }
}

guint
MockUser :: get_next_uid ()
{
  return next_uid++;
}


MockUser :: MockUser (GMainLoop       * loop,
                      GDBusConnection * bus_connection,
                      const char      * userName,
                      const char      * realName,
                      guint64           login_frequency,
                      guint             uid_):
  MockObject (loop, bus_connection, DBUS_ACCOUNTS_NAME, path_for_uid(uid_)),
  my_skeleton (accounts_user_skeleton_new ())
{
  accounts_user_set_uid (my_skeleton, uid_);
  accounts_user_set_user_name (my_skeleton, userName);
  accounts_user_set_real_name (my_skeleton, realName);
  accounts_user_set_login_frequency (my_skeleton, login_frequency);
  accounts_user_set_system_account (my_skeleton, false);

  set_skeleton (G_DBUS_INTERFACE_SKELETON(my_skeleton));
}

MockUser :: ~MockUser ()
{
  g_signal_handlers_disconnect_by_data (my_skeleton, this);
  g_clear_object (&my_skeleton);
}
