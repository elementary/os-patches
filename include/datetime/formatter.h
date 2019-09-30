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

#ifndef INDICATOR_DATETIME_FORMATTER_H
#define INDICATOR_DATETIME_FORMATTER_H

#include <core/property.h>
#include <core/signal.h>

#include <datetime/clock.h>
#include <datetime/settings.h>
#include <datetime/utils.h> // is_locale_12h()

#include <glib.h>

#include <string>
#include <memory>

namespace unity {
namespace indicator {
namespace datetime {

class Clock;
class DateTime;

/***
****
***/

/**
 * \brief Provide the strftime() format strings
 *
 * This is a simple goal, but getting there has a lot of options and edge cases:
 *
 *  - The default time format can change based on the locale.
 *
 *  - The user's settings can change or completely override the format string.
 *
 *  - The time formats are different on the Phone and Desktop profiles.
 *
 *  - The time format string in the Locations' menuitems uses (mostly)
 *    the same time format as the header, except for some changes.
 *
 *  - The 'current time' format string in the Locations' menuitems also
 *    prepends the string 'Yesterday' or 'Today' if it differs from the
 *    local time, so Formatter needs to have a Clock for its state.
 *
 * So the Formatter monitors system settings, the current timezone, etc.
 * and upate its time format properties appropriately.
 */
class Formatter
{
public:

    /** \brief The time format string for the menu header */
    core::Property<std::string> header_format;

    /** \brief The time string for the menu header. (eg, the header_format + the clock's time */
    core::Property<std::string> header;

    /** \brief Signal to denote when the relativeFormat has changed.
               When this is emitted, clients will want to rebuild their
               menuitems that contain relative time strings
               (ie, the Appointments and Locations menuitems) */
    core::Signal<> relative_format_changed;

    /** \brief Generate a relative time format for some time (or time range)
               from the current clock's value. For example, a full-day interval
               starting at the end of the current clock's day yields "Tomorrow" */
    std::string relative_format(GDateTime* then, GDateTime* then_end=nullptr) const;

protected:
    Formatter(const std::shared_ptr<const Clock>&);
    virtual ~Formatter();

    static const char* default_header_time_format(bool twelvehour, bool show_seconds);

private:

    Formatter(const Formatter&) =delete;
    Formatter& operator=(const Formatter&) =delete;

    class Impl;
    std::unique_ptr<Impl> p;
};


/**
 * \brief A Formatter for the Desktop and DesktopGreeter profiles.
 */
class DesktopFormatter: public Formatter
{
public:
    DesktopFormatter(const std::shared_ptr<const Clock>&, const std::shared_ptr<const Settings>&);

private:
    std::shared_ptr<const Settings> m_settings;

    void rebuildHeaderFormat();
    const gchar* getFullTimeFormatString() const;
    std::string getHeaderLabelFormatString() const;
    const gchar* getDateFormat(bool show_day, bool show_date, bool show_year) const;

};


/**
 * \brief A Formatter for Phone and PhoneGreeter profiles.
 */
class PhoneFormatter: public Formatter
{
public:
    PhoneFormatter(const std::shared_ptr<const Clock>& clock): Formatter(clock) {
        header_format.set(default_header_time_format(is_locale_12h(), false));
    }
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_CLOCK_H
