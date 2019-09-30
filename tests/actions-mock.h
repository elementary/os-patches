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

#ifndef INDICATOR_DATETIME_ACTIONS_MOCK_H
#define INDICATOR_DATETIME_ACTIONS_MOCK_H

#include <datetime/actions.h>

#include <set>

namespace unity {
namespace indicator {
namespace datetime {

class MockActions: public Actions
{
public:
    MockActions(std::shared_ptr<State>& state_in): Actions(state_in) {}
    ~MockActions() =default;

    enum Action { DesktopOpenAlarmApp,
                  DesktopOpenAppt,
                  DesktopOpenCalendarApp,
                  DesktopOpenSettingsApp,
                  PhoneOpenAlarmApp,
                  PhoneOpenAppt,
                  PhoneOpenCalendarApp,
                  PhoneOpenSettingsApp,
                  SetLocation };

    const std::vector<Action>& history() const { return m_history; }
    const DateTime& date_time() const { return m_date_time; }
    const std::string& zone() const { return m_zone; }
    const std::string& name() const { return m_name; }
    const Appointment& appointment() const { return m_appt; }
    void clear() { m_history.clear(); m_zone.clear(); m_name.clear(); }

    bool desktop_has_calendar_app() const {
        return m_desktop_has_calendar_app;
    }
    void desktop_open_alarm_app() {
        m_history.push_back(DesktopOpenAlarmApp);
    }
    void desktop_open_appointment(const Appointment& appt) {
        m_appt = appt;
        m_history.push_back(DesktopOpenAppt);
    }
    void desktop_open_calendar_app(const DateTime& dt) {
        m_date_time = dt;
        m_history.push_back(DesktopOpenCalendarApp);
    }
    void desktop_open_settings_app() {
        m_history.push_back(DesktopOpenSettingsApp);
    }

    void phone_open_alarm_app() {
        m_history.push_back(PhoneOpenAlarmApp);
    }
    void phone_open_appointment(const Appointment& appt) {
        m_appt = appt;
        m_history.push_back(PhoneOpenAppt);
    }
    void phone_open_calendar_app(const DateTime& dt) {
        m_date_time = dt;
        m_history.push_back(PhoneOpenCalendarApp);
    }
    void phone_open_settings_app() {
        m_history.push_back(PhoneOpenSettingsApp);
    }

    void set_location(const std::string& zone_, const std::string& name_) {
        m_history.push_back(SetLocation);
        m_zone = zone_;
        m_name = name_;
    }

    void set_desktop_has_calendar_app(bool b) {
        m_desktop_has_calendar_app = b;
    }

private:
    bool m_desktop_has_calendar_app = true;
    Appointment m_appt;
    std::string m_zone;
    std::string m_name;
    DateTime m_date_time;
    std::vector<Action> m_history;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_ACTIONS_MOCK_H
