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
#include "state-fixture.h"

#include <datetime/clock-mock.h>
#include <datetime/locations.h>
#include <datetime/menu.h>
#include <datetime/state.h>

#include <gio/gio.h>

using namespace unity::indicator::datetime;

class MenuFixture: public StateFixture
{
private:
    typedef StateFixture super;

protected:
    std::shared_ptr<MenuFactory> m_menu_factory;
    std::vector<std::shared_ptr<Menu>> m_menus;

    virtual void SetUp()
    {
        super::SetUp();

        // build the menus on top of the actions and state
        m_menu_factory.reset(new MenuFactory(m_actions, m_state));
        for(int i=0; i<Menu::NUM_PROFILES; i++)
            m_menus.push_back(m_menu_factory->buildMenu(Menu::Profile(i)));
    }

    virtual void TearDown()
    {
        m_menus.clear();
        m_menu_factory.reset();

        super::TearDown();
    }

    void InspectHeader(GMenuModel* menu_model, const std::string& name)
    {
        // check that there's a header menuitem
        EXPECT_EQ(1,g_menu_model_get_n_items(menu_model));
        gchar* str = nullptr;
        g_menu_model_get_item_attribute(menu_model, 0, "x-canonical-type", "s", &str);
        EXPECT_STREQ("com.canonical.indicator.root", str);
        g_clear_pointer(&str, g_free);
        g_menu_model_get_item_attribute(menu_model, 0, G_MENU_ATTRIBUTE_ACTION, "s", &str);
        const auto action_name = name + "-header";
        EXPECT_EQ(std::string("indicator.")+action_name, str);
        g_clear_pointer(&str, g_free);

        // check the header
        auto dict = g_action_group_get_action_state(m_actions->action_group(), action_name.c_str());
        EXPECT_TRUE(dict != nullptr);
        EXPECT_TRUE(g_variant_is_of_type(dict, G_VARIANT_TYPE_VARDICT));
        auto v = g_variant_lookup_value(dict, "accessible-desc", G_VARIANT_TYPE_STRING);
        EXPECT_TRUE(v != nullptr);
        g_variant_unref(v);
        v = g_variant_lookup_value(dict, "label", G_VARIANT_TYPE_STRING);
        EXPECT_TRUE(v != nullptr);
        g_variant_unref(v);
        v = g_variant_lookup_value(dict, "title", G_VARIANT_TYPE_STRING);
        EXPECT_TRUE(v != nullptr);
        g_variant_unref(v);
        v = g_variant_lookup_value(dict, "visible", G_VARIANT_TYPE_BOOLEAN);
        EXPECT_TRUE(v != nullptr);
        g_variant_unref(v);
        g_variant_unref(dict);
    }

    void InspectCalendar(GMenuModel* menu_model, Menu::Profile profile)
    {
        gchar* str = nullptr;

        const char * expected_action;

        if (profile == Menu::Desktop)
            expected_action = "indicator.desktop.open-calendar-app";
        else if (profile == Menu::Phone)
            expected_action = "indicator.phone.open-calendar-app";
        else
            expected_action = nullptr;

        const auto calendar_expected = ((profile == Menu::Desktop) || (profile == Menu::DesktopGreeter))
                                    && (m_state->settings->show_calendar.get());

        // get the calendar section
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);
        auto section = g_menu_model_get_item_link(submenu, Menu::Calendar, G_MENU_LINK_SECTION);

        // should be one or two items: a date label and maybe a calendar
        ASSERT_TRUE(section != nullptr);
        auto n_expected = calendar_expected ? 2 : 1;
        EXPECT_EQ(n_expected, g_menu_model_get_n_items(section));

        // look at the date menuitem
        g_menu_model_get_item_attribute(section, 0, G_MENU_ATTRIBUTE_LABEL, "s", &str);
        const auto now = m_state->clock->localtime();
        EXPECT_EQ(now.format("%A, %e %B %Y"), str);
      
        g_clear_pointer(&str, g_free);

