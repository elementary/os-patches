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

#ifndef INDICATOR_DATETIME_GEOCLUE_TIMEZONE_H
#define INDICATOR_DATETIME_GEOCLUE_TIMEZONE_H

#include <datetime/timezone.h> // base class

#include <string>

#include <glib.h>
#include <gio/gio.h>

namespace unity {
namespace indicator {
namespace datetime {

/**
 * \brief A #Timezone that gets its information from asking GeoClue
 */
class GeoclueTimezone: public Timezone
{
public:
    GeoclueTimezone();
    ~GeoclueTimezone();

private:
    static void on_bus_got (GObject*, GAsyncResult*, gpointer);
    static void on_client_created (GObject*, GAsyncResult*, gpointer);
    static void on_address_changed (GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant*, gpointer);
    static void on_requirements_set (GObject*, GAsyncResult*, gpointer);
    static void on_address_started (GObject*, GAsyncResult*, gpointer);
    static void on_address_got (GObject*, GAsyncResult*, gpointer);
    void setTimezoneFromAddressVariant (GVariant*);
    static GVariant * call_finish (GObject*, GAsyncResult*);

    GCancellable * m_cancellable = nullptr;
    GDBusConnection * m_connection = nullptr;
    std::string m_client_object_path;
    guint m_signal_subscription = 0;

    // we've got pointers and gsignal tags in here, so don't allow copying
    GeoclueTimezone(const GeoclueTimezone&) =delete;
    GeoclueTimezone& operator=(const GeoclueTimezone&) =delete;
};


} // namespace datetime
} // namespace indicator
} // namespace unity

#endif // INDICATOR_DATETIME_GEOCLUE_TIMEZONE_H

