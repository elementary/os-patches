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

#ifndef MOCK_END_SESSION_DIALOG_H
#define MOCK_END_SESSION_DIALOG_H

#include "mock-object.h" // parent class
#include "backend-dbus/dbus-end-session-dialog.h" // EndSessionDialog

class MockEndSessionDialog: public MockObject
{
  public:

    MockEndSessionDialog (GMainLoop       * loop,
                          GDBusConnection * bus_connection);
    virtual ~MockEndSessionDialog ();

    bool is_open () const { return my_isOpen; }

    void cancel ()           { my_isOpen = false; end_session_dialog_emit_canceled (my_skeleton); }
    void confirm_logout ()   { my_isOpen = false; end_session_dialog_emit_confirmed_logout (my_skeleton); }
    void confirm_reboot ()   { my_isOpen = false; end_session_dialog_emit_confirmed_reboot (my_skeleton); }
    void confirm_shutdown () { my_isOpen = false; end_session_dialog_emit_confirmed_shutdown (my_skeleton); }
    void close ()            { my_isOpen = false; end_session_dialog_emit_closed (my_skeleton); }

  private:

    EndSessionDialog * my_skeleton;

    bool my_isOpen;

    static gboolean handle_open (EndSessionDialog *,
                                 GDBusMethodInvocation *,
                                 guint,
                                 guint,
                                 guint,
                                 const gchar * const *,
                                 gpointer);


#if 0
    static gboolean handle_lock (GnomeScreenSaver *,
                                 GDBusMethodInvocation *,
                                 gpointer);
    static gboolean handle_simulate_user_activity (GnomeScreenSaver *,
                                                   GDBusMethodInvocation *,
                                                   gpointer);
#endif
};

#endif
