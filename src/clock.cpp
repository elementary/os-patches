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

#include <glib.h>
#include <gio/gio.h>

namespace unity {
namespace indicator {
namespace datetime {

/***
****
***/

Clock::Clock():
    m_cancellable(g_cancellable_new())
{
    g_bus_get(G_BUS_TYPE_SYSTEM, m_cancellable, on_system_bus_ready, this);
}

Clock::~Clock()
{
    g_cancellable_cancel(m_cancellable);
    g_clear_object(&m_cancellable);

    if (m_sleep_subscription_id)
        g_dbus_connection_signal_unsubscribe(m_system_bus, m_sleep_subscription_id);

    g_clear_object(&m_system_bus);
}

void
Clock::on_system_bus_ready(GObject*, GAsyncResult * res, gpointer gself)
{
    GDBusConnection * system_bus;

    if ((system_bus = g_bus_get_finish(res, nullptr)))
    {
        auto self = static_cast<Clock*>(gself);

        self->m_system_bus = system_bus;

        self->m_sleep_subscription_id = g_dbus_connection_signal_subscribe(
                        system_bus,
                        nullptr,
                        "org.freedesktop.login1.Manager", // interface
                        "PrepareForSleep", // signal name
                        "/org/freedesktop/login1", // object path
                        nullptr, // arg0
                        G_DBUS_SIGNAL_FLAGS_NONE,
                        on_prepare_for_sleep,
                        self,
                        nullptr);
    }
}

void
Clock::on_prepare_for_sleep(GDBusConnection* /*connection*/,
                            const gchar*     /*sender_name*/,
                            const gchar*     /*object_path*/,
                            const gchar*     /*interface_name*/,
                            const gchar*     /*signal_name*/,
                            GVariant*        /*parameters*/,
                            gpointer           gself)
{
    static_cast<Clock*>(gself)->minute_changed();
}

/***
****
***/

} // namespace datetime
} // namespace indicator
} // namespace unity
