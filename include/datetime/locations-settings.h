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

#ifndef INDICATOR_DATETIME_SETTINGS_LOCATIONS_H
#define INDICATOR_DATETIME_SETTINGS_LOCATIONS_H

#include <datetime/locations.h> // base class

#include <datetime/settings.h>
#include <datetime/timezones.h>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief #Locations implementation which builds its list from the #Settings.
 */
class SettingsLocations: public Locations
{
public:
    /**
     * @param[in] settings the #Settings whose locations property is to be used
     * @param[in] timezones the #Timezones to always show first in the list
     */
    SettingsLocations (const std::shared_ptr<const Settings>& settings,
                       const std::shared_ptr<const Timezones>& timezones);

private:
    std::shared_ptr<const Settings> m_settings;
    std::shared_ptr<const Timezones> m_timezones;
    void reload();
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_SETTINGS_LOCATIONS_H
