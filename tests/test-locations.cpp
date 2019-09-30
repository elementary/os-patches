

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

#include <datetime/locations-settings.h>

using unity::indicator::datetime::Location;
using unity::indicator::datetime::Locations;
using unity::indicator::datetime::Settings;
using unity::indicator::datetime::SettingsLocations;
using unity::indicator::datetime::Timezones;

/***
****
***/

class LocationsFixture: public GlibFixture
{
  private:

    typedef GlibFixture super;

  protected:

    //GSettings * settings = nullptr;
    std::shared_ptr<Settings> m_settings;
    std::shared_ptr<Timezones> m_timezones;
    const std::string nyc = "America/New_York";
    const std::string chicago = "America/Chicago";

    virtual void SetUp()
    {
      super::SetUp();

      m_settings.reset(new Settings);
      m_settings->show_locations.set(true);
      m_settings->locations.set({"America/Los_Angeles Oakland",
                                 "America/Chicago Chicago",
                                 "America/Chicago Oklahoma City",
                                 "America/Toronto Toronto",
                                 "Europe/London London",
                                 "Europe/Berlin Berlin"});

      m_timezones.reset(new Timezones);
      m_timezones->timezone.set(chicago);
      m_timezones->timezones.set(std::set<std::string>({ nyc, chicago }));
    }

    virtual void TearDown()
    {
      m_timezones.reset();
      m_settings.reset();

      super::TearDown();
    }
};

TEST_F(LocationsFixture, Timezones)
{
    m_settings->show_locations.set(false);

    SettingsLocations locations(m_settings, m_timezones);
    const auto l = locations.locations.get();
    EXPECT_EQ(2, l.size());
    EXPECT_STREQ("Chicago", l[0].name().c_str());
    EXPECT_EQ(chicago, l[0].zone());
    EXPECT_EQ("New York", l[1].name());
    EXPECT_EQ(nyc, l[1].zone());
}

TEST_F(LocationsFixture, SettingsLocations)
{
    SettingsLocations locations(m_settings, m_timezones);

    const auto l = locations.locations.get();
    EXPECT_EQ(7, l.size());
    EXPECT_EQ("Chicago", l[0].name());
    EXPECT_EQ(chicago, l[0].zone());
    EXPECT_EQ("New York", l[1].name());
    EXPECT_EQ(nyc, l[1].zone());
    EXPECT_EQ("Oakland", l[2].name());
    EXPECT_EQ("America/Los_Angeles", l[2].zone());
    EXPECT_EQ("Oklahoma City", l[3].name());
    EXPECT_EQ("America/Chicago", l[3].zone());
    EXPECT_EQ("Toronto", l[4].name());
    EXPECT_EQ("America/Toronto", l[4].zone());
    EXPECT_EQ("London", l[5].name());
    EXPECT_EQ("Europe/London", l[5].zone());
    EXPECT_EQ("Berlin", l[6].name());
    EXPECT_EQ("Europe/Berlin", l[6].zone());
}

TEST_F(LocationsFixture, ChangeLocationStrings)
{
    SettingsLocations locations(m_settings, m_timezones);

    bool locations_changed = false;
    locations.locations.changed().connect([&locations_changed, this](const std::vector<Location>&){
                    locations_changed = true;
                    g_main_loop_quit(loop);
                });

    g_idle_add([](gpointer settings){
                    static_cast<Settings*>(settings)->locations.set({"America/Los_Angeles Oakland", "Europe/London London", "Europe/Berlin Berlin"});
                    return G_SOURCE_REMOVE;
                }, m_settings.get());

    g_main_loop_run(loop);

    EXPECT_TRUE(locations_changed);
    const auto l = locations.locations.get();
    EXPECT_EQ(5, l.size());
    EXPECT_EQ("Chicago", l[0].name());
    EXPECT_EQ(chicago, l[0].zone());
    EXPECT_EQ("New York", l[1].name());
    EXPECT_EQ(nyc, l[1].zone());
    EXPECT_EQ("Oakland", l[2].name());
    EXPECT_EQ("America/Los_Angeles", l[2].zone());
    EXPECT_EQ("London", l[3].name());
    EXPECT_EQ("Europe/London", l[3].zone());
    EXPECT_EQ("Berlin", l[4].name());
    EXPECT_EQ("Europe/Berlin", l[4].zone());
    locations_changed = false;
}

TEST_F(LocationsFixture, ChangeLocationVisibility)
{
    SettingsLocations locations(m_settings, m_timezones);

    bool locations_changed = false;
    locations.locations.changed().connect([&locations_changed, this](const std::vector<Location>&){
                    locations_changed = true;
                    g_main_loop_quit(loop);
                });

    g_idle_add([](gpointer settings){
                    static_cast<Settings*>(settings)->show_locations.set(false);
                    return G_SOURCE_REMOVE;
                }, m_settings.get());

    g_main_loop_run(loop);

    EXPECT_TRUE(locations_changed);
    const auto l = locations.locations.get();
    EXPECT_EQ(2, l.size());
    EXPECT_EQ("Chicago", l[0].name());
    EXPECT_EQ(chicago, l[0].zone());
    EXPECT_EQ("New York", l[1].name());
    EXPECT_EQ(nyc, l[1].zone());
}