        g_menu_model_get_item_attribute(section, 0, G_MENU_ATTRIBUTE_ACTION, "s", &str);
        if (expected_action != nullptr)
            EXPECT_STREQ(expected_action, str);
        else
            EXPECT_TRUE(str == nullptr);
        g_clear_pointer(&str, g_free);

        // look at the calendar menuitem
        if (calendar_expected)
        {
            g_menu_model_get_item_attribute(section, 1, "x-canonical-type", "s", &str);
            EXPECT_STREQ("com.canonical.indicator.calendar", str);
            g_clear_pointer(&str, g_free);

            g_menu_model_get_item_attribute(section, 1, G_MENU_ATTRIBUTE_ACTION, "s", &str);
            EXPECT_STREQ("indicator.calendar", str);
            g_clear_pointer(&str, g_free);

            g_menu_model_get_item_attribute(section, 1, "activation-action", "s", &str);
            if (expected_action != nullptr)
                EXPECT_STREQ(expected_action, str);
            else
                EXPECT_TRUE(str == nullptr);
            g_clear_pointer(&str, g_free);
        }

        g_clear_object(&section);

        // now change the clock and see if the date label changes appropriately

        auto gdt_tomorrow = g_date_time_add_days(now.get(), 1);
        auto tomorrow = DateTime(gdt_tomorrow);
        g_date_time_unref(gdt_tomorrow);
        m_mock_state->mock_clock->set_localtime(tomorrow);
        wait_msec();

        section = g_menu_model_get_item_link(submenu, Menu::Calendar, G_MENU_LINK_SECTION);
        g_menu_model_get_item_attribute(section, 0, G_MENU_ATTRIBUTE_LABEL, "s", &str);
        EXPECT_EQ(tomorrow.format("%A, %e %B %Y"), str);
        g_clear_pointer(&str, g_free);
        g_clear_object(&section);

        // cleanup
        g_object_unref(submenu);
    }

