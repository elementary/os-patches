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

#include "glib-fixture.h"

#include <libdbustest/dbus-test.h>

class GeoclueFixture : public GlibFixture
{
  private:

    typedef GlibFixture super;

    GDBusConnection * bus = nullptr;

  protected:

    DbusTestService * service = nullptr;
    DbusTestDbusMock * mock = nullptr;
    DbusTestDbusMockObject * obj_geo = nullptr;
    DbusTestDbusMockObject * obj_geo_m = nullptr;
    DbusTestDbusMockObject * obj_geo_mc = nullptr;
    DbusTestDbusMockObject * obj_geo_addr = nullptr;
    const std::string timezone_1 = "America/Denver";

    void SetUp ()
    {
      super::SetUp();

      GError * error = nullptr;
      const auto master_path = "/org/freedesktop/Geoclue/Master";
      const auto client_path = "/org/freedesktop/Geoclue/Master/client0";
      GString * gstr = g_string_new (nullptr);

      service = dbus_test_service_new (nullptr);
      mock = dbus_test_dbus_mock_new ("org.freedesktop.Geoclue.Master");

      auto interface = "org.freedesktop.Geoclue.Master";
      obj_geo_m = dbus_test_dbus_mock_get_object (mock, master_path, interface, nullptr);
      g_string_printf (gstr, "ret = '%s'", client_path);
      dbus_test_dbus_mock_object_add_method (mock, obj_geo_m, "Create", nullptr, G_VARIANT_TYPE_OBJECT_PATH, gstr->str, &error);

      interface = "org.freedesktop.Geoclue.MasterClient";
      obj_geo_mc = dbus_test_dbus_mock_get_object (mock, client_path, interface, nullptr);
      dbus_test_dbus_mock_object_add_method (mock, obj_geo_mc, "SetRequirements", G_VARIANT_TYPE("(iibi)"), nullptr, "", &error);
      dbus_test_dbus_mock_object_add_method (mock, obj_geo_mc, "AddressStart", nullptr, nullptr, "", &error);

      interface = "org.freedesktop.Geoclue";
      obj_geo = dbus_test_dbus_mock_get_object (mock, client_path, interface, nullptr);
      dbus_test_dbus_mock_object_add_method (mock, obj_geo, "AddReference", nullptr, nullptr, "", &error);
      g_string_printf (gstr, "ret = (1385238033, {'timezone': '%s'}, (3, 0.0, 0.0))", timezone_1.c_str());

      interface = "org.freedesktop.Geoclue.Address";
      obj_geo_addr = dbus_test_dbus_mock_get_object (mock, client_path, interface, nullptr);
      dbus_test_dbus_mock_object_add_method (mock, obj_geo_addr, "GetAddress", nullptr, G_VARIANT_TYPE("(ia{ss}(idd))"), gstr->str, &error);
                                             
      dbus_test_service_add_task(service, DBUS_TEST_TASK(mock));
      dbus_test_service_start_tasks(service);

      bus = g_bus_get_sync (G_BUS_TYPE_SESSION, nullptr, nullptr);
      g_dbus_connection_set_exit_on_close (bus, FALSE);
      g_object_add_weak_pointer (G_OBJECT(bus), (gpointer*)&bus);

      g_string_free (gstr, TRUE);
    }

    virtual void TearDown ()
    {
      g_clear_object (&mock);
      g_clear_object (&service);
      g_object_unref (bus);

      unsigned int cleartry = 0;
      while (bus != nullptr && cleartry < 10)
        {
          wait_msec (100);
          cleartry++;
        }

      // I've looked and can't find where this extra ref is coming from.
      // is there an unbalanced ref to the bus in the test harness?!
      while (bus != nullptr)
        {
          g_object_unref (bus);
          wait_msec (1000);
        }

      super::TearDown ();
    }

private:

    struct EmitAddressChangedData
    {
        DbusTestDbusMock * mock = nullptr;
        DbusTestDbusMockObject * obj_geo_addr = nullptr;
        std::string timezone;
        EmitAddressChangedData(DbusTestDbusMock* mock_,
                               DbusTestDbusMockObject* obj_geo_addr_,
                               const std::string& timezone_): mock(mock_), obj_geo_addr(obj_geo_addr_), timezone(timezone_) {}
    };

    static gboolean emit_address_changed_idle (gpointer gdata)
    {
        auto data = static_cast<EmitAddressChangedData*>(gdata);
        auto fmt = g_strdup_printf ("(1385238033, {'timezone': '%s'}, (3, 0.0, 0.0))", data->timezone.c_str());

        GError * error = nullptr;
        dbus_test_dbus_mock_object_emit_signal(data->mock, data->obj_geo_addr,
                                               //"org.freedesktop.Geoclue.Address",
                                               "AddressChanged",
                                               G_VARIANT_TYPE("(ia{ss}(idd))"),
                                               g_variant_new_parsed (fmt),
                                               &error);
        if (error)
        {
            g_warning("%s: %s", G_STRFUNC, error->message);
            g_error_free (error);
        }

        g_free (fmt);
        delete data;
        return G_SOURCE_REMOVE;
    }

public:

    void setGeoclueTimezoneOnIdle (const std::string& newZone)
    {
        g_timeout_add (50, emit_address_changed_idle, new EmitAddressChangedData(mock, obj_geo_addr, newZone.c_str()));
    }

};

