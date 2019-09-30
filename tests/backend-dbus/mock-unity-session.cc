/*
 * Copyright 2014 Canonical Ltd.
 *
 * Authors:
 *   Marco Trevisan <marco.trevisan@canonical.com>
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

#include "mock-unity-session.h"


gboolean
MockUnitySession :: handle_lock (UnitySession          * us,
                                 GDBusMethodInvocation * inv,
                                 gpointer                gself)
{
  static_cast<MockUnitySession*>(gself)->my_last_action = Lock;
  unity_session_complete_lock (us, inv);
  return true;
}

gboolean
MockUnitySession :: handle_prompt_lock (UnitySession          * us,
                                        GDBusMethodInvocation * inv,
                                        gpointer                gself)
{
  static_cast<MockUnitySession*>(gself)->my_last_action = PromptLock;
  unity_session_complete_prompt_lock (us, inv);
  return true;
}

gboolean
MockUnitySession :: handle_request_logout (UnitySession          * us,
                                           GDBusMethodInvocation * inv,
                                           gpointer                gself)
{
  static_cast<MockUnitySession*>(gself)->my_last_action = RequestLogout;
  unity_session_complete_request_logout (us, inv);
  return true;
}

/***
****
***/

namespace
{
  const char * const UNITY_SESSION_NAME = "com.canonical.Unity";
  const char * const UNITY_SESSION_PATH = "/com/canonical/Unity/Session";

}

MockUnitySession :: MockUnitySession (GMainLoop       * loop,
                                      GDBusConnection * bus_connection):
  MockObject (loop, bus_connection, UNITY_SESSION_NAME, UNITY_SESSION_PATH),
  my_skeleton (unity_session_skeleton_new ()),
  my_last_action (None)
{
  g_signal_connect (my_skeleton, "handle-lock",
                    G_CALLBACK(handle_lock), this);
  g_signal_connect (my_skeleton, "handle-prompt-lock",
                    G_CALLBACK(handle_prompt_lock), this);
  g_signal_connect (my_skeleton, "handle-request-logout",
                    G_CALLBACK(handle_request_logout), this);

  set_skeleton (G_DBUS_INTERFACE_SKELETON(my_skeleton));
}

MockUnitySession :: ~MockUnitySession ()
{
  g_clear_object (&my_skeleton);
}