private:

    void InspectEmptySection(GMenuModel* menu_model, Menu::Section section)
    {
        // get the Appointments section
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);
        auto menu_section = g_menu_model_get_item_link(submenu, section, G_MENU_LINK_SECTION);
        EXPECT_EQ(0, g_menu_model_get_n_items(menu_section));
        g_clear_object(&menu_section);
        g_clear_object(&submenu);
    }

    std::vector<Appointment> build_some_appointments()
    {
        const auto now = m_state->clock->localtime();
        auto gdt_tomorrow = g_date_time_add_days(now.get(), 1);
        const auto tomorrow = DateTime(gdt_tomorrow);
        g_date_time_unref(gdt_tomorrow);

        Appointment a1; // an alarm clock appointment
        a1.color = "red";
        a1.summary = "Alarm";
        a1.summary = "http://www.example.com/";
        a1.uid = "example";
        a1.has_alarms = true;
        a1.begin = a1.end = tomorrow;

        Appointment a2; // a non-alarm appointment
        a2.color = "green";
        a2.summary = "Other Text";
        a2.summary = "http://www.monkey.com/";
        a2.uid = "monkey";
        a2.has_alarms = false;
        a2.begin = a2.end = tomorrow;

        return std::vector<Appointment>({a1, a2});
    }

    void InspectAppointmentMenuItem(GMenuModel*         section,
                                    int                 index,
                                    const Appointment&  appt)
    {
        //  confirm it has the right x-canonical-type
        gchar * str = nullptr;
        g_menu_model_get_item_attribute(section, index, "x-canonical-type", "s", &str);
        if (appt.has_alarms)
            EXPECT_STREQ("com.canonical.indicator.alarm", str);
        else
            EXPECT_STREQ("com.canonical.indicator.appointment", str);
        g_clear_pointer(&str, g_free);

        // confirm it has a nonempty x-canonical-time-format
        g_menu_model_get_item_attribute(section, index, "x-canonical-time-format", "s", &str);
        EXPECT_TRUE(str && *str);
        g_clear_pointer(&str, g_free);

        // confirm the color hint, if it exists,
        // is in the x-canonical-color attribute
        if (appt.color.empty())
        {
            EXPECT_FALSE(g_menu_model_get_item_attribute(section,
                                                         index,
                                                         "x-canonical-color",
                                                         "s",
                                                         &str));
        }
        else
        {
            EXPECT_TRUE(g_menu_model_get_item_attribute(section,
                                                        index,
                                                        "x-canonical-color",
                                                        "s",
                                                        &str));
            EXPECT_EQ(appt.color, str);
        }
        g_clear_pointer(&str, g_free);

        // confirm that alarms have an icon
        if (appt.has_alarms)
        {
            auto v = g_menu_model_get_item_attribute_value(section,
                                                           index,
                                                           G_MENU_ATTRIBUTE_ICON,
                                                           nullptr);
            EXPECT_TRUE(v != nullptr);
            auto icon = g_icon_deserialize(v);
            EXPECT_TRUE(icon != nullptr);
            g_clear_object(&icon);
            g_clear_pointer(&v, g_variant_unref);
        }
    }

    void InspectAppointmentMenuItems(GMenuModel* section,
                                     int first_appt_index,
                                     const std::vector<Appointment>& appointments,
                                     bool can_open_planner)
    {
        // try adding a few appointments and see if the menu updates itself
        m_state->calendar_upcoming->appointments().set(appointments);
        wait_msec(); // wait a moment for the menu to update

        //auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);
        //auto section = g_menu_model_get_item_link(submenu, Menu::Appointments, G_MENU_LINK_SECTION);
        const int n_add_event_buttons = can_open_planner ? 1 : 0;
        EXPECT_EQ(n_add_event_buttons + appointments.size(), g_menu_model_get_n_items(section));

        for (int i=0, n=appointments.size(); i<n; i++)
            InspectAppointmentMenuItem(section, first_appt_index+i, appointments[i]);

        //g_clear_object(&section);
        //g_clear_object(&submenu);
    }

    void InspectDesktopAppointments(GMenuModel* menu_model, bool can_open_planner)
    {
        const int n_add_event_buttons = can_open_planner ? 1 : 0;

        // get the Appointments section
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);

        // there shouldn't be any menuitems when "show events" is false
        m_state->settings->show_events.set(false);
        wait_msec();
        auto section = g_menu_model_get_item_link(submenu, Menu::Appointments, G_MENU_LINK_SECTION);
        EXPECT_EQ(0, g_menu_model_get_n_items(section));
        g_clear_object(&section);

        std::vector<Appointment> appointments;
        m_state->settings->show_events.set(true);
        m_state->calendar_upcoming->appointments().set(appointments);
        wait_msec();
        section = g_menu_model_get_item_link(submenu, Menu::Appointments, G_MENU_LINK_SECTION);
        EXPECT_EQ(n_add_event_buttons, g_menu_model_get_n_items(section));
        if (can_open_planner)
        {
            // when "show_events" is true,
            // there should be an "add event" button even if there aren't any appointments
            gchar* action = nullptr;
            EXPECT_TRUE(g_menu_model_get_item_attribute(section, 0, G_MENU_ATTRIBUTE_ACTION, "s", &action));
            const char* expected_action = "desktop.open-calendar-app";
            EXPECT_EQ(std::string("indicator.")+expected_action, action);
            EXPECT_TRUE(g_action_group_has_action(m_actions->action_group(), expected_action));
            g_free(action);
        }
        g_clear_object(&section);

        // try adding a few appointments and see if the menu updates itself
        appointments = build_some_appointments();
        m_state->calendar_upcoming->appointments().set(appointments);
        wait_msec(); // wait a moment for the menu to update
        section = g_menu_model_get_item_link(submenu, Menu::Appointments, G_MENU_LINK_SECTION);
        EXPECT_EQ(n_add_event_buttons + 2, g_menu_model_get_n_items(section));
        InspectAppointmentMenuItems(section, 0, appointments, can_open_planner);
        g_clear_object(&section);

        // cleanup
        g_clear_object(&submenu);
    }

    void InspectPhoneAppointments(GMenuModel* menu_model, bool can_open_planner)
    {
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);

        // clear all the appointments
        std::vector<Appointment> appointments;
        m_state->calendar_upcoming->appointments().set(appointments);
        wait_msec(); // wait a moment for the menu to update

        // check that there's a "clock app" menuitem even when there are no appointments
        auto section = g_menu_model_get_item_link(submenu, Menu::Appointments, G_MENU_LINK_SECTION);
        const char* expected_action = "phone.open-alarm-app";
        EXPECT_EQ(1, g_menu_model_get_n_items(section));
        gchar* action = nullptr;
        EXPECT_TRUE(g_menu_model_get_item_attribute(section, 0, G_MENU_ATTRIBUTE_ACTION, "s", &action));
        EXPECT_EQ(std::string("indicator.")+expected_action, action);
        EXPECT_TRUE(g_action_group_has_action(m_actions->action_group(), expected_action));
        g_free(action);
        g_clear_object(&section);

        // add some appointments and test them
        appointments = build_some_appointments();
        m_state->calendar_upcoming->appointments().set(appointments);
        wait_msec(); // wait a moment for the menu to update
        section = g_menu_model_get_item_link(submenu, Menu::Appointments, G_MENU_LINK_SECTION);
        EXPECT_EQ(3, g_menu_model_get_n_items(section));
        InspectAppointmentMenuItems(section, 1, appointments, can_open_planner);
        g_clear_object(&section);

        // cleanup
        g_clear_object(&submenu);
    }

