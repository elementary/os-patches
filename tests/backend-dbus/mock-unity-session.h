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

#ifndef MOCK_UNITY_SESSION_H
#define MOCK_UNITY_SESSION_H

#include "mock-object.h" // parent class
#include "backend-dbus/unity-session.h" // Unity Session

class MockUnitySession: public MockObject
{
  public:

    MockUnitySession (GMainLoop       * loop,
                      GDBusConnection * bus_connection);
    virtual ~MockUnitySession ();

  public:

    enum Action { None, Lock, PromptLock, RequestLogout, RequestShutdown, RequestReboot };
    Action last_action () { return my_last_action; }
    void clear_last_action () { my_last_action = None; }

  private:

    UnitySession * my_skeleton;
    Action my_last_action;

    static gboolean handle_lock (UnitySession *,
                                 GDBusMethodInvocation *,
                                 gpointer);
    static gboolean handle_prompt_lock (UnitySession *,
                                        GDBusMethodInvocation *,
                                        gpointer);
    static gboolean handle_request_logout (UnitySession *,
                                           GDBusMethodInvocation *,
                                           gpointer);

};

#endif
