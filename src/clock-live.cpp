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

#include <datetime/clock.h>
#include <datetime/timezones.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

namespace
{

void clearTimer(guint& tag)
{
    if (tag)
    {
        g_source_remove(tag);
        tag = 0;
    }
}

guint calculate_milliseconds_until_next_minute(const DateTime& now)
{
    auto next = g_date_time_add_minutes(now.get(), 1);
    auto start_of_next = g_date_time_add_seconds (next, -g_date_time_get_seconds(next));
    const auto interval_usec = g_date_time_difference(start_of_next, now.get());
    const guint interval_msec = (interval_usec + 999) / 1000;
    g_date_time_unref(start_of_next);
    g_date_time_unref(next);
    g_assert (interval_msec <= 60000);
    return interval_msec;
}

} // unnamed namespace


class LiveClock::Impl
{
public:

    Impl(LiveClock& owner, const std::shared_ptr<const Timezones>& tzd):
        m_owner(owner),
        m_timezones(tzd)
    {
        if (m_timezones)
        {
            m_timezones->timezone.changed().connect([this](const std::string& z) {setTimezone(z);});
            setTimezone(m_timezones->timezone.get());
        }

        restart_minute_timer();
    }

    ~Impl()
    {
        clearTimer(m_timer);

        g_clear_pointer(&m_timezone, g_time_zone_unref);
    }

    DateTime localtime() const
    {
        g_assert(m_timezone != nullptr);

        auto gdt = g_date_time_new_now(m_timezone);
        DateTime ret(gdt);
        g_date_time_unref(gdt);
        return ret;
    }

private:

    void setTimezone(const std::string& str)
    {
        g_clear_pointer(&m_timezone, g_time_zone_unref);
        m_timezone = g_time_zone_new(str.c_str());
        m_owner.minute_changed();
    }

    /***
    ****
    ***/

    void restart_minute_timer()
    {
        clearTimer(m_timer);

        // maybe emit change signals
        const auto now = localtime();
        if (!DateTime::is_same_minute(m_prev_datetime, now))
            m_owner.minute_changed();
        if (!DateTime::is_same_day(m_prev_datetime, now))
            m_owner.date_changed();

        // queue up a timer to fire at the next minute
        m_prev_datetime = now;
        auto interval_msec = calculate_milliseconds_until_next_minute(now);
        interval_msec += 50; // add a small margin to ensure the callback
                             // fires /after/ next is reached
        m_timer = g_timeout_add_full(G_PRIORITY_HIGH,
                                     interval_msec,
                                     on_minute_timer_reached,
                                     this,
                                     nullptr);
    }

    static gboolean on_minute_timer_reached(gpointer gself)
    {
        static_cast<LiveClock::Impl*>(gself)->restart_minute_timer();
        return G_SOURCE_REMOVE;
    }

protected:

    LiveClock& m_owner;
    GTimeZone* m_timezone = nullptr;
    std::shared_ptr<const Timezones> m_timezones;

    DateTime m_prev_datetime;
    unsigned int m_timer = 0;
};

LiveClock::LiveClock(const std::shared_ptr<const Timezones>& tzd):
    p(new Impl(*this, tzd))
{
}

LiveClock::~LiveClock() =default;

DateTime LiveClock::localtime() const
{
    return p->localtime();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

