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

#include <datetime/locations-settings.h>

#include <datetime/settings-shared.h>
#include <datetime/timezones.h>
#include <datetime/utils.h>

#include <algorithm> // std::find()

namespace unity {
namespace indicator {
namespace datetime {

SettingsLocations::SettingsLocations(const std::shared_ptr<const Settings>& settings,
                                     const std::shared_ptr<const Timezones>& timezones):
    m_settings(settings),
    m_timezones(timezones)
{
    m_settings->locations.changed().connect([this](const std::vector<std::string>&){reload();});
    m_settings->show_locations.changed().connect([this](bool){reload();});
    m_timezones->timezone.changed().connect([this](const std::string&){reload();});
    m_timezones->timezones.changed().connect([this](const std::set<std::string>&){reload();});

    reload();
}

void
SettingsLocations::reload()
{
    std::vector<Location> v;
    const std::string timezone_name = m_settings->timezone_name.get();

    // add the primary timezone first
    auto zone = m_timezones->timezone.get();
    if (!zone.empty())
    {
        gchar * name = get_beautified_timezone_name(zone.c_str(), timezone_name.c_str());
        Location l(zone, name);
        v.push_back(l);
        g_free(name);
    }

    // add the other detected timezones
    for(const auto& zone : m_timezones->timezones.get())
    {
        gchar * name = get_beautified_timezone_name(zone.c_str(), timezone_name.c_str());
        Location l(zone, name);
        if (std::find(v.begin(), v.end(), l) == v.end())
            v.push_back(l);
        g_free(name);
    }

    // maybe add the user-specified locations
    if (m_settings->show_locations.get())
    {
        for(const auto& locstr : m_settings->locations.get())
        {
            gchar* zone;
            gchar* name;
            split_settings_location(locstr.c_str(), &zone, &name);
            Location loc(zone, name);
            if (std::find(v.begin(), v.end(), loc) == v.end())
                v.push_back(loc);
            g_free(name);
            g_free(zone);
        }
    }

    locations.set(v);
}

} // namespace datetime
} // namespace indicator
} // namespace unity
