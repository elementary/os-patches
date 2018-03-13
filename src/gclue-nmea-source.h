/* vim: set et ts=8 sw=8: */
/*
 * Copyright (C) 2014 Red Hat, Inc.
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
 * Authors: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *          Ankit (Verma) <ankitstarski@gmail.com>
 */

#ifndef GCLUE_NMEA_SOURCE_H
#define GCLUE_NMEA_SOURCE_H

#include <glib.h>
#include <gio/gio.h>
#include "gclue-location-source.h"

G_BEGIN_DECLS

GType gclue_nmea_source_get_type (void) G_GNUC_CONST;

#define GCLUE_TYPE_NMEA_SOURCE            (gclue_nmea_source_get_type ())
#define GCLUE_NMEA_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_NMEA_SOURCE, GClueNMEASource))
#define GCLUE_IS_NMEA_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_NMEA_SOURCE))
#define GCLUE_NMEA_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCLUE_TYPE_NMEA_SOURCE, GClueNMEASourceClass))
#define GCLUE_IS_NMEA_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GCLUE_TYPE_NMEA_SOURCE))
#define GCLUE_NMEA_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCLUE_TYPE_NMEA_SOURCE, GClueNMEASourceClass))

/**
 * GClueNMEASource:
 *
 * All the fields in the #GClueNMEASource structure are private and should never be accessed directly.
**/
typedef struct _GClueNMEASource        GClueNMEASource;
typedef struct _GClueNMEASourceClass   GClueNMEASourceClass;
typedef struct _GClueNMEASourcePrivate GClueNMEASourcePrivate;

struct _GClueNMEASource {
        /* <private> */
        GClueLocationSource parent_instance;
        GClueNMEASourcePrivate *priv;
};

/**
 * GClueNMEASourceClass:
 *
 * All the fields in the #GClueNMEASourceClass structure are private and should never be accessed directly.
**/
struct _GClueNMEASourceClass {
        /* <private> */
        GClueLocationSourceClass parent_class;
};

GClueNMEASource * gclue_nmea_source_get_singleton (void);

G_END_DECLS

#endif /* GCLUE_NMEA_SOURCE_H */
