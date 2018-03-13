/* vim: set et ts=8 sw=8: */
/* gclue-location.c
 *
 * Copyright (C) 2012 Bastien Nocera
 * Copyright (C) 2015 Ankit (Verma)
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
 *    Authors: Bastien Nocera <hadess@hadess.net>
 *             Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *             Ankit (Verma) <ankitstarski@gmail.com>
 */

#include "gclue-location.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define TIME_DIFF_THRESHOLD 60

struct _GClueLocationPrivate {
        gdouble speed;
        gdouble heading;
};

enum {
        PROP_0,

        PROP_SPEED,
        PROP_HEADING,
};

G_DEFINE_TYPE (GClueLocation, gclue_location, GEOCODE_TYPE_LOCATION);

static void
gclue_location_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
        GClueLocation *location = GCLUE_LOCATION (object);

        switch (property_id) {
        case PROP_SPEED:
                g_value_set_double (value,
                                    gclue_location_get_speed (location));
                break;

        case PROP_HEADING:
                g_value_set_double (value,
                                    gclue_location_get_heading (location));
                break;

        default:
                /* We don't have any other property... */
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gclue_location_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
        GClueLocation *location = GCLUE_LOCATION (object);

        switch (property_id) {
        case PROP_SPEED:
                gclue_location_set_speed (location,
                                          g_value_get_double (value));
                break;

        case PROP_HEADING:
                gclue_location_set_heading (location,
                                            g_value_get_double (value));
                break;

        default:
                /* We don't have any other property... */
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gclue_location_finalize (GObject *glocation)
{
        G_OBJECT_CLASS (gclue_location_parent_class)->finalize (glocation);
}

static void
gclue_location_class_init (GClueLocationClass *klass)
{
        GObjectClass *glocation_class = G_OBJECT_CLASS (klass);
        GParamSpec *pspec;

        glocation_class->finalize = gclue_location_finalize;
        glocation_class->get_property = gclue_location_get_property;
        glocation_class->set_property = gclue_location_set_property;

        g_type_class_add_private (klass, sizeof (GClueLocationPrivate));

        /**
         * GClueLocation:speed
         *
         * The speed in meters per second.
         */
        pspec = g_param_spec_double ("speed",
                                     "Speed",
                                     "Speed in meters per second",
                                     GCLUE_LOCATION_SPEED_UNKNOWN,
                                     G_MAXDOUBLE,
                                     GCLUE_LOCATION_SPEED_UNKNOWN,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
        g_object_class_install_property (glocation_class, PROP_SPEED, pspec);

        /**
         * GClueLocation:heading
         *
         * The positive angle between the direction of movement and the North
         * direction, in clockwise direction. The angle is measured in degrees.
         */
        pspec = g_param_spec_double ("heading",
                                     "Heading",
                                     "The positive Angle between the direction"
                                     " of movement and the North direction, in"
                                     " clockwise direction. The angle is "
                                     "measured in degrees.",
                                     GCLUE_LOCATION_HEADING_UNKNOWN,
                                     G_MAXDOUBLE,
                                     GCLUE_LOCATION_HEADING_UNKNOWN,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
        g_object_class_install_property (glocation_class, PROP_HEADING, pspec);
}

static void
gclue_location_init (GClueLocation *location)
{
        location->priv = G_TYPE_INSTANCE_GET_PRIVATE ((location),
                                                      GCLUE_TYPE_LOCATION,
                                                      GClueLocationPrivate);

        location->priv->speed = GCLUE_LOCATION_SPEED_UNKNOWN;
        location->priv->heading = GCLUE_LOCATION_HEADING_UNKNOWN;
}

static gdouble
get_accuracy_from_hdop (gdouble hdop)
{
        /* FIXME: These are really just rough estimates based on:
         *        http://en.wikipedia.org/wiki/Dilution_of_precision_%28GPS%29#Meaning_of_DOP_Values
         */
        if (hdop <= 1)
                return 0;
        else if (hdop <= 2)
                return 1;
        else if (hdop <= 5)
                return 3;
        else if (hdop <= 10)
                return 50;
        else if (hdop <= 20)
                return 100;
        else
                return 300;
}

static gdouble
parse_coordinate_string (const char *coordinate,
                         const char *direction)
{
        gdouble minutes, degrees, out;
        gchar *degrees_str;
        gchar *dot_str;
        gint dot_offset;

        if (coordinate[0] == '\0' ||
            direction[0] == '\0' ||
            direction[0] == '\0')
                return INVALID_COORDINATE;

        if (direction[0] != 'N' &&
            direction[0] != 'S' &&
            direction[0] != 'E' &&
            direction[0] != 'W') {
                g_warning ("Unknown direction '%s' for coordinates, ignoring..",
                           direction);
                return INVALID_COORDINATE;
        }

        dot_str = g_strstr_len (coordinate, 6, ".");
        if (dot_str == NULL)
                return INVALID_COORDINATE;
        dot_offset = dot_str - coordinate;

        degrees_str = g_strndup (coordinate, dot_offset - 2);
        degrees = g_ascii_strtod (degrees_str, NULL);
        g_free (degrees_str);

        minutes = g_ascii_strtod (dot_str - 2, NULL);

        /* Include the minutes as part of the degrees */
        out = degrees + (minutes / 60.0);

        if (direction[0] == 'S' || direction[0] == 'W')
                out = 0 - out;

        return out;
}

static gdouble
parse_altitude_string (const char *altitude,
                       const char *unit)
{
        if (altitude[0] == '\0' || unit[0] == '\0')
                return GEOCODE_LOCATION_ALTITUDE_UNKNOWN;

        if (unit[0] != 'M') {
                g_warning ("Unknown unit '%s' for altitude, ignoring..",
                           unit);

                return GEOCODE_LOCATION_ALTITUDE_UNKNOWN;
        }

        return g_ascii_strtod (altitude, NULL);
}

static gint64
parse_nmea_timestamp (const char *nmea_ts)
{
        char parts[3][3];
        int i, hours, minutes, seconds;
        GDateTime *now, *ts = NULL;
        guint64 ret;

        now = g_date_time_new_now_utc ();
        ret = g_date_time_to_unix (now);

        if (strlen (nmea_ts) < 6) {
                if (strlen (nmea_ts) >= 1)
                        /* Empty string just means no ts, so no warning */
                        g_warning ("Failed to parse NMEA timestamp '%s'",
                                   nmea_ts);

                goto parse_error;
        }

        for (i = 0; i < 3; i++) {
                memmove (parts[i], nmea_ts + (i * 2), 2);
                parts[i][2] = '\0';
        }
        hours = atoi (parts[0]);
        minutes = atoi (parts[1]);
        seconds = atoi (parts[2]);

        ts = g_date_time_new_utc (g_date_time_get_year (now),
                                  g_date_time_get_month (now),
                                  g_date_time_get_day_of_month (now),
                                  hours,
                                  minutes,
                                  seconds);

        if (g_date_time_difference (ts, now) > TIME_DIFF_THRESHOLD) {
                g_debug ("NMEA timestamp '%s' in future. Assuming yesterday's.",
                         nmea_ts);
                g_date_time_unref (ts);

                ts = g_date_time_new_utc (g_date_time_get_year (now),
                                          g_date_time_get_month (now),
                                          g_date_time_get_day_of_month (now) - 1,
                                          hours,
                                          minutes,
                                          seconds);
        }

        ret = g_date_time_to_unix (ts);
        g_date_time_unref (ts);
parse_error:
        g_date_time_unref (now);

        return ret;
}

/**
 * gclue_location_new:
 * @latitude: a valid latitude
 * @longitude: a valid longitude
 * @accuracy: accuracy of location in meters
 *
 * Creates a new #GClueLocation object.
 *
 * Returns: a new #GClueLocation object. Use g_object_unref() when done.
 **/
GClueLocation *
gclue_location_new (gdouble latitude,
                    gdouble longitude,
                    gdouble accuracy)
{
        return g_object_new (GCLUE_TYPE_LOCATION,
                             "latitude", latitude,
                             "longitude", longitude,
                             "accuracy", accuracy,
                             NULL);
}

/**
 * gclue_location_new_full:
 * @latitude: a valid latitude
 * @longitude: a valid longitude
 * @accuracy: accuracy of location in meters
 * @speed: speed in meters per second
 * @heading: heading in degrees
 * @altitude: altitude of location in meters
 * @timestamp: timestamp in seconds since the Epoch
 * @description: a description for the location
 *
 * Creates a new #GClueLocation object.
 *
 * Returns: a new #GClueLocation object. Use g_object_unref() when done.
 **/
GClueLocation *
gclue_location_new_full (gdouble     latitude,
                         gdouble     longitude,
                         gdouble     accuracy,
                         gdouble     speed,
                         gdouble     heading,
                         gdouble     altitude,
                         guint64     timestamp,
                         const char *description)
{
        return g_object_new (GCLUE_TYPE_LOCATION,
                             "latitude", latitude,
                             "longitude", longitude,
                             "accuracy", accuracy,
                             "speed", speed,
                             "heading", heading,
                             "altitude", altitude,
                             "timestamp", timestamp,
                             "description", description,
                             NULL);
}

/**
 * gclue_location_create_from_gga:
 * @gga: NMEA GGA sentence
 * @error: Place-holder for errors.
 *
 * Creates a new #GClueLocation object from a GGA sentence.
 *
 * Returns: a new #GClueLocation object, or %NULL on error. Unref using
 * #g_object_unref() when done with it.
 **/
GClueLocation *
gclue_location_create_from_gga (const char *gga, GError **error)
{
        GClueLocation *location = NULL;
        gdouble latitude, longitude, accuracy, altitude;
        gdouble hdop; /* Horizontal Dilution Of Precision */
        guint64 timestamp;
        char **parts;

        parts = g_strsplit (gga, ",", -1);
        if (g_strv_length (parts) < 14) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_INVALID_ARGUMENT,
                                     "Invalid NMEA GGA sentence");
                goto out;
        }

        /* For syntax of GGA sentences:
         * http://www.gpsinformation.org/dale/nmea.htm#GGA
         */
        timestamp = parse_nmea_timestamp (parts[1]);
        latitude = parse_coordinate_string (parts[2], parts[3]);
        longitude = parse_coordinate_string (parts[4], parts[5]);
        if (latitude == INVALID_COORDINATE || longitude == INVALID_COORDINATE) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     G_IO_ERROR_INVALID_ARGUMENT,
                                     "Invalid NMEA GGA sentence");
                goto out;
        }

        altitude = parse_altitude_string (parts[9], parts[10]);

        hdop = g_ascii_strtod (parts[8], NULL);
        accuracy = get_accuracy_from_hdop (hdop);

        location = g_object_new (GCLUE_TYPE_LOCATION,
                                 "latitude", latitude,
                                 "longitude", longitude,
                                 "accuracy", accuracy,
                                 "timestamp", timestamp,
                                 NULL);
        if (altitude != GEOCODE_LOCATION_ALTITUDE_UNKNOWN)
                g_object_set (location, "altitude", altitude, NULL);

out:
        g_strfreev (parts);
        return location;
}

