/*
 * Copyright 2010 Canonical Ltd.
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
 *   Ted Gould <ted@canonical.com>
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#ifndef INDICATOR_DATETIME_SETTINGS_SHARED
#define INDICATOR_DATETIME_SETTINGS_SHARED

typedef enum
{
  TIME_FORMAT_MODE_LOCALE_DEFAULT,
  TIME_FORMAT_MODE_12_HOUR,
  TIME_FORMAT_MODE_24_HOUR,
  TIME_FORMAT_MODE_CUSTOM
}
TimeFormatMode;

#define SETTINGS_INTERFACE              "com.canonical.indicator.datetime"
#define SETTINGS_SHOW_CLOCK_S           "show-clock"
#define SETTINGS_TIME_FORMAT_S          "time-format"
#define SETTINGS_SHOW_SECONDS_S         "show-seconds"
#define SETTINGS_SHOW_DAY_S             "show-day"
#define SETTINGS_SHOW_DATE_S            "show-date"
#define SETTINGS_SHOW_YEAR_S            "show-year"
#define SETTINGS_CUSTOM_TIME_FORMAT_S   "custom-time-format"
#define SETTINGS_SHOW_CALENDAR_S        "show-calendar"
#define SETTINGS_SHOW_WEEK_NUMBERS_S    "show-week-numbers"
#define SETTINGS_SHOW_EVENTS_S          "show-events"
#define SETTINGS_SHOW_LOCATIONS_S       "show-locations"
#define SETTINGS_SHOW_DETECTED_S        "show-auto-detected-location"
#define SETTINGS_LOCATIONS_S            "locations"
#define SETTINGS_TIMEZONE_NAME_S        "timezone-name"

#endif // INDICATOR_DATETIME_SETTINGS_SHARED
