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

#ifndef INDICATOR_DATETIME_APPOINTMENT_H
#define INDICATOR_DATETIME_APPOINTMENT_H

#include <datetime/date-time.h>
#include <string>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Plain Old Data Structure that represents a calendar appointment.
 *
 * @see Planner
 */
struct Appointment
{
public:
    std::string color; 
    std::string summary;
    std::string url;
    std::string uid;
    bool has_alarms = false;
    DateTime begin;
    DateTime end;

    bool operator== (const Appointment& that) const;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_APPOINTMENT_H
