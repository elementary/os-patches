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

#ifndef INDICATOR_DATETIME_CLOCK_H
#define INDICATOR_DATETIME_CLOCK_H

#include <datetime/date-time.h>

#include <core/property.h>
#include <core/signal.h>

#include <gio/gio.h> // GDBusConnection

#include <memory> // std::shared_ptr, std::unique_ptr

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A clock.
 */
class Clock
{
public:
    virtual ~Clock();
    virtual DateTime localtime() const =0;

    /** \brief A signal which fires when the clock's minute changes */
    core::Signal<> minute_changed;

    /** \brief A signal which fires when the clock's date changes */
    core::Signal<> date_changed;

protected:
    Clock();

    /** \brief Compares old and new times, emits minute_changed() or date_changed() signals if appropriate */
    void maybe_emit (const DateTime& a, const DateTime& b);

private:
    static void on_system_bus_ready(GObject*, GAsyncResult*, gpointer);
    static void on_prepare_for_sleep(GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant*, gpointer);

    GCancellable * m_cancellable = nullptr;
    GDBusConnection * m_system_bus = nullptr;
    unsigned int m_sleep_subscription_id = 0;

    // we've got raw pointers and GSignal tags in here, so disable copying
    Clock(const Clock&) =delete;
    Clock& operator=(const Clock&) =delete;
};

/***
****
***/

class Timezones;

/**
 * \brief A live #Clock that provides the actual system time.
 */
class LiveClock: public Clock
{
public:
    LiveClock (const std::shared_ptr<const Timezones>& zones);
    virtual ~LiveClock();
    virtual DateTime localtime() const;

private:
    class Impl;
    std::unique_ptr<Impl> p;
};

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_CLOCK_H
