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

#ifndef INDICATOR_DATETIME_EXPORTER_H
#define INDICATOR_DATETIME_EXPORTER_H

#include <datetime/actions.h>
#include <datetime/menu.h>

#include <core/signal.h>

#include <gio/gio.h> // GActionGroup

#include <memory> // std::shared_ptr
#include <vector>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief Exports actions and menus to DBus. 
 */
class Exporter
{
public:
    Exporter() =default;
    ~Exporter();

    core::Signal<> name_lost;

    void publish(const std::shared_ptr<Actions>& actions,
                 const std::vector<std::shared_ptr<Menu>>& menus);

private:
    static void on_bus_acquired(GDBusConnection*, const gchar *name, gpointer gthis);
    void on_bus_acquired(GDBusConnection*, const gchar *name);

    static void on_name_lost(GDBusConnection*, const gchar *name, gpointer gthis);
    void on_name_lost(GDBusConnection*, const gchar *name);

    std::set<guint> m_exported_menu_ids;
    guint m_own_id = 0;
    guint m_exported_actions_id = 0;
    GDBusConnection * m_dbus_connection = nullptr;
    std::shared_ptr<Actions> m_actions;
    std::vector<std::shared_ptr<Menu>> m_menus;

    // we've got raw pointers and gsignal tags in here, so disable copying
    Exporter(const Exporter&) =delete;
    Exporter& operator=(const Exporter&) =delete;
};

} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_EXPORTER_H
