/*
 * Copyright 2013 Canonical Ltd.
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
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#include "actions-mock.h"
#include "state-mock.h"
#include "glib-fixture.h"

#include <datetime/actions.h>
#include <datetime/dbus-shared.h>
#include <datetime/exporter.h>

#include <set>
#include <string>

using namespace unity::indicator::datetime;

class ExporterFixture: public GlibFixture
{
private:

    typedef GlibFixture super;

protected:

    GTestDBus* bus = nullptr;

    void SetUp()
    {
        super::SetUp();

        // bring up the test bus
        bus = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(bus);
        const auto address = g_test_dbus_get_bus_address(bus);
        g_setenv("DBUS_SYSTEM_BUS_ADDRESS", address, true);
        g_setenv("DBUS_SESSION_BUS_ADDRESS", address, true);
    }

    void TearDown()
    {
        GError * error = nullptr;
        GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if(!g_dbus_connection_is_closed(connection))
            g_dbus_connection_close_sync(connection, nullptr, &error);
        g_assert_no_error(error);
        g_clear_object(&connection);
        g_test_dbus_down(bus);
        g_clear_object(&bus);

        super::TearDown();
    }
};

TEST_F(ExporterFixture, HelloWorld)
{
    // confirms that the Test DBus SetUp() and TearDown() works
}

TEST_F(ExporterFixture, Publish)
{
    std::shared_ptr<State> state(new MockState);
    std::shared_ptr<Actions> actions(new MockActions(state));
    std::vector<std::shared_ptr<Menu>> menus;

    MenuFactory menu_factory (actions, state);
    for(int i=0; i<Menu::NUM_PROFILES; i++)
      menus.push_back(menu_factory.buildMenu(Menu::Profile(i)));

    Exporter exporter;
    exporter.publish(actions, menus);
    wait_msec();

    auto connection = g_bus_get_sync (G_BUS_TYPE_SESSION, nullptr, nullptr);
    auto exported = g_dbus_action_group_get (connection, BUS_NAME, BUS_PATH);
    auto names_strv = g_action_group_list_actions(G_ACTION_GROUP(exported));

    // wait for the exported ActionGroup to be populated
    if (g_strv_length(names_strv) == 0)
    {
        g_strfreev(names_strv);
        wait_for_signal(exported, "action-added");
        names_strv = g_action_group_list_actions(G_ACTION_GROUP(exported));
    }

    // convert it to a std::set for easy prodding
    std::set<std::string> names;
    for(int i=0; names_strv && names_strv[i]; i++)
      names.insert(names_strv[i]);

    // confirm the actions that we expect
    EXPECT_EQ(1, names.count("desktop.open-alarm-app"));
    EXPECT_EQ(1, names.count("desktop.open-appointment"));
    EXPECT_EQ(1, names.count("desktop.open-calendar-app"));
    EXPECT_EQ(1, names.count("desktop.open-settings-app"));
    EXPECT_EQ(1, names.count("phone.open-alarm-app"));
    EXPECT_EQ(1, names.count("phone.open-appointment"));
    EXPECT_EQ(1, names.count("phone.open-calendar-app"));
    EXPECT_EQ(1, names.count("phone.open-settings-app"));
    EXPECT_EQ(1, names.count("calendar"));
    EXPECT_EQ(1, names.count("desktop_greeter-header"));
    EXPECT_EQ(1, names.count("desktop-header"));
    EXPECT_EQ(1, names.count("phone_greeter-header"));
    EXPECT_EQ(1, names.count("phone-header"));
    EXPECT_EQ(1, names.count("set-location"));

    // try closing the connection prematurely
    // to test Exporter's name-lost signal
    bool name_lost = false;
    exporter.name_lost.connect([this,&name_lost](){
        name_lost = true;
        g_main_loop_quit(loop);
    });
    g_dbus_connection_close_sync(connection, nullptr, nullptr);
    g_main_loop_run(loop);
    EXPECT_TRUE(name_lost);

    // cleanup
    g_strfreev(names_strv);
    g_clear_object(&exported);
    g_clear_object(&connection);
}
