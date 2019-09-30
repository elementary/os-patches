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

#include <datetime/formatter.h>

#include <datetime/clock.h>
#include <datetime/utils.h> // T_()

#include <glib.h>
#include <glib/gi18n.h>

#include <locale.h> // setlocale()
#include <langinfo.h> // nl_langinfo()
#include <string.h> // strstr()

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

namespace
{

void clear_timer(guint& tag)
{
    if (tag)
    {
        g_source_remove(tag);
        tag = 0;
    }
}

gint calculate_milliseconds_until_next_second(const DateTime& now)
{
    gint interval_usec;
    guint interval_msec;

    interval_usec = G_USEC_PER_SEC - g_date_time_get_microsecond(now.get());
    interval_msec = (interval_usec + 999) / 1000;
    return interval_msec;
}

/*
 * We periodically rebuild the sections that have time format strings
 * that are dependent on the current time:
 *
 * 1. appointment menuitems' time format strings depend on the
 *    current time; for example, they don't show the day of week
 *    if the appointment is today.
 *
 * 2. location menuitems' time format strings depend on the
 *    current time; for example, they don't show the day of the week
 *    if the local date and location date are the same.
 *
 * 3. the "local date" menuitem in the calendar section is,
 *    obviously, dependent on the local time.
 *
 * In short, we want to update whenever the number of days between two zone
 * might have changed. We do that by updating when either zone's day changes.
 *
 * Since not all UTC offsets are evenly divisible by hours
 * (examples: Newfoundland UTC-03:30, Nepal UTC+05:45), refreshing on the hour
 * is not enough. We need to refresh at HH:00, HH:15, HH:30, and HH:45.
 */
guint calculate_seconds_until_next_fifteen_minutes(GDateTime * now)
{
    char * str;
    gint minute;
    guint seconds;
    GTimeSpan diff;
    GDateTime * next;
    GDateTime * start_of_next;

    minute = g_date_time_get_minute(now);
    minute = 15 - (minute % 15);
    next = g_date_time_add_minutes(now, minute);
    start_of_next = g_date_time_new_local(g_date_time_get_year(next),
                                          g_date_time_get_month(next),
                                          g_date_time_get_day_of_month(next),
                                          g_date_time_get_hour(next),
                                          g_date_time_get_minute(next),
                                          0.1);

    str = g_date_time_format(start_of_next, "%F %T");
    g_debug("%s %s the next timestamp rebuild will be at %s", G_STRLOC, G_STRFUNC, str);
    g_free(str);

    diff = g_date_time_difference(start_of_next, now);
    seconds = (diff + (G_TIME_SPAN_SECOND-1)) / G_TIME_SPAN_SECOND;

    g_date_time_unref(start_of_next);
    g_date_time_unref(next);
    return seconds;
}
} // unnamed namespace


class Formatter::Impl
{
public:

    Impl(Formatter* owner, const std::shared_ptr<const Clock>& clock):
        m_owner(owner),
        m_clock(clock)
    {
        m_owner->header_format.changed().connect([this](const std::string& /*fmt*/){update_header();});
        m_clock->minute_changed.connect([this](){update_header();});
        update_header();

        restartRelativeTimer();
    }

    ~Impl()
    {
        clear_timer(m_header_seconds_timer);
        clear_timer(m_relative_timer);
    }

private:

    static bool format_shows_seconds(const std::string& fmt)
    {
        return (fmt.find("%s") != std::string::npos)
            || (fmt.find("%S") != std::string::npos)
            || (fmt.find("%T") != std::string::npos)
            || (fmt.find("%X") != std::string::npos)
            || (fmt.find("%c") != std::string::npos);
    }

    void update_header()
    {
        // update the header property
        const auto fmt = m_owner->header_format.get();
        const auto str = m_clock->localtime().format(fmt);
        m_owner->header.set(str);

        // if the header needs to show seconds, set a timer.
        if (format_shows_seconds(fmt))
            start_header_timer();
        else
            clear_timer(m_header_seconds_timer);
    }

    // we've got a header format that shows seconds,
    // so we need to update it every second
    void start_header_timer()
    {
        clear_timer(m_header_seconds_timer);

        const auto now = m_clock->localtime();
        auto interval_msec = calculate_milliseconds_until_next_second(now);
        interval_msec += 50; // add a small margin to ensure the callback
                             // fires /after/ next is reached
        m_header_seconds_timer = g_timeout_add_full(G_PRIORITY_HIGH,
                                                    interval_msec,
                                                    on_header_timer,
                                                    this,
                                                    nullptr);
    }

    static gboolean on_header_timer(gpointer gself)
    {
        static_cast<Formatter::Impl*>(gself)->update_header();
        return G_SOURCE_REMOVE;
    }

private:

    void restartRelativeTimer()
    {
        clear_timer(m_relative_timer);

        const auto now = m_clock->localtime();
        const auto seconds = calculate_seconds_until_next_fifteen_minutes(now.get());
        m_relative_timer = g_timeout_add_seconds(seconds, onRelativeTimer, this);
    }

    static gboolean onRelativeTimer(gpointer gself)
    {
        auto self = static_cast<Formatter::Impl*>(gself);
        self->m_owner->relative_format_changed();
        self->restartRelativeTimer();
        return G_SOURCE_REMOVE;
    }

private:
    Formatter* const m_owner;
    guint m_header_seconds_timer = 0;
    guint m_relative_timer = 0;

public:
    std::shared_ptr<const Clock> m_clock;
};

/***
****
***/

Formatter::Formatter(const std::shared_ptr<const Clock>& clock):
    p(new Formatter::Impl(this, clock))
{
}

Formatter::~Formatter()
{
}

const char*
Formatter::default_header_time_format(bool twelvehour, bool show_seconds)
{
  const char* fmt;

  if (twelvehour && show_seconds)
    /* TRANSLATORS: a strftime(3) format for 12hr time w/seconds */
    fmt = T_("%l:%M:%S %p");
  else if (twelvehour)
    /* TRANSLATORS: a strftime(3) format for 12hr time */
    fmt = T_("%l:%M %p");
  else if (show_seconds)
    /* TRANSLATORS: a strftime(3) format for 24hr time w/seconds */
    fmt = T_("%H:%M:%S");
  else
    /* TRANSLATORS: a strftime(3) format for 24hr time */
    fmt = T_("%H:%M");

  return fmt;
}

/***
****
***/

std::string
Formatter::relative_format(GDateTime* then_begin, GDateTime* then_end) const
{
    auto cstr = generate_full_format_string_at_time (p->m_clock->localtime().get(), then_begin, then_end);
    const std::string ret = cstr;
    g_free (cstr);
    return ret;
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
