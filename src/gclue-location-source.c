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
#include "gclue-location-source.h"
#include "gclue-compass.h"

/**
 * SECTION:gclue-location-source
 * @short_description: GeoIP client
 * @include: gclue-glib/gclue-location-source.h
 *
 * The interface all geolocation sources must implement.
 **/

static gboolean
start_source (GClueLocationSource *source);
static gboolean
stop_source (GClueLocationSource *source);

G_DEFINE_ABSTRACT_TYPE (GClueLocationSource, gclue_location_source, G_TYPE_OBJECT)

struct _GClueLocationSourcePrivate
{
        GClueLocation *location;

        guint active_counter;

        GClueAccuracyLevel avail_accuracy_level;

        gboolean compute_movement;

        GClueCompass *compass;

        guint heading_changed_id;
};

enum
{
        PROP_0,
        PROP_LOCATION,
        PROP_ACTIVE,
        PROP_AVAILABLE_ACCURACY_LEVEL,
        PROP_COMPUTE_MOVEMENT,
        LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

static gboolean
set_heading_from_compass (GClueLocationSource *source,
                          GClueLocation       *location)
{
        GClueLocationSourcePrivate *priv = source->priv;
        gdouble heading, curr_heading;

        if (priv->compass == NULL)
                return FALSE;

        heading = gclue_compass_get_heading (priv->compass);
        curr_heading = gclue_location_get_heading (location);

        if (heading == GCLUE_LOCATION_HEADING_UNKNOWN  ||
            heading == curr_heading)
                return FALSE;

        g_debug ("%s got new heading %f", G_OBJECT_TYPE_NAME (source), heading);
        /* We trust heading from compass more than any other source so we always
         * override existing heading
         */
        gclue_location_set_heading (location, heading);

        return TRUE;
}

static void
on_compass_heading_changed (GObject    *gobject,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
        GClueLocationSource* source = GCLUE_LOCATION_SOURCE (user_data);

        if (source->priv->location == NULL)
                return;

        if (set_heading_from_compass (source, source->priv->location))
                g_object_notify (G_OBJECT (source), "location");
}

static void
gclue_location_source_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
        GClueLocationSource *source = GCLUE_LOCATION_SOURCE (object);

