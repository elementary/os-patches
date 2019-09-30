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

#include <datetime/clock.h>
#include <datetime/timezones.h>

#include "test-dbus-fixture.h"

/***
****
***/

using namespace unity::indicator::datetime;

class ClockFixture: public TestDBusFixture
{
  private:
    typedef TestDBusFixture super;

  public:
    void emitPrepareForSleep()
    {
      g_dbus_connection_emit_signal(g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr),
                                    nullptr,
                                    "/org/freedesktop/login1", // object path
                                    "org.freedesktop.login1.Manager", // interface
                                    "PrepareForSleep", // signal name
                                    g_variant_new("(b)", FALSE),
                                    nullptr);
    }
};

TEST_F(ClockFixture, MinuteChangedSignalShouldTriggerOncePerMinute)
{
    // start up a live clock
    std::shared_ptr<Timezones> zones(new Timezones);
    zones->timezone.set("America/New_York");
    LiveClock clock(zones);
    wait_msec(500); // wait for the bus to set up

    // count how many times clock.minute_changed() is emitted over the next minute
    const DateTime now = clock.localtime();
    const auto gnow = now.get();
    auto gthen = g_date_time_add_minutes(gnow, 1);
    int count = 0;
    clock.minute_changed.connect([&count](){count++;});
    const auto msec = g_date_time_difference(gthen,gnow) / 1000;
    wait_msec(msec);
    EXPECT_EQ(1, count);
    g_date_time_unref(gthen);
}

/***
****
***/

#define TIMEZONE_FILE (SANDBOX"/timezone")

TEST_F(ClockFixture, HelloFixture)
{
    std::shared_ptr<Timezones> zones(new Timezones);
    zones->timezone.set("America/New_York");
    LiveClock clock(zones);
}


TEST_F(ClockFixture, TimezoneChangeTriggersSkew)
{
    std::shared_ptr<Timezones> zones(new Timezones);
    zones->timezone.set("America/New_York");
    LiveClock clock(zones);

    auto tz_nyc = g_time_zone_new("America/New_York");
    auto now_nyc = g_date_time_new_now(tz_nyc);
    auto now = clock.localtime();
    EXPECT_EQ(g_date_time_get_utc_offset(now_nyc), g_date_time_get_utc_offset(now.get()));
    EXPECT_LE(abs(g_date_time_difference(now_nyc,now.get())), G_USEC_PER_SEC);
    g_date_time_unref(now_nyc);
    g_time_zone_unref(tz_nyc);

    /// change the timezones!
    clock.minute_changed.connect([this](){
                   g_main_loop_quit(loop);
               });
    g_idle_add([](gpointer gs){
                   static_cast<Timezones*>(gs)->timezone.set("America/Los_Angeles");
                   return G_SOURCE_REMOVE;
               }, zones.get());
    g_main_loop_run(loop);

    auto tz_la = g_time_zone_new("America/Los_Angeles");
    auto now_la = g_date_time_new_now(tz_la);
    now = clock.localtime();
    EXPECT_EQ(g_date_time_get_utc_offset(now_la), g_date_time_get_utc_offset(now.get()));
    EXPECT_LE(abs(g_date_time_difference(now_la,now.get())), G_USEC_PER_SEC);
    g_date_time_unref(now_la);
    g_time_zone_unref(tz_la);
}

/**
 * Confirm that a "PrepareForSleep" event wil trigger a skew event
 */
TEST_F(ClockFixture, SleepTriggersSkew)
{
    std::shared_ptr<Timezones> zones(new Timezones);
    zones->timezone.set("America/New_York");
    LiveClock clock(zones);
    wait_msec(500); // wait for the bus to set up

    bool skewed = false;
    clock.minute_changed.connect([&skewed, this](){
                    skewed = true;
                    g_main_loop_quit(loop);
                    return G_SOURCE_REMOVE;
                });

    g_idle_add([](gpointer gself){
                   static_cast<ClockFixture*>(gself)->emitPrepareForSleep();
                   return G_SOURCE_REMOVE;
                }, this);

    g_main_loop_run(loop);
    EXPECT_TRUE(skewed);
}
