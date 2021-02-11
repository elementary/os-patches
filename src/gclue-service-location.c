/* vim: set et ts=8 sw=8: */
/* gclue-service-location.c
 *
 * Copyright 2013 Red Hat, Inc.
 *
 * Geoclue is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Geoclue is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Geoclue; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#include <glib/gi18n.h>

#include "gclue-service-location.h"

static void
gclue_service_location_initable_iface_init (GInitableIface *iface);

struct _GClueServiceLocationPrivate
{
        GClueClientInfo *client_info;
        char *path;
        GDBusConnection *connection;
};

G_DEFINE_TYPE_WITH_CODE (GClueServiceLocation,
                         gclue_service_location,
                         GCLUE_DBUS_TYPE_LOCATION_SKELETON,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                gclue_service_location_initable_iface_init)
                         G_ADD_PRIVATE (GClueServiceLocation));

enum
{
        PROP_0,
        PROP_CLIENT_INFO,
        PROP_PATH,
        PROP_CONNECTION,
        PROP_LOCATION,
        LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

static void
gclue_service_location_finalize (GObject *object)
{
        GClueServiceLocationPrivate *priv = GCLUE_SERVICE_LOCATION (object)->priv;

        g_clear_pointer (&priv->path, g_free);
        g_clear_object (&priv->connection);
        g_clear_object (&priv->client_info);

        /* Chain up to the parent class */
        G_OBJECT_CLASS (gclue_service_location_parent_class)->finalize (object);
}

