/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
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
 */

#include <datetime/timezone-geoclue.h>

#define GEOCLUE_BUS_NAME "org.freedesktop.Geoclue.Master"

namespace unity {
namespace indicator {
namespace datetime {


GeoclueTimezone::GeoclueTimezone():
    m_cancellable(g_cancellable_new())
{
    g_bus_get(G_BUS_TYPE_SESSION, m_cancellable, on_bus_got, this);
}

GeoclueTimezone::~GeoclueTimezone()
{
    g_cancellable_cancel(m_cancellable);
    g_object_unref(m_cancellable);

    if (m_signal_subscription)
        g_dbus_connection_signal_unsubscribe(m_connection, m_signal_subscription);

    g_object_unref(m_connection);
}

/***
****
***/

void
GeoclueTimezone::on_bus_got(GObject*      /*source*/,
                            GAsyncResult*   res,
                            gpointer        gself)
{
    GError * error;
    GDBusConnection * connection;

    error = nullptr;
    connection = g_bus_get_finish(res, &error);
    if (error)
    {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning("Couldn't get bus: %s", error->message);

        g_error_free(error);
    }
    else
    {
        auto self = static_cast<GeoclueTimezone*>(gself);

        self->m_connection = connection;

        g_dbus_connection_call(self->m_connection,
                               GEOCLUE_BUS_NAME,
                               "/org/freedesktop/Geoclue/Master",
                               "org.freedesktop.Geoclue.Master",
                               "Create",
                               nullptr, // parameters
                               G_VARIANT_TYPE("(o)"),
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               self->m_cancellable,
                               on_client_created,
                               self);
    }
}

void
GeoclueTimezone::on_client_created(GObject * source, GAsyncResult * res, gpointer gself)
{
    GVariant * result;

    if ((result = call_finish(source, res)))
    {
        auto self = static_cast<GeoclueTimezone*>(gself);

        GVariant * child = g_variant_get_child_value(result, 0);
        self->m_client_object_path = g_variant_get_string(child, nullptr);
        g_variant_unref(child);
        g_variant_unref(result);

        self->m_signal_subscription = g_dbus_connection_signal_subscribe(
                    self->m_connection,
                    GEOCLUE_BUS_NAME,
                    "org.freedesktop.Geoclue.Address", // inteface
                    "AddressChanged", // signal name
                    self->m_client_object_path.c_str(), // object path
                    nullptr, // arg0
                    G_DBUS_SIGNAL_FLAGS_NONE,
                    on_address_changed,
                    self,
                    nullptr);

        g_dbus_connection_call(self->m_connection,
                               GEOCLUE_BUS_NAME,
                               self->m_client_object_path.c_str(),
                               "org.freedesktop.Geoclue.MasterClient",
                               "SetRequirements",
                               g_variant_new("(iibi)", 2, 0, FALSE, 1023),
                               nullptr,
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               self->m_cancellable,
                               on_requirements_set,
                               self);
    }
}

void
GeoclueTimezone::on_address_changed(GDBusConnection* /*connection*/,
                                    const gchar*     /*sender_name*/,
                                    const gchar*     /*object_path*/,
                                    const gchar*     /*interface_name*/,
                                    const gchar*     /*signal_name*/,
                                    GVariant*          parameters,
                                    gpointer           gself)
{
    static_cast<GeoclueTimezone*>(gself)->setTimezoneFromAddressVariant(parameters);
}

void
GeoclueTimezone::on_requirements_set(GObject* source, GAsyncResult* res, gpointer gself)
{
    GVariant * result;

    if ((result = call_finish(source, res)))
    {
        auto self = static_cast<GeoclueTimezone*>(gself);

        g_dbus_connection_call(self->m_connection,
                               GEOCLUE_BUS_NAME,
                               self->m_client_object_path.c_str(),
                               "org.freedesktop.Geoclue.MasterClient",
                               "AddressStart",
                               nullptr,
                               nullptr,
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               self->m_cancellable,
                               on_address_started,
                               self);

        g_variant_unref(result);
    }
}

void
GeoclueTimezone::on_address_started(GObject * source, GAsyncResult * res, gpointer gself)
{
    GVariant * result;

    if ((result = call_finish(source, res)))
    {
        auto self = static_cast<GeoclueTimezone*>(gself);

        g_dbus_connection_call(self->m_connection,
                               GEOCLUE_BUS_NAME,
                               self->m_client_object_path.c_str(),
                               "org.freedesktop.Geoclue.Address",
                               "GetAddress",
                               nullptr,
                               G_VARIANT_TYPE("(ia{ss}(idd))"),
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               self->m_cancellable,
                               on_address_got,
                               self);

        g_variant_unref(result);
    }
}

void
GeoclueTimezone::on_address_got(GObject * source, GAsyncResult * res, gpointer gself)
{
    GVariant * result;

    if ((result = call_finish(source, res)))
    {
        static_cast<GeoclueTimezone*>(gself)->setTimezoneFromAddressVariant(result);
        g_variant_unref(result);
    }
}

void
GeoclueTimezone::setTimezoneFromAddressVariant(GVariant * variant)
{
    g_return_if_fail(g_variant_is_of_type(variant, G_VARIANT_TYPE("(ia{ss}(idd))")));

    const gchar * timezone_string = nullptr;
    GVariant * dict = g_variant_get_child_value(variant, 1);
    if (dict)
    {
        if (g_variant_lookup(dict, "timezone", "&s", &timezone_string))
            timezone.set(timezone_string);

        g_variant_unref(dict);
    }
}

GVariant*
GeoclueTimezone::call_finish(GObject * source, GAsyncResult * res)
{
    GError * error;
    GVariant * result;

    error = nullptr;
    result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &error);

    if (error)
    {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("AddressStart() failed: %s", error->message);

            g_error_free(error);

            g_clear_pointer(&result, g_variant_unref);
    }

    return result;
}

/****
*****
****/

} // namespace datetime
} // namespace indicator
} // namespace unity

