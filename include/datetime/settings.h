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

#ifndef INDICATOR_DATETIME_SETTINGS_H
#define INDICATOR_DATETIME_SETTINGS_H

#include <datetime/settings-shared.h>

#include <core/property.h>

#include <vector>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Interface that represents user-configurable settings.
 *
 * See the descriptions in data/com.canonical.indicator.datetime.gschema.xml
 * for more information on specific properties.
 */
class Settings 
{
public:
    Settings() =default;
    virtual ~Settings() =default;

    core::Property<std::string> custom_time_format;
    core::Property<std::vector<std::string>> locations;
    core::Property<bool> show_calendar;
    core::Property<bool> show_clock;
    core::Property<bool> show_date;
    core::Property<bool> show_day;
    core::Property<bool> show_detected_location;
    core::Property<bool> show_events;
    core::Property<bool> show_locations;
    core::Property<bool> show_seconds;
    core::Property<bool> show_week_numbers;
    core::Property<bool> show_year;
    core::Property<TimeFormatMode> time_format_mode;
    core::Property<std::string> timezone_name;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_SETTINGS_H
