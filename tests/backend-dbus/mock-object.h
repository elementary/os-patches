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

#ifndef MOCK_OBJECT_H
#define MOCK_OBJECT_H

#include <string>

#include <glib.h>
#include <gio/gio.h>

class MockObject
{
  public:

    MockObject (GMainLoop          * loop,
                GDBusConnection    * bus_connection,
                const std::string  & object_name,
                const std::string  & object_path);

    virtual ~MockObject ();

    const char * name() const { return my_object_name.c_str(); }
    const char * path() const { return my_object_path.c_str(); }

    GDBusInterfaceSkeleton * skeleton() { return my_skeleton; }

  protected:

    guint my_owner_id;
    GMainLoop * my_loop;
    GDBusConnection * my_bus_connection;
    const std::string my_object_name;
    const std::string my_object_path;
    GDBusInterfaceSkeleton * my_skeleton;

    void set_skeleton (GDBusInterfaceSkeleton * skeleton);

  private:
    // safeguard to make sure we don't copy-by-value...
    // this object's holding a handful of pointers
    MockObject (const MockObject& rhs);
    MockObject& operator= (const MockObject& rhs);
};

#endif
