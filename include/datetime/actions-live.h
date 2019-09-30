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

#ifndef INDICATOR_DATETIME_ACTIONS_LIVE_H
#define INDICATOR_DATETIME_ACTIONS_LIVE_H

#include <datetime/actions.h>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Production implementation of the Actions interface.
 *
 * Delegates URLs, sets the timezone via org.freedesktop.timedate1, etc.
 *
 * @see MockActions
 */
class LiveActions: public Actions
{
public:
    LiveActions(const std::shared_ptr<State>& state_in);
    ~LiveActions() =default;

    bool desktop_has_calendar_app() const;
    void desktop_open_alarm_app();
    void desktop_open_appointment(const Appointment&);
    void desktop_open_calendar_app(const DateTime&);
    void desktop_open_settings_app();

    void phone_open_alarm_app();
    void phone_open_appointment(const Appointment&);
    void phone_open_calendar_app(const DateTime&);
    void phone_open_settings_app();

    void set_location(const std::string& zone, const std::string& name);

protected:
    virtual void execute_command(const std::string& command);
    virtual void dispatch_url(const std::string& url);
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_ACTIONS_H
