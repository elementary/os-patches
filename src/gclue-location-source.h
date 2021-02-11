/* vim: set et ts=8 sw=8: */
/*
 * Copyright 2014 Red Hat, Inc.
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

#ifndef GCLUE_LOCATION_SOURCE_H
#define GCLUE_LOCATION_SOURCE_H

#include <glib.h>
#include <gio/gio.h>
#include "gclue-enum-types.h"
#include "gclue-location.h"
#include "gclue-min-uint.h"

G_BEGIN_DECLS

#define GCLUE_TYPE_LOCATION_SOURCE            (gclue_location_source_get_type())
#define GCLUE_LOCATION_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_LOCATION_SOURCE, GClueLocationSource))
#define GCLUE_LOCATION_SOURCE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_LOCATION_SOURCE, GClueLocationSource const))
#define GCLUE_LOCATION_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCLUE_TYPE_LOCATION_SOURCE, GClueLocationSourceClass))
#define GCLUE_IS_LOCATION_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_LOCATION_SOURCE))
#define GCLUE_IS_LOCATION_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCLUE_TYPE_LOCATION_SOURCE))
#define GCLUE_LOCATION_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GCLUE_TYPE_LOCATION_SOURCE, GClueLocationSourceClass))

typedef struct _GClueLocationSource        GClueLocationSource;
typedef struct _GClueLocationSourceClass   GClueLocationSourceClass;
typedef struct _GClueLocationSourcePrivate GClueLocationSourcePrivate;

struct _GClueLocationSource
{
        GObject parent;

        /*< private >*/
        GClueLocationSourcePrivate *priv;
};

struct _GClueLocationSourceClass
{
        GObjectClass parent_class;

        gboolean (*start) (GClueLocationSource *source);
        gboolean (*stop)  (GClueLocationSource *source);
};

GType gclue_location_source_get_type (void) G_GNUC_CONST;

void              gclue_location_source_start (GClueLocationSource *source);
void              gclue_location_source_stop  (GClueLocationSource *source);
GClueLocation    *gclue_location_source_get_location
                                              (GClueLocationSource *source);
void              gclue_location_source_set_location
                                              (GClueLocationSource *source,
                                               GClueLocation       *location);
gboolean          gclue_location_source_get_active
                                              (GClueLocationSource *source);
GClueAccuracyLevel
                  gclue_location_source_get_available_accuracy_level
                                              (GClueLocationSource *source);
GClueMinUINT     *gclue_location_source_get_time_threshold
                                              (GClueLocationSource *source);

gboolean
gclue_location_source_get_compute_movement (GClueLocationSource *source);
void
gclue_location_source_set_compute_movement (GClueLocationSource *source,
                                            gboolean             compute);

G_END_DECLS

#endif /* GCLUE_LOCATION_SOURCE_H */