/**
 * gclue_location_duplicate:
 * @location: the #GClueLocation instance to duplicate.
 *
 * Creates a new copy of @location object.
 *
 * Returns: a new #GClueLocation object. Use g_object_unref() when done.
 **/
GClueLocation *
gclue_location_duplicate (GClueLocation *location)
{
        g_return_val_if_fail (GCLUE_IS_LOCATION (location), NULL);

        GeocodeLocation *gloc = GEOCODE_LOCATION (location);

        return g_object_new
                (GCLUE_TYPE_LOCATION,
                 "description", geocode_location_get_description (gloc),
                 "latitude", geocode_location_get_latitude (gloc),
                 "longitude", geocode_location_get_longitude (gloc),
                 "accuracy", geocode_location_get_accuracy (gloc),
                 "altitude", geocode_location_get_altitude (gloc),
                 "crs", geocode_location_get_crs (gloc),
                 "timestamp", geocode_location_get_timestamp (gloc),
                 "speed", location->priv->speed,
                 "heading", location->priv->heading,
                 NULL);
}

/**
 * gclue_location_get_speed:
 * @location: a #GClueLocation
 *
 * Gets the speed in meters per second.
 *
 * Returns: The speed, or %GCLUE_LOCATION_SPEED_UNKNOWN if speed in unknown.
 **/
