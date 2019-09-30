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

#include <datetime/planner-month.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

MonthPlanner::MonthPlanner(const std::shared_ptr<RangePlanner>& range_planner,
                           const DateTime& month_in):
    m_range_planner(range_planner)
{
    month().changed().connect([this](const DateTime& m){
        auto month_begin = m.add_full(0, // no years
                                      0, // no months
                                      -(m.day_of_month()-1),
                                      -m.hour(),
                                      -m.minute(),
                                      -m.seconds());
        auto month_end = month_begin.add_full(0, 1, 0, 0, 0, -0.1);
        g_debug("PlannerMonth %p setting calendar month range: [%s..%s]", this, month_begin.format("%F %T").c_str(), month_end.format("%F %T").c_str());
        m_range_planner->range().set(std::pair<DateTime,DateTime>(month_begin,month_end));
    });

    month().set(month_in);
}

core::Property<DateTime>& MonthPlanner::month()
{
    return m_month;
}

core::Property<std::vector<Appointment>>& MonthPlanner::appointments()
{
    return m_range_planner->appointments();
}


/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
