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

#include <datetime/planner-upcoming.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

UpcomingPlanner::UpcomingPlanner(const std::shared_ptr<RangePlanner>& range_planner,
                                 const DateTime& date_in):
    m_range_planner(range_planner)
{
    date().changed().connect([this](const DateTime& dt){
        // set the range to the upcoming month
        const auto b = dt.add_full(0, 0, -1, -dt.hour(), -dt.minute(), -dt.seconds());
        const auto e = b.add_full(0, 1, 0, 0, 0, 0);
        g_debug("%p setting date range to [%s..%s]", this, b.format("%F %T").c_str(), e.format("%F %T").c_str());
        m_range_planner->range().set(std::pair<DateTime,DateTime>(b,e));
    });

    date().set(date_in);
}

core::Property<DateTime>& UpcomingPlanner::date()
{
    return m_date;
}

core::Property<std::vector<Appointment>>& UpcomingPlanner::appointments()
{
    return m_range_planner->appointments();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
