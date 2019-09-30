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

#ifndef INDICATOR_DATETIME_PLANNER_MOCK_H
#define INDICATOR_DATETIME_PLANNER_MOCK_H

#include <datetime/planner-range.h>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief #RangePlanner which does nothing on its own.
 *        Its controller must set its appointments property.
 */
class MockRangePlanner: public RangePlanner
{
public:
    MockRangePlanner():
        m_range(std::pair<DateTime,DateTime>(DateTime::NowLocal(), DateTime::NowLocal()))
    {
    }

    ~MockRangePlanner() =default;

    core::Property<std::vector<Appointment>>& appointments() { return m_appointments; }
    core::Property<std::pair<DateTime,DateTime>>& range() { return m_range; }

private:
    core::Property<std::vector<Appointment>> m_appointments;
    core::Property<std::pair<DateTime,DateTime>> m_range;
};
 

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_PLANNER_MOCK_H
