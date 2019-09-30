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

#include "mock-login1-seat.h"

#include "mock-object.h"
#include "mock-user.h"

namespace
{
  const char * BUS_NAME = "org.freedesktop.login1";

  std::string next_unique_sid ()
  {
    static int id = 1;

    char * tmp;
    std::string ret;

    tmp = g_strdup_printf ("/org/freedesktop/login1/seat/seat%d", id++);
    ret = tmp;
    g_free (tmp);
    return ret;
  }

  static int next_session_tag = 1;
}

void
MockLogin1Seat :: get_session_id_and_path_for_tag (int           tag,
                                                   std::string & id,
                                                   std::string & path)
{
  if (tag)
    {
      char tmp[80];

      g_snprintf (tmp, sizeof(tmp), "c%d", tag);
      id = tmp;

      g_snprintf (tmp, sizeof(tmp), "/org/freedesktop/login1/session/%s", id.c_str());
      path = tmp;
    }
  else
    {
      id = "";
      path = "";
    }
}


/***
****
***/

void
MockLogin1Seat :: update_sessions_property ()
{
  GVariantBuilder b;

  g_variant_builder_init (&b, G_VARIANT_TYPE("a(so)"));
  for (const auto& it : my_sessions)
    {
      std::string id, path;
      get_session_id_and_path_for_tag (it.first, id, path);
      g_variant_builder_add (&b, "(so)", id.c_str(), path.c_str());
    }

  GVariant * v = g_variant_builder_end (&b);
  g_object_set (my_skeleton, "sessions", v, NULL);
}

void
MockLogin1Seat :: update_active_session_property ()
{
  std::string id;
  std::string path;
  get_session_id_and_path_for_tag (my_active_session, id, path);

  GVariant * v = g_variant_new ("(so)", id.c_str(), path.c_str());
  g_object_set (my_skeleton, "active-session", v, NULL);
}

void
MockLogin1Seat :: update_can_multi_session_property ()
{
  g_object_set (my_skeleton, "can-multi-session", my_can_multi_session, NULL);
}

/***
****
***/

/* lists this seat's sessions in the format of Login1Manager::ListSessions() */
GVariant *
MockLogin1Seat :: list_sessions ()
{
  GVariantBuilder b;
  g_variant_builder_init (&b, G_VARIANT_TYPE("a(susso)"));
  for (auto it : my_sessions)
    {
      std::string id, path;
      get_session_id_and_path_for_tag (it.first, id, path);
      g_variant_builder_add (&b, "(susso)",
                             id.c_str(),
                             uint32_t(it.second->uid()),
                             it.second->username(),
                             seat_id(),
                             path.c_str());
    }

  return g_variant_builder_end (&b);
}

/***
****
***/

std::set<int>
MockLogin1Seat :: sessions () const
{
  std::set<int> ret;

  for (auto it : my_sessions)
    ret.insert (it.first);

  return ret;
}

int
MockLogin1Seat :: add_session (MockUser * user)
{
  const int tag = next_session_tag++;

  my_sessions[tag] = user;
  update_sessions_property ();

  return tag;
}

void
MockLogin1Seat :: remove_session (int session_tag)
{
  my_sessions.erase (session_tag);
  update_sessions_property ();
}

/***
****
***/

std::string
MockLogin1Seat :: user_state (unsigned int uid) const
{
  for (auto it : my_sessions)
    if (it.second->uid() == uid)
      return it.first == my_active_session ? "active" : "online";

  return "offline"; // no matching session
}

void
MockLogin1Seat :: activate_session (int session_tag)
{
  g_assert (my_sessions.count(session_tag) == 1);

  if (my_active_session != session_tag)
    {
      std::string id, path;
      my_active_session = session_tag;
      get_session_id_and_path_for_tag (session_tag, id, path);
      g_setenv ("XDG_SESSION_ID", id.c_str(), true);
      update_active_session_property ();

    }
}

void
MockLogin1Seat :: switch_to_guest ()
{
  for (const auto& it : my_sessions)
    {
      if (it.second->is_guest())
        {
          activate_session (it.first);
          return;
        }
    }

  g_warn_if_reached ();
}

void
MockLogin1Seat :: switch_to_user (const char * username)
{
  for (const auto& it : my_sessions)
    {
      if (!g_strcmp0 (username, it.second->username()))
        {
          activate_session (it.first);
          return;
        }
    }

  g_warn_if_reached ();
}

/***
****  Life Cycle
***/

MockLogin1Seat :: MockLogin1Seat (GMainLoop       * loop,
                                  GDBusConnection * bus_connection,
                                  bool              can_activate_sessions):
  MockObject (loop, bus_connection, BUS_NAME, next_unique_sid()),
  my_skeleton (login1_seat_skeleton_new ()),
  my_active_session (0),
  my_can_multi_session (can_activate_sessions)

{
  set_skeleton (G_DBUS_INTERFACE_SKELETON(my_skeleton));
  update_can_multi_session_property ();
}

MockLogin1Seat :: ~MockLogin1Seat ()
{
  g_clear_object (&my_skeleton);
}
