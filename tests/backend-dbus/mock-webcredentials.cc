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

#include "mock-webcredentials.h"

namespace
{
  const char * const MY_NAME = "com.canonical.indicators.webcredentials";
  const char * const MY_PATH = "/com/canonical/indicators/webcredentials";
}

MockWebcredentials :: MockWebcredentials (GMainLoop       * loop,
                                          GDBusConnection * bus_connection):
  MockObject (loop, bus_connection, MY_NAME, MY_PATH),
  my_skeleton (webcredentials_skeleton_new ())
{
  set_skeleton (G_DBUS_INTERFACE_SKELETON(my_skeleton));
}

MockWebcredentials :: ~MockWebcredentials ()
{
  g_clear_object (&my_skeleton);
}