gdouble
gclue_location_get_speed (GClueLocation *location)
{
        g_return_val_if_fail (GCLUE_IS_LOCATION (location),
                              GCLUE_LOCATION_SPEED_UNKNOWN);

        return location->priv->speed;
}

/**
 * gclue_location_set_speed:
 * @location: a #GClueLocation
 * @speed: speed in meters per second
 *
 * Sets the speed.
 **/
void
gclue_location_set_speed (GClueLocation *location,
                          gdouble        speed)
{
        location->priv->speed = speed;

        g_object_notify (G_OBJECT (location), "speed");
}

/**
 * gclue_location_set_speed_from_prev_location:
 * @location: a #GClueLocation
 * @prev_location: a #GClueLocation
 *
 * Calculates the speed based on provided previous location @prev_location
 * and sets it on @location.
 **/
void
gclue_location_set_speed_from_prev_location (GClueLocation *location,
                                             GClueLocation *prev_location)
{
        gdouble speed;
        guint64 timestamp, prev_timestamp;
        GeocodeLocation *gloc, *prev_gloc;

        g_return_if_fail (GCLUE_IS_LOCATION (location));
        g_return_if_fail (prev_location == NULL ||
                          GCLUE_IS_LOCATION (prev_location));

        if (prev_location == NULL) {
               speed = GCLUE_LOCATION_SPEED_UNKNOWN;

               goto out;
        }

        gloc = GEOCODE_LOCATION (location);
        prev_gloc = GEOCODE_LOCATION (prev_location);

        timestamp = geocode_location_get_timestamp (gloc);
        prev_timestamp = geocode_location_get_timestamp (prev_gloc);

        if (timestamp <= prev_timestamp) {
               speed = GCLUE_LOCATION_SPEED_UNKNOWN;

               goto out;
        }

        speed = geocode_location_get_distance_from (gloc, prev_gloc) *
                1000.0 / (timestamp - prev_timestamp);

out:
        location->priv->speed = speed;

        g_object_notify (G_OBJECT (location), "speed");
}

