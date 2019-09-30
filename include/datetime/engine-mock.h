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

#ifndef INDICATOR_DATETIME_ENGINE_MOCK__H
#define INDICATOR_DATETIME_ENGINE_MOCK__H

#include <datetime/engine.h>

namespace unity {
namespace indicator {
namespace datetime {

/****
*****
****/

/**
 * A no-op #Engine
 * 
 * @see Engine
 */
class MockEngine: public Engine
{
public:
    MockEngine() =default;
    ~MockEngine() =default;

    void get_appointments(const DateTime& /*begin*/,
                          const DateTime& /*end*/,
                          const Timezone& /*default_timezone*/,
                          std::function<void(const std::vector<Appointment>&)> appointment_func) {
        appointment_func(m_appointments);
    }

    core::Signal<>& changed() {
        return m_changed;
    }

private:
    core::Signal<> m_changed;
    std::vector<Appointment> m_appointments;
};

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_ENGINE_NOOP__H
