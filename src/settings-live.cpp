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

#include <datetime/settings-live.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

LiveSettings::~LiveSettings()
{
    g_clear_object(&m_settings);
}

LiveSettings::LiveSettings():
    m_settings(g_settings_new(SETTINGS_INTERFACE))
{
    g_signal_connect (m_settings, "changed", G_CALLBACK(on_changed), this);

    // init the Properties from the GSettings backend
    update_custom_time_format();
    update_locations();
    update_show_calendar();
    update_show_clock();
    update_show_date();
    update_show_day();
    update_show_detected_locations();
    update_show_events();
    update_show_locations();
    update_show_seconds();
    update_show_week_numbers();
    update_show_year();
    update_time_format_mode();
    update_timezone_name();

    // now listen for clients to change the properties s.t. we can sync update GSettings

    custom_time_format.changed().connect([this](const std::string& value){
        g_settings_set_string(m_settings, SETTINGS_CUSTOM_TIME_FORMAT_S, value.c_str());
    });

    locations.changed().connect([this](const std::vector<std::string>& value){
        const int n = value.size();
        gchar** strv = g_new0(gchar*, n+1);
        for(int i=0; i<n; i++)
            strv[i] = const_cast<char*>(value[i].c_str());
        g_settings_set_strv(m_settings, SETTINGS_LOCATIONS_S, strv);
        g_free(strv);
    });

    show_calendar.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_CALENDAR_S, value);
    });

    show_clock.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_CLOCK_S, value);
    });

    show_date.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_DATE_S, value);
    });

    show_day.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_DAY_S, value);
    });

    show_detected_location.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_DETECTED_S, value);
    });

    show_events.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_EVENTS_S, value);
    });

    show_locations.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_LOCATIONS_S, value);
    });

    show_seconds.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_SECONDS_S, value);
    });

    show_week_numbers.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_WEEK_NUMBERS_S, value);
    });

    show_year.changed().connect([this](bool value){
        g_settings_set_boolean(m_settings, SETTINGS_SHOW_YEAR_S, value);
    });

    time_format_mode.changed().connect([this](TimeFormatMode value){
        g_settings_set_enum(m_settings, SETTINGS_TIME_FORMAT_S, gint(value));
    });

    timezone_name.changed().connect([this](const std::string& value){
        g_settings_set_string(m_settings, SETTINGS_TIMEZONE_NAME_S, value.c_str());
    });
}

/***
****
***/

void LiveSettings::update_custom_time_format()
{
    auto val = g_settings_get_string(m_settings, SETTINGS_CUSTOM_TIME_FORMAT_S);
    custom_time_format.set(val);
    g_free(val);
}

void LiveSettings::update_locations()
{
    auto strv = g_settings_get_strv(m_settings, SETTINGS_LOCATIONS_S);
    std::vector<std::string> l;
    for(int i=0; strv && strv[i]; i++)
        l.push_back(strv[i]);
    g_strfreev(strv);
    locations.set(l);
}

void LiveSettings::update_show_calendar()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_CALENDAR_S);
    show_calendar.set(val);
}

void LiveSettings::update_show_clock()
{
    show_clock.set(g_settings_get_boolean(m_settings, SETTINGS_SHOW_CLOCK_S));
}

void LiveSettings::update_show_date()
{
    show_date.set(g_settings_get_boolean(m_settings, SETTINGS_SHOW_DATE_S));
}

void LiveSettings::update_show_day()
{
    show_day.set(g_settings_get_boolean(m_settings, SETTINGS_SHOW_DAY_S));
}

void LiveSettings::update_show_detected_locations()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_DETECTED_S);
    show_detected_location.set(val);
}

void LiveSettings::update_show_events()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_EVENTS_S);
    show_events.set(val);
}

void LiveSettings::update_show_locations()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_LOCATIONS_S);
    show_locations.set(val);
}

void LiveSettings::update_show_seconds()
{
    show_seconds.set(g_settings_get_boolean(m_settings, SETTINGS_SHOW_SECONDS_S));
}

void LiveSettings::update_show_week_numbers()
{
    const auto val = g_settings_get_boolean(m_settings, SETTINGS_SHOW_WEEK_NUMBERS_S);
    show_week_numbers.set(val);
}

void LiveSettings::update_show_year()
{
    show_year.set(g_settings_get_boolean(m_settings, SETTINGS_SHOW_YEAR_S));
}

void LiveSettings::update_time_format_mode()
{
    time_format_mode.set((TimeFormatMode)g_settings_get_enum(m_settings, SETTINGS_TIME_FORMAT_S));
}

void LiveSettings::update_timezone_name()
{
    auto val = g_settings_get_string(m_settings, SETTINGS_TIMEZONE_NAME_S);
    timezone_name.set(val);
    g_free(val);
}

/***
****
***/

void LiveSettings::on_changed(GSettings* /*settings*/,
                              gchar*       key,
                              gpointer     gself)
{
    static_cast<LiveSettings*>(gself)->update_key(key);
}

void LiveSettings::update_key(const std::string& key)
{
    if (key == SETTINGS_SHOW_CLOCK_S)
        update_show_clock();
    else if (key == SETTINGS_LOCATIONS_S)
        update_locations();
    else if (key == SETTINGS_TIME_FORMAT_S)
        update_time_format_mode();
    else if (key == SETTINGS_SHOW_SECONDS_S)
        update_show_seconds();
    else if (key == SETTINGS_SHOW_DAY_S)
        update_show_day();
    else if (key == SETTINGS_SHOW_DATE_S)
        update_show_date();
    else if (key == SETTINGS_SHOW_YEAR_S)
        update_show_year();
    else if (key == SETTINGS_CUSTOM_TIME_FORMAT_S)
        update_custom_time_format();
    else if (key == SETTINGS_SHOW_CALENDAR_S)
        update_show_calendar();
    else if (key == SETTINGS_SHOW_WEEK_NUMBERS_S)
        update_show_week_numbers();
    else if (key == SETTINGS_SHOW_EVENTS_S)
        update_show_events();
    else if (key == SETTINGS_SHOW_LOCATIONS_S)
        update_show_locations();
    else if (key == SETTINGS_SHOW_DETECTED_S)
        update_show_detected_locations();
    else if (key == SETTINGS_TIMEZONE_NAME_S)
        update_timezone_name();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
