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

#include <datetime/timezones-live.h>

#include <glib.h>

namespace unity {
namespace indicator {
namespace datetime {

LiveTimezones::LiveTimezones(const std::shared_ptr<const Settings>& settings,
                             const std::string& filename):
    m_file(filename),
    m_settings(settings)
{
    m_file.timezone.changed().connect([this](const std::string&){update_timezones();});

    m_settings->show_detected_location.changed().connect([this](bool){update_geolocation();});
    update_geolocation();

    update_timezones();
}

void LiveTimezones::update_geolocation()
{
    // clear the previous pointer, if any
    m_geo.reset();

    // if location detection is enabled, turn on GeoClue
    if(m_settings->show_detected_location.get())
    {
        auto geo = new GeoclueTimezone();
        geo->timezone.changed().connect([this](const std::string&){update_timezones();});
        m_geo.reset(geo);
    }
}

void LiveTimezones::update_timezones()
{
    const auto a = m_file.timezone.get();
    const auto b = m_geo ? m_geo->timezone.get() : "";

    timezone.set(a.empty() ? b : a);

    std::set<std::string> zones;
    if (!a.empty())
        zones.insert(a);
    if (!b.empty())
        zones.insert(b);
    timezones.set(zones);
}

} // namespace datetime
} // namespace indicator
} // namespace unity
