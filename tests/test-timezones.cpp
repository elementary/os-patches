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

#include "geoclue-fixture.h"

#include <datetime/settings.h>
#include <datetime/timezones-live.h>

#include <memory> // std::shared_ptr

#include <cstdio> // fopen()
#include <unistd.h> // sync()

using namespace unity::indicator::datetime;

typedef GeoclueFixture TimezonesFixture;

#define TIMEZONE_FILE (SANDBOX "/timezone")

namespace
{
    /* convenience func to set the timezone file */
    void set_file(const std::string& text)
    {
        auto fp = fopen(TIMEZONE_FILE, "w+");
        fprintf(fp, "%s\n", text.c_str());
        fclose(fp);
        sync();
    }
}


TEST_F(TimezonesFixture, ManagerTest)
{
  std::string timezone_file = "America/New_York";
  std::string timezone_geo = "America/Denver";

  set_file(timezone_file);
  std::shared_ptr<Settings> settings(new Settings);
  LiveTimezones z(settings, TIMEZONE_FILE);
  wait_msec(500); // wait for the bus to get set up
  EXPECT_EQ(timezone_file, z.timezone.get());
  auto zones = z.timezones.get();
  //std::set<std::string> zones = z.timezones.get();
  EXPECT_EQ(1, zones.size());
  EXPECT_EQ(1, zones.count(timezone_file));

  bool zone_changed = false;
  auto zone_connection = z.timezone.changed().connect([&zone_changed, this](const std::string&) {
          zone_changed = true;
          g_main_loop_quit(loop);
        });

  // start listening for a timezone change, then change the timezone
  bool zones_changed = false;
  auto zones_connection = z.timezones.changed().connect([&zones_changed, &zones, this](const std::set<std::string>& timezones) {
          zones_changed = true;
          zones = timezones;
          g_main_loop_quit(loop);
        });

  g_idle_add([](gpointer s_in) {
          auto s = static_cast<Settings*>(s_in);
          g_message("geolocation was %d", (int)s->show_detected_location.get());
          g_message("turning geolocation on");
          s->show_detected_location.set(true);
          return G_SOURCE_REMOVE;
        }, settings.get());

  // turn on geoclue during the idle... this should add timezone_1 to the 'timezones' property
  g_main_loop_run(loop);
  EXPECT_TRUE(zones_changed);
  EXPECT_EQ(timezone_file, z.timezone.get());
  EXPECT_EQ(2, zones.size());
  EXPECT_EQ(1, zones.count(timezone_file));
  EXPECT_EQ(1, zones.count(timezone_geo));
  zones_changed = false;

  // now tweak the geoclue value... the geoclue-detected timezone should change,
  // causing the 'timezones' property to change
  zone_changed = false;
  zones_changed = false;
  timezone_geo = "America/Chicago";
  setGeoclueTimezoneOnIdle(timezone_geo);
  g_main_loop_run(loop);
  EXPECT_FALSE(zone_changed);
  EXPECT_TRUE(zones_changed);
  EXPECT_EQ(timezone_file, z.timezone.get());
  EXPECT_EQ(2, zones.size());
  EXPECT_EQ(1, zones.count(timezone_file));
  EXPECT_EQ(1, zones.count(timezone_geo));

  // now set the file value... this should change both the primary property and set property
  zone_changed = false;
  zones_changed = false;
  timezone_file = "America/Los_Angeles";
  EXPECT_EQ(0, zones.count(timezone_file));
  g_idle_add([](gpointer str) {set_file(static_cast<const char*>(str)); return G_SOURCE_REMOVE;}, const_cast<char*>(timezone_file.c_str()));
  g_main_loop_run(loop);
  EXPECT_TRUE(zone_changed);
  EXPECT_TRUE(zones_changed);
  EXPECT_EQ(timezone_file, z.timezone.get());
  EXPECT_EQ(2, zones.size());
  EXPECT_EQ(1, zones.count(timezone_file));
  EXPECT_EQ(1, zones.count(timezone_geo));
}


