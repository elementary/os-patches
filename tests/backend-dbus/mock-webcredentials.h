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

#ifndef MOCK_WEBCREDENTIALS_H
#define MOCK_WEBCREDENTIALS_H

#include "mock-object.h" // parent class
#include "backend-dbus/dbus-webcredentials.h" // Webcredentials

class MockWebcredentials: public MockObject
{
  public:

    MockWebcredentials (GMainLoop       * loop,
                        GDBusConnection * bus_connection);
    virtual ~MockWebcredentials ();

    bool has_error () const { return webcredentials_get_error_status (my_skeleton); }
    void set_error (bool b) const { webcredentials_set_error_status (my_skeleton, b); }

  private:

    Webcredentials * my_skeleton;
};

#endif
