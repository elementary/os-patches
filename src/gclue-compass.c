/* vim: set et ts=8 sw=8: */
/*
 * Copyright (C) 2014 Red Hat, Inc.
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
 * Authors: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#include <glib.h>
#include "gclue-compass.h"
#include "gclue-location.h"
#include "compass-interface.h"

/**
 * SECTION:gclue-compass
 * @short_description: IIO compass support
 * @include: gclue-glib/gclue-compass.h
 **/

G_DEFINE_TYPE (GClueCompass, gclue_compass, G_TYPE_OBJECT)

struct _GClueCompassPrivate
{
        Compass *proxy;

        GCancellable *cancellable;
};

enum
{
        PROP_0,
        PROP_HEADING,
        LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

static void
gclue_compass_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
        GClueCompass *compass = GCLUE_COMPASS (object);

        switch (prop_id) {
        case PROP_HEADING:
                g_value_set_double (value,
                                    gclue_compass_get_heading (compass));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_compass_finalize (GObject *object)
{
        GClueCompassPrivate *priv = GCLUE_COMPASS (object)->priv;

        g_cancellable_cancel (priv->cancellable);
        g_clear_object (&priv->cancellable);

        if (priv->proxy != NULL) {
                GError *error = NULL;

                if (!compass_call_release_compass_sync (priv->proxy,
                                                        NULL,
                                                        &error)) {
                        g_warning ("Failed to release compass: %s",
                                   error->message);
                        g_error_free (error);
                }
                g_debug ("IIO compass released");
                g_object_unref (priv->proxy);
        }

        G_OBJECT_CLASS (gclue_compass_parent_class)->finalize (object);
}

static void
gclue_compass_class_init (GClueCompassClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);
        object_class->get_property = gclue_compass_get_property;
        object_class->finalize = gclue_compass_finalize;
        g_type_class_add_private (object_class, sizeof (GClueCompassPrivate));

        /**
         * GClueCompass:heading
         *
         * The positive angle between the direction of movement and the North
         * direction, in clockwise direction. The angle is measured in degrees.
         */
        gParamSpecs[PROP_HEADING] =
                g_param_spec_double ("heading",
                                     "Heading",
                                     "The positive angle between the direction"
                                     " of movement and the North direction, in"
                                     " clockwise direction. The angle is "
                                     "measured in degrees.",
                                     GCLUE_LOCATION_HEADING_UNKNOWN,
                                     G_MAXDOUBLE,
                                     GCLUE_LOCATION_HEADING_UNKNOWN,
                                     G_PARAM_READABLE |
                                     G_PARAM_STATIC_STRINGS);
        g_object_class_install_property (object_class, PROP_HEADING, gParamSpecs[PROP_HEADING]);
}

static void
on_compass_heading_changed (GObject    *gobject,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
        g_object_notify_by_pspec (G_OBJECT (user_data),
                                  gParamSpecs[PROP_HEADING]);
}

static void
on_compass_claimed (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
        GClueCompass *compass;
        Compass *proxy = COMPASS (source_object);
        GError *error = NULL;

        if (!compass_call_claim_compass_finish (proxy, res, &error)) {
                if (error->code != G_IO_ERROR_CANCELLED)
                        g_debug ("Failed to claim IIO proxy compass: %s",
                                 error->message);
                g_error_free (error);
                g_object_unref (proxy);

                return;
        }
        g_debug ("IIO compass claimed");

        compass = GCLUE_COMPASS (user_data);
        compass->priv->proxy = proxy;

        g_object_notify_by_pspec (G_OBJECT (compass),
                                  gParamSpecs[PROP_HEADING]);
        g_signal_connect_object (G_OBJECT (proxy),
                                 "notify::compass-heading",
                                 G_CALLBACK (on_compass_heading_changed),
                                 compass,
                                 G_CONNECT_AFTER);
}

static void
on_compass_proxy_ready (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
        GClueCompass *compass;
        Compass *proxy;
        GError *error = NULL;

        proxy = compass_proxy_new_for_bus_finish (res, &error);
        if (proxy == NULL) {
                if (error->code != G_IO_ERROR_CANCELLED)
                        g_debug ("Failed to connect to IIO compass proxy: %s",
                                 error->message);
                g_error_free (error);

                return;
        }

        compass = GCLUE_COMPASS (user_data);

        compass_call_claim_compass (proxy, 
                                    compass->priv->cancellable,
                                    on_compass_claimed,
                                    compass);
}

static void
gclue_compass_init (GClueCompass *compass)
{
        compass->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (compass,
                                             GCLUE_TYPE_COMPASS,
                                             GClueCompassPrivate);
        compass->priv->cancellable = g_cancellable_new ();

        compass_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   "net.hadess.SensorProxy",
                                   "/net/hadess/SensorProxy/Compass",
                                   compass->priv->cancellable,
                                   on_compass_proxy_ready,
                                   compass);
}

static void
on_compass_destroyed (gpointer data,
                      GObject *where_the_object_was)
{
        GClueCompass **compass = (GClueCompass **) data;

        *compass = NULL;
}

/**
 * gclue_compass_get_singleton:
 *
 * Get the #GClueCompass singleton.
 *
 * Returns: (transfer full): a new ref to #GClueCompass. Use g_object_unref()
 * when done.
 **/
GClueCompass *
gclue_compass_get_singleton (void)
{
        static GClueCompass *compass = NULL;

        if (compass == NULL) {
                compass = g_object_new (GCLUE_TYPE_COMPASS, NULL);
                g_object_weak_ref (G_OBJECT (compass),
                                   on_compass_destroyed,
                                   &compass);
        } else
                g_object_ref (compass);

        return compass;
}

/**
 * gclue_compass_get_heading:
 * @compass: a #GClueCompass
 *
 * Gets the positive angle between direction of movement and North direction.
 * The angle is measured in degrees.
 *
 * Returns: The heading, or %GCLUE_LOCATION_HEADING_UNKNOWN if heading is
 *          unknown.
 **/
gdouble
gclue_compass_get_heading (GClueCompass *compass)
{
        gdouble ret = GCLUE_LOCATION_HEADING_UNKNOWN;

        g_return_val_if_fail (GCLUE_IS_COMPASS (compass), ret);

        if (compass->priv->proxy == NULL)
                return GCLUE_LOCATION_HEADING_UNKNOWN;

        /* FIXME:
         *
         * IIO compass gives us raw magnetic heading so we need to translate it
         * to true heading here. Some pointers on that from elad:
         *
         * A Python implementation:
         * https://github.com/cmweiss/geomag/blob/master/geomag/geomag/geomag.py
         *
         * It seems to use the magnetic model from NOAA:
         * http://www.ngdc.noaa.gov/geomag/WMM/
         *
         * C implementation: http://www.ngdc.noaa.gov/geomag/WMM/soft.shtml
         */
        ret = compass_get_compass_heading (compass->priv->proxy);

        if (ret >= 0)
                return ret;
        else
                return GCLUE_LOCATION_HEADING_UNKNOWN;
}
