/* vim: set et ts=8 sw=8: */
/* gclue-location.h
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

#ifndef GCLUE_LOCATION_H
#define GCLUE_LOCATION_H

#include <glib-object.h>
#include <gio/gio.h>
#include "geocode-glib/geocode-location.h"

G_BEGIN_DECLS

#define GCLUE_TYPE_LOCATION            (gclue_location_get_type ())
#define GCLUE_LOCATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_LOCATION, GClueLocation))
#define GCLUE_IS_LOCATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_LOCATION))
#define GCLUE_LOCATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCLUE_TYPE_LOCATION, GClueLocationClass))
#define GCLUE_IS_LOCATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCLUE_TYPE_LOCATION))
#define GCLUE_LOCATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GCLUE_TYPE_LOCATION, GClueLocationClass))

#define INVALID_COORDINATE -G_MAXDOUBLE

typedef struct _GClueLocation        GClueLocation;
typedef struct _GClueLocationClass   GClueLocationClass;
typedef struct _GClueLocationPrivate GClueLocationPrivate;

struct _GClueLocation
{
        /* Parent instance structure */
        GeocodeLocation parent_instance;

        GClueLocationPrivate *priv;
};

struct _GClueLocationClass
{
        /* Parent class structure */
        GeocodeLocationClass parent_class;
};

GType gclue_location_get_type (void);

/**
 * GCLUE_LOCATION_HEADING_UNKNOWN:
 *
 * Constant representing unknown heading.
 */
#define GCLUE_LOCATION_HEADING_UNKNOWN -1.0

/**
 * GCLUE_LOCATION_SPEED_UNKNOWN:
 *
 * Constant representing unknown speed.
 */
#define GCLUE_LOCATION_SPEED_UNKNOWN -1.0

GClueLocation *gclue_location_new (gdouble latitude,
                                   gdouble longitude,
                                   gdouble accuracy);

GClueLocation *gclue_location_new_full
                                  (gdouble     latitude,
                                   gdouble     longitude,
                                   gdouble     accuracy,
                                   gdouble     speed,
                                   gdouble     heading,
                                   gdouble     altitude,
                                   guint64     timestamp,
                                   const char *description);

GClueLocation *gclue_location_create_from_gga
                                  (const char *gga,
                                   GError    **error);

GClueLocation *gclue_location_duplicate
                                  (GClueLocation *location);

void gclue_location_set_speed     (GClueLocation *loc,
                                   gdouble        speed);

void gclue_location_set_speed_from_prev_location
                                  (GClueLocation *location,
                                   GClueLocation *prev_location);

gdouble gclue_location_get_speed  (GClueLocation *loc);

void gclue_location_set_heading   (GClueLocation *loc,
                                   gdouble        heading);

void gclue_location_set_heading_from_prev_location
                                  (GClueLocation *location,
                                   GClueLocation *prev_location);


gdouble gclue_location_get_heading
                                  (GClueLocation *loc);

#endif /* GCLUE_LOCATION_H */
