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

#ifndef INDICATOR_DATETIME_TIMEZONES_H
#define INDICATOR_DATETIME_TIMEZONES_H

#include <datetime/timezone.h>

#include <core/property.h>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Helper class which aggregates one or more timezones
 *
 * @see LiveClock
 * @see SettingsLocations
 */
class Timezones
{
public:
    Timezones() =default;
    virtual ~Timezones() =default;

    /**
     * \brief the current timezone
     */
    core::Property<std::string> timezone;

    /**
     * \brief all the detected timezones.
     * The count is >1 iff the detection mechamisms disagree.
     */
    core::Property<std::set<std::string> > timezones;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_TIMEZONES_H