        switch (prop_id) {
        case PROP_LOCATION:
                g_value_set_object (value, source->priv->location);
                break;

        case PROP_ACTIVE:
                g_value_set_boolean (value,
                                     gclue_location_source_get_active (source));
                break;

        case PROP_AVAILABLE_ACCURACY_LEVEL:
                g_value_set_enum (value, source->priv->avail_accuracy_level);
                break;

        case PROP_COMPUTE_MOVEMENT:
                g_value_set_boolean (value, source->priv->compute_movement);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_location_source_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        GClueLocationSource *source = GCLUE_LOCATION_SOURCE (object);

        switch (prop_id) {
        case PROP_LOCATION:
        {
                GClueLocation *location = g_value_get_object (value);

                gclue_location_source_set_location (source, location);
                break;
        }

        case PROP_AVAILABLE_ACCURACY_LEVEL:
                source->priv->avail_accuracy_level = g_value_get_enum (value);
                break;

        case PROP_COMPUTE_MOVEMENT:
                source->priv->compute_movement = g_value_get_boolean (value);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
gclue_location_source_finalize (GObject *object)
{
        GClueLocationSourcePrivate *priv = GCLUE_LOCATION_SOURCE (object)->priv;

        gclue_location_source_stop (GCLUE_LOCATION_SOURCE (object));
        g_clear_object (&priv->location);

        G_OBJECT_CLASS (gclue_location_source_parent_class)->finalize (object);
}

static void
gclue_location_source_class_init (GClueLocationSourceClass *klass)
{
        GObjectClass *object_class;

        klass->start = start_source;
        klass->stop = stop_source;

        object_class = G_OBJECT_CLASS (klass);
        object_class->get_property = gclue_location_source_get_property;
        object_class->set_property = gclue_location_source_set_property;
        object_class->finalize = gclue_location_source_finalize;
        g_type_class_add_private (object_class, sizeof (GClueLocationSourcePrivate));

        gParamSpecs[PROP_LOCATION] = g_param_spec_object ("location",
                                                          "Location",
                                                          "Location",
                                                          GCLUE_TYPE_LOCATION,
                                                          G_PARAM_READWRITE);
        g_object_class_install_property (object_class,
                                         PROP_LOCATION,
                                         gParamSpecs[PROP_LOCATION]);

        gParamSpecs[PROP_ACTIVE] = g_param_spec_boolean ("active",
                                                         "Active",
                                                         "Active",
                                                         FALSE,
                                                         G_PARAM_READABLE);
        g_object_class_install_property (object_class,
                                         PROP_ACTIVE,
                                         gParamSpecs[PROP_ACTIVE]);

        gParamSpecs[PROP_AVAILABLE_ACCURACY_LEVEL] =
                g_param_spec_enum ("available-accuracy-level",
                                   "AvailableAccuracyLevel",
                                   "Available accuracy level",
                                   GCLUE_TYPE_ACCURACY_LEVEL,
                                   0,
                                   G_PARAM_READWRITE);
        g_object_class_install_property (object_class,
                                         PROP_AVAILABLE_ACCURACY_LEVEL,
                                         gParamSpecs[PROP_AVAILABLE_ACCURACY_LEVEL]);

        gParamSpecs[PROP_COMPUTE_MOVEMENT] =
                g_param_spec_boolean ("compute-movement",
                                      "ComputeMovement",
                                      "Whether or not, speed and heading should "
                                      "be automatically computed (or fetched "
                                      "from hardware) and set on new locations.",
                                      TRUE,
                                      G_PARAM_READWRITE);
        g_object_class_install_property (object_class,
                                         PROP_COMPUTE_MOVEMENT,
                                         gParamSpecs[PROP_COMPUTE_MOVEMENT]);
}

static void
gclue_location_source_init (GClueLocationSource *source)
{
        source->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (source,
                                             GCLUE_TYPE_LOCATION_SOURCE,
                                             GClueLocationSourcePrivate);
        source->priv->compute_movement = TRUE;
}

static gboolean
start_source (GClueLocationSource *source)
{
        source->priv->active_counter++;
        if (source->priv->active_counter > 1) {
                g_debug ("%s already active, not starting.",
                         G_OBJECT_TYPE_NAME (source));
                return FALSE;
        }

        if (source->priv->compute_movement) {
                source->priv->compass = gclue_compass_get_singleton ();
                source->priv->heading_changed_id = g_signal_connect
                        (G_OBJECT (source->priv->compass),
                         "notify::heading",
                         G_CALLBACK (on_compass_heading_changed),
                         source);
        }

        g_object_notify (G_OBJECT (source), "active");
        g_debug ("%s now active", G_OBJECT_TYPE_NAME (source));
        return TRUE;
}

static gboolean
stop_source (GClueLocationSource *source)
{
        if (source->priv->active_counter == 0) {
                g_debug ("%s already inactive, not stopping.",
                         G_OBJECT_TYPE_NAME (source));
                return FALSE;
        }

        source->priv->active_counter--;
        if (source->priv->active_counter > 0) {
                g_debug ("%s still in use, not stopping.",
                         G_OBJECT_TYPE_NAME (source));
                return FALSE;
        }

        if (source->priv->compass) {
                g_signal_handler_disconnect (source->priv->compass,
                                             source->priv->heading_changed_id);
                g_clear_object (&source->priv->compass);
        }

        g_object_notify (G_OBJECT (source), "active");
        g_debug ("%s now inactive", G_OBJECT_TYPE_NAME (source));

        return TRUE;
}

/**
 * gclue_location_source_start:
 * @source: a #GClueLocationSource
 *
 * Start searching for location and keep an eye on location changes.
 **/
void
gclue_location_source_start (GClueLocationSource *source)
{
        g_return_if_fail (GCLUE_IS_LOCATION_SOURCE (source));

        GCLUE_LOCATION_SOURCE_GET_CLASS (source)->start (source);
}

/**
 * gclue_location_source_stop:
 * @source: a #GClueLocationSource
 *
 * Stop searching for location and no need to keep an eye on location changes
 * anymore.
 **/
void
gclue_location_source_stop (GClueLocationSource *source)
{
        g_return_if_fail (GCLUE_IS_LOCATION_SOURCE (source));

        GCLUE_LOCATION_SOURCE_GET_CLASS (source)->stop (source);
}

/**
 * gclue_location_source_get_location:
 * @source: a #GClueLocationSource
 *
 * Returns: (transfer none): The location, or NULL if unknown.
 **/
GClueLocation *
gclue_location_source_get_location (GClueLocationSource *source)
{
        g_return_val_if_fail (GCLUE_IS_LOCATION_SOURCE (source), NULL);

        return source->priv->location;
}

/**
 * gclue_location_source_set_location:
 * @source: a #GClueLocationSource
 *
 * Set the current location to @location. Its meant to be only used by
 * subclasses.
 **/
void
gclue_location_source_set_location (GClueLocationSource *source,
                                    GClueLocation       *location)
{
        GClueLocationSourcePrivate *priv = source->priv;
        GClueLocation *cur_location;
        gdouble speed, heading;

        cur_location = priv->location;
        priv->location = gclue_location_duplicate (location);

        speed = gclue_location_get_speed (location);
        if (speed == GCLUE_LOCATION_SPEED_UNKNOWN) {
                if (cur_location != NULL && priv->compute_movement) {
                        guint64 cur_timestamp, timestamp;

                        timestamp = geocode_location_get_timestamp
                                        (GEOCODE_LOCATION (location));
                        cur_timestamp = geocode_location_get_timestamp
                                        (GEOCODE_LOCATION (cur_location));

                        if (timestamp != cur_timestamp)
                                gclue_location_set_speed_from_prev_location
                                        (priv->location, cur_location);
                }
        } else {
                gclue_location_set_speed (priv->location, speed);
        }

        set_heading_from_compass (source, location);
        heading = gclue_location_get_heading (location);
        if (heading == GCLUE_LOCATION_HEADING_UNKNOWN) {
                if (cur_location != NULL && priv->compute_movement)
                        gclue_location_set_heading_from_prev_location
                                (priv->location, cur_location);
        } else {
                gclue_location_set_heading (priv->location, heading);
        }

        g_object_notify (G_OBJECT (source), "location");
        g_clear_object (&cur_location);
}

/**
 * gclue_location_source_get_active:
 * @source: a #GClueLocationSource
 *
 * Returns: TRUE if source is active, FALSE otherwise.
 **/
gboolean
gclue_location_source_get_active (GClueLocationSource *source)
{
        g_return_val_if_fail (GCLUE_IS_LOCATION_SOURCE (source), FALSE);

        return (source->priv->active_counter > 0);
}

/**
 * gclue_location_source_get_available_accuracy_level:
 * @source: a #GClueLocationSource
 *
 * Returns: The currently available accuracy level.
 **/
GClueAccuracyLevel
gclue_location_source_get_available_accuracy_level (GClueLocationSource *source)
{
        g_return_val_if_fail (GCLUE_IS_LOCATION_SOURCE (source), 0);

        return source->priv->avail_accuracy_level;
}

/**
 * gclue_location_source_get_compute_movement
 * @source: a #GClueLocationSource
 *
 * Returns: %TRUE if speed and heading will be automatically computed (or
 * fetched from hardware) and set on new locations, %FALSE otherwise.
 **/
gboolean
gclue_location_source_get_compute_movement (GClueLocationSource *source)
{
        g_return_val_if_fail (GCLUE_IS_LOCATION_SOURCE (source), FALSE);

        return source->priv->compute_movement;
}

/**
 * gclue_location_source_set_compute_movement
 * @source: a #GClueLocationSource
 * @compute: a #gboolean
 *
 * Use this to specify whether or not you want @source to automatically compute
 * (or fetch from hardware) and set speed and heading on new locations.
 **/
void
gclue_location_source_set_compute_movement (GClueLocationSource *source,
                                            gboolean             compute)
{
        g_return_if_fail (GCLUE_IS_LOCATION_SOURCE (source));

        source->priv->compute_movement = compute;
}
