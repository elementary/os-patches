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

#include <datetime/appointment.h>

namespace unity {
namespace indicator {
namespace datetime {

/****
*****
****/

bool Appointment::operator==(const Appointment& that) const
{
    return (color==that.color)
        && (summary==that.summary)
        && (url==that.url)
        && (uid==that.uid)
        && (has_alarms==that.has_alarms)
        && (begin==that.begin)
        && (end==that.end);
}

/****
*****
****/

} // namespace datetime
} // namespace indicator
} // namespace unity
