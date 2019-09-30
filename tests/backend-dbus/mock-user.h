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

#ifndef MOCK_USER_H
#define MOCK_USER_H

#include "mock-object.h" // parent class
#include "backend-dbus/dbus-user.h" // AccountsUser

class MockUser: public MockObject
{
  protected:

    static guint get_next_uid ();

  public:

    MockUser (GMainLoop       * loop,
              GDBusConnection * bus_connection,
              const char      * userName,
              const char      * realName,
              guint64           login_frequency,
              guint             uid = get_next_uid());
    virtual ~MockUser ();

    const char * username () const;
    const char * realname () const;
    void set_realname (const char *);
    guint uid () const;
    guint64 login_frequency () const;
    //bool system_account() const;

    bool is_guest() const;
    void set_system_account (gboolean b);

  private:

    AccountsUser * my_skeleton;
};

#endif
