/*
 * Copyright 2014 Canonical Ltd.
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

#ifndef INDICATOR_DATETIME_CLOCK_WATCHER_H
#define INDICATOR_DATETIME_CLOCK_WATCHER_H

#include <datetime/appointment.h>
#include <datetime/clock.h>
#include <datetime/planner-upcoming.h>

#include <core/signal.h>

#include <memory>
#include <set>
#include <string>

namespace unity {
namespace indicator {
namespace datetime {


/**
 * \brief Watches the clock and appointments to notify when an
 *        appointment's time is reached.
 */
class ClockWatcher
{
public:
    ClockWatcher() =default;
    virtual ~ClockWatcher() =default;
    virtual core::Signal<const Appointment&>& alarm_reached() = 0;
};


/**
 * \brief A #ClockWatcher implementation 
 */
class ClockWatcherImpl: public ClockWatcher
{
public:
    ClockWatcherImpl(const std::shared_ptr<Clock>& clock,
                     const std::shared_ptr<UpcomingPlanner>& upcoming_planner);
    ~ClockWatcherImpl() =default;
    core::Signal<const Appointment&>& alarm_reached();

private:
    void pulse();
    std::set<std::string> m_triggered;
    const std::shared_ptr<Clock> m_clock;
    const std::shared_ptr<UpcomingPlanner> m_upcoming_planner;
    core::Signal<const Appointment&> m_alarm_reached;
};


} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_CLOCK_WATCHER_H
