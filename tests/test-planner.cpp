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
#include "timezone-mock.h"

#include <datetime/appointment.h>
#include <datetime/clock-mock.h>
#include <datetime/date-time.h>
#include <datetime/planner.h>
#include <datetime/planner-range.h>

#include <langinfo.h>
#include <locale.h>

using namespace unity::indicator::datetime;

/***
****
***/

typedef GlibFixture PlannerFixture;

TEST_F(PlannerFixture, HelloWorld)
{
    auto halloween = g_date_time_new_local(2020, 10, 31, 18, 30, 59);
    auto christmas = g_date_time_new_local(2020, 12, 25,  0,  0,  0);

    Appointment a;
    a.summary = "Test";
    a.begin = halloween;
    a.end = g_date_time_add_hours(halloween, 1);
    const Appointment b = a;
    a.summary = "Foo";

    EXPECT_EQ(a.summary, "Foo");
    EXPECT_EQ(b.summary, "Test");
    EXPECT_EQ(0, g_date_time_compare(a.begin(), b.begin()));
    EXPECT_EQ(0, g_date_time_compare(a.end(), b.end()));

    Appointment c;
    c.begin = christmas;
    c.end = g_date_time_add_hours(christmas, 1);
    Appointment d;
    d = c;
    EXPECT_EQ(0, g_date_time_compare(c.begin(), d.begin()));
    EXPECT_EQ(0, g_date_time_compare(c.end(), d.end()));
    a = d;
    EXPECT_EQ(0, g_date_time_compare(d.begin(), a.begin()));
    EXPECT_EQ(0, g_date_time_compare(d.end(), a.end()));
}

