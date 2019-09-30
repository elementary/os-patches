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

#include "geoclue-fixture.h"

#include <datetime/timezone-geoclue.h>

using unity::indicator::datetime::GeoclueTimezone;

// This test looks small because the interesting
// work is all happening in GeoclueFixture...
TEST_F(GeoclueFixture, ChangeDetected)
{
  GeoclueTimezone tz;
  wait_msec(500); // wait for the bus to get set up
  EXPECT_EQ(timezone_1, tz.timezone.get());

  // Start listening for a timezone change, then change the timezone.

  bool changed = false;
  auto connection = tz.timezone.changed().connect(
        [&changed, this](const std::string& s){
          g_debug("timezone changed to %s", s.c_str());
          changed = true;
          g_main_loop_quit(loop);
        });

  const std::string timezone_2 = "America/Chicago";
  setGeoclueTimezoneOnIdle(timezone_2);
  g_main_loop_run(loop);
  EXPECT_EQ(timezone_2, tz.timezone.get());
}
