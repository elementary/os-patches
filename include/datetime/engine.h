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

#ifndef INDICATOR_DATETIME_ENGINE__H
#define INDICATOR_DATETIME_ENGINE__H

#include <datetime/appointment.h>
#include <datetime/date-time.h>
#include <datetime/timezone.h>

#include <functional>
#include <vector>

namespace unity {
namespace indicator {
namespace datetime {

/****
*****
****/

/**
 * Class wrapper around the backend that generates appointments
 * 
 * @see EdsEngine
 * @see EdsPlanner
 */
class Engine
{
public:
    virtual ~Engine() =default;

    virtual void get_appointments(const DateTime& begin,
                                  const DateTime& end,
                                  const Timezone& default_timezone,
                                  std::function<void(const std::vector<Appointment>&)> appointment_func) =0;

    virtual core::Signal<>& changed() =0;

protected:
    Engine() =default;
};

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_ENGINE__H