/**
 * gclue_location_get_heading:
 * @location: a #GClueLocation
 *
 * Gets the positive angle between direction of movement and North direction.
 * The angle is measured in degrees.
 *
 * Returns: The heading, or %GCLUE_LOCATION_HEADING_UNKNOWN if heading is
 *          unknown.
 **/
gdouble
gclue_location_get_heading (GClueLocation *location)
{
        g_return_val_if_fail (GCLUE_IS_LOCATION (location),
                              GCLUE_LOCATION_HEADING_UNKNOWN);

        return location->priv->heading;
}

/**
 * gclue_location_set_heading:
 * @location: a #GClueLocation
 * @heading: heading in degrees
 *
 * Sets the heading.
 **/
void
gclue_location_set_heading (GClueLocation *location,
                            gdouble        heading)
{
        location->priv->heading = heading;

        g_object_notify (G_OBJECT (location), "heading");
}

/**
 * gclue_location_set_heading_from_prev_location:
 * @location: a #GClueLocation
 * @prev_location: a #GClueLocation
 *
 * Calculates the heading direction in degrees with respect to North direction
 * based on provided @prev_location and sets it on @location.
 **/
void
gclue_location_set_heading_from_prev_location (GClueLocation *location,
                                               GClueLocation *prev_location)
{
        gdouble dx, dy, angle, lat, lon, prev_lat, prev_lon;
        GeocodeLocation *gloc, *prev_gloc;

        g_return_if_fail (GCLUE_IS_LOCATION (location));
        g_return_if_fail (prev_location == NULL ||
                          GCLUE_IS_LOCATION (prev_location));

        if (prev_location == NULL) {
               location->priv->heading = GCLUE_LOCATION_HEADING_UNKNOWN;

               return;
        }

        gloc = GEOCODE_LOCATION (location);
        prev_gloc = GEOCODE_LOCATION (prev_location);

        lat = geocode_location_get_latitude(gloc);
        lon = geocode_location_get_longitude(gloc);
        prev_lat = geocode_location_get_latitude(prev_gloc);
        prev_lon = geocode_location_get_longitude(prev_gloc);

        dx = (lat - prev_lat);
        dy = (lon - prev_lon);

        /* atan2 takes in coordinate values of a 2D space and returns the angle
         * which the line from origin to that coordinate makes with the positive
         * X-axis, in the range (-PI,+PI]. Converting it into degrees we get the
         * angle in range (-180,180]. This means East = 0 degree,
         * West = -180 degrees, North = 90 degrees, South = -90 degrees.
         *
         * Passing atan2 a negative value of dx will flip the angles about
         * Y-axis. This means the angle now returned will be the angle with
         * respect to negative X-axis. Which makes West = 0 degree,
         * East = 180 degrees, North = 90 degrees, South = -90 degrees. */
        angle = atan2(dy, -dx) * 180.0 / M_PI;

        /* Now, North is supposed to be 0 degree. Lets subtract 90 degrees
         * from angle. After this step West = -90 degrees, East = 90 degrees,
         * North = 0 degree, South = -180 degrees. */
        angle -= 90.0;

        /* As we know, angle ~= angle + 360; using this on negative values would
         * bring the the angle in range [0,360).
         *
         * After this step West = 270 degrees, East = 90 degrees,
         * North = 0 degree, South = 180 degrees. */
        if (angle < 0)
                angle += 360.0;

        location->priv->heading = angle;

        g_object_notify (G_OBJECT (location), "heading");
}
