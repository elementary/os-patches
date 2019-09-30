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

#ifndef INDICATOR_DATETIME_SETTINGS_LIVE_H
#define INDICATOR_DATETIME_SETTINGS_LIVE_H

#include <datetime/settings.h> // parent class

#include <gio/gio.h> // GSettings

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief #Settings implementation which uses GSettings.
 */
class LiveSettings: public Settings
{
public:
    LiveSettings();
    virtual ~LiveSettings();

private:
    static void on_changed(GSettings*, gchar*, gpointer);
    void update_key(const std::string& key);

    void update_custom_time_format();
    void update_locations();
    void update_show_calendar();
    void update_show_clock();
    void update_show_date();
    void update_show_day();
    void update_show_detected_locations();
    void update_show_events();
    void update_show_locations();
    void update_show_seconds();
    void update_show_week_numbers();
    void update_show_year();
    void update_time_format_mode();
    void update_timezone_name();

    GSettings* m_settings;

    // we've got a raw pointer here, so disable copying
    LiveSettings(const LiveSettings&) =delete;
    LiveSettings& operator=(const LiveSettings&) =delete;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_SETTINGS_LIVE_H
