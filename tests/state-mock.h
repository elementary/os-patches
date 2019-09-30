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

#include "planner-mock.h"

#include <datetime/clock-mock.h>
#include <datetime/state.h>

using namespace unity::indicator::datetime;

class MockState: public State
{
public:
    std::shared_ptr<MockClock> mock_clock;
    std::shared_ptr<MockRangePlanner> mock_range_planner;

    MockState()
    {
        const DateTime now = DateTime::NowLocal();
        mock_clock.reset(new MockClock(now));
        clock = std::dynamic_pointer_cast<Clock>(mock_clock);

        settings.reset(new Settings);

        mock_range_planner.reset(new MockRangePlanner);
        auto range_planner = std::dynamic_pointer_cast<RangePlanner>(mock_range_planner);
        calendar_month.reset(new MonthPlanner(range_planner, now));
        calendar_upcoming.reset(new UpcomingPlanner(range_planner, now));

        locations.reset(new Locations);
    }
};

