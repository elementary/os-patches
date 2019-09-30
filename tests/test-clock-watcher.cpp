/*
 * Copyright 2014 Canonical Ltd.
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

#include <datetime/clock-watcher.h>

#include <gtest/gtest.h>

#include "state-fixture.h"

using namespace unity::indicator::datetime;

class ClockWatcherFixture: public StateFixture
{
private:

    typedef StateFixture super;

protected:

    std::vector<std::string> m_triggered;
    std::unique_ptr<ClockWatcher> m_watcher;
    std::shared_ptr<RangePlanner> m_range_planner;
    std::shared_ptr<UpcomingPlanner> m_upcoming;

    void SetUp()
    {
        super::SetUp();

        m_range_planner.reset(new MockRangePlanner);
        m_upcoming.reset(new UpcomingPlanner(m_range_planner, m_state->clock->localtime()));
        m_watcher.reset(new ClockWatcherImpl(m_state->clock, m_upcoming));
        m_watcher->alarm_reached().connect([this](const Appointment& appt){
            m_triggered.push_back(appt.uid);
        });

        EXPECT_TRUE(m_triggered.empty());
    }

    void TearDown()
    {
        m_triggered.clear();
        m_watcher.reset();
        m_upcoming.reset();
        m_range_planner.reset();

        super::TearDown();
    }

    std::vector<Appointment> build_some_appointments()
    {
        const auto now = m_state->clock->localtime();
        auto tomorrow = g_date_time_add_days (now.get(), 1);
        auto tomorrow_begin = g_date_time_add_full (tomorrow, 0, 0, 0,
                                                    -g_date_time_get_hour(tomorrow),
                                                    -g_date_time_get_minute(tomorrow),
                                                    -g_date_time_get_seconds(tomorrow));
        auto tomorrow_end = g_date_time_add_full (tomorrow_begin, 0, 0, 1, 0, 0, -1);

        Appointment a1; // an alarm clock appointment
        a1.color = "red";
        a1.summary = "Alarm";
        a1.summary = "http://www.example.com/";
        a1.uid = "example";
        a1.has_alarms = true;
        a1.begin = tomorrow_begin;
        a1.end = tomorrow_end;

        auto ubermorgen_begin = g_date_time_add_days (tomorrow, 1);
        auto ubermorgen_end = g_date_time_add_full (tomorrow_begin, 0, 0, 1, 0, 0, -1);

        Appointment a2; // a non-alarm appointment
        a2.color = "green";
        a2.summary = "Other Text";
        a2.summary = "http://www.monkey.com/";
        a2.uid = "monkey";
        a2.has_alarms = false;
        a2.begin = ubermorgen_begin;
        a2.end = ubermorgen_end;

        // cleanup
        g_date_time_unref(ubermorgen_end);
        g_date_time_unref(ubermorgen_begin);
        g_date_time_unref(tomorrow_end);
        g_date_time_unref(tomorrow_begin);
        g_date_time_unref(tomorrow);

        return std::vector<Appointment>({a1, a2});
    }
};

/***
****
***/

TEST_F(ClockWatcherFixture, AppointmentsChanged)
{
    // Add some appointments to the planner.
    // One of these matches our state's localtime, so that should get triggered.
    std::vector<Appointment> a = build_some_appointments();
    a[0].begin = m_state->clock->localtime();
    m_range_planner->appointments().set(a);

    // Confirm that it got fired
    EXPECT_EQ(1, m_triggered.size());
    EXPECT_EQ(a[0].uid, m_triggered[0]);
}


TEST_F(ClockWatcherFixture, TimeChanged)
{
    // Add some appointments to the planner.
    // Neither of these match the state's localtime, so nothing should be triggered.
    std::vector<Appointment> a = build_some_appointments();
    m_range_planner->appointments().set(a);
    EXPECT_TRUE(m_triggered.empty());

    // Set the state's clock to a time that matches one of the appointments().
    // That appointment should get triggered.
    m_mock_state->mock_clock->set_localtime(a[1].begin);
    EXPECT_EQ(1, m_triggered.size());
    EXPECT_EQ(a[1].uid, m_triggered[0]);
}


TEST_F(ClockWatcherFixture, MoreThanOne)
{
    const auto now = m_state->clock->localtime();
    std::vector<Appointment> a = build_some_appointments();
    a[0].begin = a[1].begin = now;
    m_range_planner->appointments().set(a);

    EXPECT_EQ(2, m_triggered.size());
    EXPECT_EQ(a[0].uid, m_triggered[0]);
    EXPECT_EQ(a[1].uid, m_triggered[1]);
}


TEST_F(ClockWatcherFixture, NoDuplicates)
{
    // Setup: add an appointment that gets triggered.
    const auto now = m_state->clock->localtime();
    const std::vector<Appointment> appointments = build_some_appointments();
    std::vector<Appointment> a;
    a.push_back(appointments[0]);
    a[0].begin = now;
    m_range_planner->appointments().set(a);
    EXPECT_EQ(1, m_triggered.size());
    EXPECT_EQ(a[0].uid, m_triggered[0]);

    // Now change the appointment vector by adding one to it.
    // Confirm that the ClockWatcher doesn't re-trigger a[0]
    a.push_back(appointments[1]);
    m_range_planner->appointments().set(a);
    EXPECT_EQ(1, m_triggered.size());
    EXPECT_EQ(a[0].uid, m_triggered[0]);
}
