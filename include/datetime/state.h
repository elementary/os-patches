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

#ifndef INDICATOR_DATETIME_STATE_H
#define INDICATOR_DATETIME_STATE_H

#include <datetime/clock.h>
#include <datetime/locations.h>
#include <datetime/planner-month.h>
#include <datetime/planner-upcoming.h>
#include <datetime/settings.h>
#include <datetime/timezones.h>
   
#include <core/property.h>

#include <memory> // std::shared_ptr

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Aggregates all the classes that represent the backend state.
 *
 * This is where the app comes together. It's a model that aggregates
 * all of the backend appointments/alarms, locations, timezones,
 * system time, and so on. The "view" code (ie, the Menus) need to
 * respond to Signals from the State and update themselves accordingly.
 *
 * @see Menu
 * @see MenuFactory
 * @see Timezones
 * @see Clock
 * @see Planner
 * @see Locations
 * @see Settings
 */
struct State
{
    /** \brief The current time. Used by the header, by the date menuitem,
               and by the locations for relative timestamp */
    std::shared_ptr<Clock> clock;

    /** \brief The locations to be displayed in the Locations
               section of the #Menu */
    std::shared_ptr<Locations> locations;

    /** \brief Appointments in the month that's being displayed
               in the calendar section of the #Menu */
    std::shared_ptr<MonthPlanner> calendar_month;

    /** \brief The next appointments that follow highlighed date
               highlighted in the calendar section of the #Menu
               (default date = today) */
    std::shared_ptr<UpcomingPlanner> calendar_upcoming;

    /** \brief Configuration options that modify the view */
    std::shared_ptr<Settings> settings;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_STATE_H
