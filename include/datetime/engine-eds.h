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

#ifndef INDICATOR_DATETIME_ENGINE_EDS__H
#define INDICATOR_DATETIME_ENGINE_EDS__H

#include <datetime/engine.h>

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
 * Class wrapper around EDS so multiple #EdsPlanners can share resources
 * 
 * @see EdsPlanner
 */
class EdsEngine: public Engine
{
public:
    EdsEngine();
    ~EdsEngine();

    void get_appointments(const DateTime& begin,
                          const DateTime& end,
                          const Timezone& default_timezone,
                          std::function<void(const std::vector<Appointment>&)> appointment_func);

    core::Signal<>& changed();

private:
    class Impl;
    std::unique_ptr<Impl> p;

    // we've got a unique_ptr here, disable copying...
    EdsEngine(const EdsEngine&) =delete;
    EdsEngine& operator=(const EdsEngine&) =delete;
};

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_ENGINE_EDS__H
