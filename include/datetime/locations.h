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

#ifndef INDICATOR_DATETIME_LOCATIONS_H
#define INDICATOR_DATETIME_LOCATIONS_H

#include <datetime/date-time.h>

#include <core/property.h>

#include <string>
#include <vector>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A physical place and its timezone; eg, "America/Chicago" + "Oklahoma City"
 *
 * @see Locations
 */
class Location
{
public:
    Location (const std::string& zone, const std::string& name);
    const std::string& zone() const;
    const std::string& name() const;
    bool operator== (const Location& that) const;

private:

    /** timezone; eg, "America/Chicago" */
    std::string m_zone;

    /* human-readable location name; eg, "Oklahoma City" */
    std::string m_name;

    /** offset from UTC in microseconds */
    int64_t m_offset = 0;
};

/**
 * Container which holds an ordered list of Locations
 *
 * @see Location
 * @see State
 */
class Locations
{
public:
    Locations() =default;
    virtual ~Locations() =default;

    /** \brief an ordered list of Location items */
    core::Property<std::vector<Location>> locations;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_LOCATIONS_H