static void
gclue_service_location_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
        GClueServiceLocation *self = GCLUE_SERVICE_LOCATION (object);

        switch (prop_id) {
        case PROP_CLIENT_INFO:
                g_value_set_object (value, self->priv->client_info);
                break;

        case PROP_PATH:
                g_value_set_string (value, self->priv->path);
                break;

        case PROP_CONNECTION:
                g_value_set_object (value, self->priv->connection);
                break;

        case PROP_LOCATION:
        {
                GClueDBusLocation *location;
                GClueLocation *loc;
                GVariant *timestamp;
                guint64 sec, usec;

                location = GCLUE_DBUS_LOCATION (object);

                timestamp = gclue_dbus_location_get_timestamp (location);
                g_variant_get (timestamp, "(tt)", &sec, &usec);

                loc = gclue_location_new_full
                        (gclue_dbus_location_get_latitude (location),
                         gclue_dbus_location_get_longitude (location),
                         gclue_dbus_location_get_accuracy (location),
                         gclue_dbus_location_get_speed (location),
                         gclue_dbus_location_get_heading (location),
                         gclue_dbus_location_get_altitude (location),
                         sec,
                         gclue_dbus_location_get_description (location));

                g_value_take_object (value, loc);
                break;
        }

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_service_location_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
        GClueServiceLocation *self = GCLUE_SERVICE_LOCATION (object);

        switch (prop_id) {
        case PROP_CLIENT_INFO:
                self->priv->client_info = g_value_dup_object (value);
                break;

        case PROP_PATH:
                self->priv->path = g_value_dup_string (value);
                break;

        case PROP_CONNECTION:
                self->priv->connection = g_value_dup_object (value);
                break;

        case PROP_LOCATION:
        {
                GClueDBusLocation *location;
                GClueLocation *loc;
                gdouble altitude;
                GVariant *timestamp;

                location = GCLUE_DBUS_LOCATION (object);
                loc = g_value_get_object (value);
                gclue_dbus_location_set_latitude
                        (location, gclue_location_get_latitude (loc));
                gclue_dbus_location_set_longitude
                        (location, gclue_location_get_longitude (loc));
                gclue_dbus_location_set_accuracy
                        (location, gclue_location_get_accuracy (loc));
                gclue_dbus_location_set_description
                        (location, gclue_location_get_description (loc));
                gclue_dbus_location_set_speed
                        (location, gclue_location_get_speed (loc));
                gclue_dbus_location_set_heading
                        (location, gclue_location_get_heading (loc));
                timestamp = g_variant_new
                        ("(tt)",
                         (guint64) gclue_location_get_timestamp (loc),
                         (guint64) 0);
                gclue_dbus_location_set_timestamp
                        (location, timestamp);
                altitude = gclue_location_get_altitude (loc);
                if (altitude != GCLUE_LOCATION_ALTITUDE_UNKNOWN)
                        gclue_dbus_location_set_altitude (location, altitude);
                break;
        }

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_service_location_handle_method_call (GDBusConnection       *connection,
                                           const gchar           *sender,
                                           const gchar           *object_path,
                                           const gchar           *interface_name,
                                           const gchar           *method_name,
                                           GVariant              *parameters,
                                           GDBusMethodInvocation *invocation,
                                           gpointer               user_data)
{
        GClueServiceLocationPrivate *priv = GCLUE_SERVICE_LOCATION (user_data)->priv;
        GDBusInterfaceSkeletonClass *skeleton_class;
        GDBusInterfaceVTable *skeleton_vtable;

        if (!gclue_client_info_check_bus_name (priv->client_info, sender)) {
                g_dbus_method_invocation_return_error_literal (invocation,
                                                               G_DBUS_ERROR,
                                                               G_DBUS_ERROR_ACCESS_DENIED,
                                                               "Access denied");
                return;
        }

        skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (gclue_service_location_parent_class);
        skeleton_vtable = skeleton_class->get_vtable (G_DBUS_INTERFACE_SKELETON (user_data));
        skeleton_vtable->method_call (connection,
                                      sender,
                                      object_path,
                                      interface_name,
                                      method_name,
                                      parameters,
                                      invocation,
                                      user_data);
}

static GVariant *
gclue_service_location_handle_get_property (GDBusConnection *connection,
                                            const gchar     *sender,
                                            const gchar     *object_path,
                                            const gchar     *interface_name,
                                            const gchar     *property_name,
                                            GError         **error,
                                            gpointer        user_data)
{
        GClueServiceLocationPrivate *priv = GCLUE_SERVICE_LOCATION (user_data)->priv;
        GDBusInterfaceSkeletonClass *skeleton_class;
        GDBusInterfaceVTable *skeleton_vtable;

        if (!gclue_client_info_check_bus_name (priv->client_info, sender)) {
                g_set_error (error,
                             G_DBUS_ERROR,
                             G_DBUS_ERROR_ACCESS_DENIED,
                             "Access denied");
                return NULL;
        }

        skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (gclue_service_location_parent_class);
        skeleton_vtable = skeleton_class->get_vtable (G_DBUS_INTERFACE_SKELETON (user_data));
        return skeleton_vtable->get_property (connection,
                                              sender,
                                              object_path,
                                              interface_name,
                                              property_name,
                                              error,
                                              user_data);
}

static gboolean
gclue_service_location_handle_set_property (GDBusConnection *connection,
                                            const gchar     *sender,
                                            const gchar     *object_path,
                                            const gchar     *interface_name,
                                            const gchar     *property_name,
                                            GVariant        *variant,
                                            GError         **error,
                                            gpointer        user_data)
{
        GClueServiceLocationPrivate *priv = GCLUE_SERVICE_LOCATION (user_data)->priv;
        GDBusInterfaceSkeletonClass *skeleton_class;
        GDBusInterfaceVTable *skeleton_vtable;

        if (!gclue_client_info_check_bus_name (priv->client_info, sender)) {
                g_set_error (error,
                             G_DBUS_ERROR,
                             G_DBUS_ERROR_ACCESS_DENIED,
                             "Access denied");
                return FALSE;
        }

        skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (gclue_service_location_parent_class);
        skeleton_vtable = skeleton_class->get_vtable (G_DBUS_INTERFACE_SKELETON (user_data));
        return skeleton_vtable->set_property (connection,
                                              sender,
                                              object_path,
                                              interface_name,
                                              property_name,
                                              variant,
                                              error,
                                              user_data);
}

static const GDBusInterfaceVTable gclue_service_location_vtable =
{
        gclue_service_location_handle_method_call,
        gclue_service_location_handle_get_property,
        gclue_service_location_handle_set_property,
        {NULL}
};

static GDBusInterfaceVTable *
gclue_service_location_get_vtable (GDBusInterfaceSkeleton *skeleton G_GNUC_UNUSED)
{
        return (GDBusInterfaceVTable *) &gclue_service_location_vtable;
}

static void
gclue_service_location_class_init (GClueServiceLocationClass *klass)
{
        GObjectClass *object_class;
        GDBusInterfaceSkeletonClass *skeleton_class;

        object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gclue_service_location_finalize;
        object_class->get_property = gclue_service_location_get_property;
        object_class->set_property = gclue_service_location_set_property;

        skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (klass);
        skeleton_class->get_vtable = gclue_service_location_get_vtable;

        gParamSpecs[PROP_CLIENT_INFO] = g_param_spec_object ("client-info",
                                                             "ClientInfo",
                                                             "Information on client",
                                                             GCLUE_TYPE_CLIENT_INFO,
                                                             G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class,
                                         PROP_CLIENT_INFO,
                                         gParamSpecs[PROP_CLIENT_INFO]);

        gParamSpecs[PROP_PATH] = g_param_spec_string ("path",
                                                      "Path",
                                                      "Path",
                                                      NULL,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class,
                                         PROP_PATH,
                                         gParamSpecs[PROP_PATH]);

        gParamSpecs[PROP_CONNECTION] = g_param_spec_object ("connection",
                                                            "Connection",
                                                            "DBus Connection",
                                                            G_TYPE_DBUS_CONNECTION,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class,
                                         PROP_CONNECTION,
                                         gParamSpecs[PROP_CONNECTION]);

        gParamSpecs[PROP_LOCATION] = g_param_spec_object ("location",
                                                          "Location",
                                                          "Location",
                                                          GCLUE_TYPE_LOCATION,
                                                          G_PARAM_READWRITE);
        g_object_class_install_property (object_class,
                                         PROP_LOCATION,
                                         gParamSpecs[PROP_LOCATION]);
}

static void
gclue_service_location_init (GClueServiceLocation *location)
{
        location->priv = G_TYPE_INSTANCE_GET_PRIVATE (location,
                                                      GCLUE_TYPE_SERVICE_LOCATION,
                                                      GClueServiceLocationPrivate);
        gclue_dbus_location_set_altitude (GCLUE_DBUS_LOCATION (location),
                                          GCLUE_LOCATION_ALTITUDE_UNKNOWN);
}

static gboolean
gclue_service_location_initable_init (GInitable    *initable,
                                      GCancellable *cancellable,
                                      GError      **error)
{
        return g_dbus_interface_skeleton_export
                (G_DBUS_INTERFACE_SKELETON (initable),
                 GCLUE_SERVICE_LOCATION (initable)->priv->connection,
                 GCLUE_SERVICE_LOCATION (initable)->priv->path,
                 error);
}

static void
gclue_service_location_initable_iface_init (GInitableIface *iface)
{
        iface->init = gclue_service_location_initable_init;
}

GClueServiceLocation *
gclue_service_location_new (GClueClientInfo *info,
                            const char      *path,
                            GDBusConnection *connection,
                            GClueLocation   *location,
                            GError         **error)
{
        return g_initable_new (GCLUE_TYPE_SERVICE_LOCATION,
                               NULL,
                               error,
                               "client-info", info,
                               "path", path,
                               "connection", connection,
                               "location", location,
                               NULL);
}

const gchar *
gclue_service_location_get_path (GClueServiceLocation *location)
{
        g_return_val_if_fail (GCLUE_IS_SERVICE_LOCATION(location), NULL);

        return location->priv->path;
}