protected:

    void InspectAppointments(GMenuModel* menu_model, Menu::Profile profile)
    {
        const auto can_open_planner = m_actions->desktop_has_calendar_app();

        switch (profile)
        {
            case Menu::Desktop:
                InspectDesktopAppointments(menu_model, can_open_planner);
                break;

            case Menu::DesktopGreeter:
                InspectEmptySection(menu_model, Menu::Appointments);
                break;

            case Menu::Phone:
                InspectPhoneAppointments(menu_model, can_open_planner);
                break;

            case Menu::PhoneGreeter:
                InspectEmptySection(menu_model, Menu::Appointments);
                break;

            default:
                g_warn_if_reached();
                break;
        }
    }

    void CompareLocationsTo(GMenuModel* menu_model, const std::vector<Location>& locations)
    {
        // get the Locations section
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);
        auto section = g_menu_model_get_item_link(submenu, Menu::Locations, G_MENU_LINK_SECTION);

        // confirm that section's menuitems mirror the "locations" vector
        const auto n = locations.size();
        ASSERT_EQ(n, g_menu_model_get_n_items(section));
        for (guint i=0; i<n; i++)
        {
            gchar* str = nullptr;

            // confirm that the x-canonical-type is right
            g_menu_model_get_item_attribute(section, i, "x-canonical-type", "s", &str);
            EXPECT_STREQ("com.canonical.indicator.location", str);
            g_clear_pointer(&str, g_free);

            // confirm that the timezones match the ones in the vector
            g_menu_model_get_item_attribute(section, i, "x-canonical-timezone", "s", &str);
            EXPECT_EQ(locations[i].zone(), str);
            g_clear_pointer(&str, g_free);

            // confirm that x-canonical-time-format has some kind of time format string
            g_menu_model_get_item_attribute(section, i, "x-canonical-time-format", "s", &str);
            EXPECT_TRUE(str && *str && (strchr(str,'%')!=nullptr));
            g_clear_pointer(&str, g_free);
        }

        g_clear_object(&section);
        g_clear_object(&submenu);
    }

    void InspectLocations(GMenuModel* menu_model, Menu::Profile profile)
    {
        const bool locations_expected = profile == Menu::Desktop;

        // when there aren't any locations, confirm the menu is empty
        const std::vector<Location> empty;
        m_state->locations->locations.set(empty);
        wait_msec();
        CompareLocationsTo(menu_model, empty);

        // add some locations and confirm the menu picked up our changes
        Location l1 ("America/Chicago", "Dallas");
        Location l2 ("America/Arizona", "Phoenix");
        std::vector<Location> locations({l1, l2});
        m_state->locations->locations.set(locations);
        wait_msec();
        CompareLocationsTo(menu_model, locations_expected ? locations : empty);

        // now remove one of the locations...
        locations.pop_back();
        m_state->locations->locations.set(locations);
        wait_msec();
        CompareLocationsTo(menu_model, locations_expected ? locations : empty);
    }

    void InspectSettings(GMenuModel* menu_model, Menu::Profile profile)
    {
        std::string expected_action;

        if (profile == Menu::Desktop)
            expected_action = "indicator.desktop.open-settings-app";
        else if (profile == Menu::Phone)
            expected_action = "indicator.phone.open-settings-app";

        // get the Settings section
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);
        auto section = g_menu_model_get_item_link(submenu, Menu::Settings, G_MENU_LINK_SECTION);

        if (expected_action.empty())
        {
            EXPECT_EQ(0, g_menu_model_get_n_items(section));
        }
        else
        {
            EXPECT_EQ(1, g_menu_model_get_n_items(section));
            gchar* str = nullptr;
            g_menu_model_get_item_attribute(section, 0, G_MENU_ATTRIBUTE_ACTION, "s", &str);
            EXPECT_EQ(expected_action, str);
            g_clear_pointer(&str, g_free);
        }

        g_clear_object(&section);
        g_object_unref(submenu);
    }
};


