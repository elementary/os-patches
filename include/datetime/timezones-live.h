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

#ifndef INDICATOR_DATETIME_LIVE_TIMEZONES_H
#define INDICATOR_DATETIME_LIVE_TIMEZONES_H

#include <datetime/settings.h>
#include <datetime/timezones.h>
#include <datetime/timezone-file.h>
#include <datetime/timezone-geoclue.h>

#include <memory> // shared_ptr<>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief #Timezones object that uses a #FileTimezone and #GeoclueTimezone
 *        to detect what timezone we're in
 */
class LiveTimezones: public Timezones
{
public:
    LiveTimezones(const std::shared_ptr<const Settings>& settings, const std::string& filename);

private:
    void update_geolocation();
    void update_timezones();

    FileTimezone m_file;
    std::shared_ptr<const Settings> m_settings;
    std::shared_ptr<GeoclueTimezone> m_geo;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_LIVE_TIMEZONES_H
