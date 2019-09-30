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

#ifndef INDICATOR_DATETIME_PLANNER_UPCOMING_H
#define INDICATOR_DATETIME_PLANNER_UPCOMING_H

#include <datetime/planner.h>

#include <datetime/date-time.h>
#include <datetime/planner-range.h>

#include <memory> // std::shared_ptr

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A collection of upcoming appointments starting from the specified date
 */
class UpcomingPlanner: public Planner
{
public:
    UpcomingPlanner(const std::shared_ptr<RangePlanner>& range_planner,
                    const DateTime& date);
    ~UpcomingPlanner() =default;

    core::Property<std::vector<Appointment>>& appointments();
    core::Property<DateTime>& date();

private:
    std::shared_ptr<RangePlanner> m_range_planner;
    core::Property<DateTime> m_date;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_PLANNER_UPCOMING_H