TEST_F(MenuFixture, HelloWorld)
{
    EXPECT_EQ(Menu::NUM_PROFILES, m_menus.size());
    for (int i=0; i<Menu::NUM_PROFILES; i++)
    {
        EXPECT_TRUE(m_menus[i] != false);
        EXPECT_TRUE(m_menus[i]->menu_model() != nullptr);
        EXPECT_EQ(i, m_menus[i]->profile());
    }
    EXPECT_EQ(m_menus[Menu::Desktop]->name(), "desktop");
}

TEST_F(MenuFixture, Header)
{
    for(auto& menu : m_menus)
      InspectHeader(menu->menu_model(), menu->name());
}

TEST_F(MenuFixture, Sections)
{
    for(auto& menu : m_menus)
    {
        // check that the header has a submenu
        auto menu_model = menu->menu_model();
        auto submenu = g_menu_model_get_item_link(menu_model, 0, G_MENU_LINK_SUBMENU);
        EXPECT_TRUE(submenu != nullptr);
        EXPECT_EQ(Menu::NUM_SECTIONS, g_menu_model_get_n_items(submenu));
        g_object_unref(submenu);
    }
}

TEST_F(MenuFixture, Calendar)
{
    m_state->settings->show_calendar.set(true);
    for(auto& menu : m_menus)
      InspectCalendar(menu->menu_model(), menu->profile());

    m_state->settings->show_calendar.set(false);
    for(auto& menu : m_menus)
      InspectCalendar(menu->menu_model(), menu->profile());
}

TEST_F(MenuFixture, Appointments)
{
    for(auto& menu : m_menus)
      InspectAppointments(menu->menu_model(), menu->profile());

    // toggle can_open_planner() and test the desktop again
    // to confirm that the "Add Event…" menuitem appears iff
    // there's a calendar available user-agent
    m_mock_actions->set_desktop_has_calendar_app (!m_actions->desktop_has_calendar_app());
    std::shared_ptr<Menu> menu = m_menu_factory->buildMenu(Menu::Desktop);
    InspectAppointments(menu->menu_model(), menu->profile());
}

TEST_F(MenuFixture, Locations)
{
    for(auto& menu : m_menus)
      InspectLocations(menu->menu_model(), menu->profile());
}

TEST_F(MenuFixture, Settings)
{
    for(auto& menu : m_menus)
      InspectSettings(menu->menu_model(), menu->profile());
}


