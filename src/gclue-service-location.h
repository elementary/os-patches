/* vim: set et ts=8 sw=8: */
/* gclue-service-location.h
 *
 * Copyright (C) 2013 Red Hat, Inc.
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

#ifndef GCLUE_SERVICE_LOCATION_H
#define GCLUE_SERVICE_LOCATION_H

#include <glib-object.h>
#include "gclue-location.h"
#include "gclue-client-info.h"
#include "gclue-location-interface.h"

G_BEGIN_DECLS

#define GCLUE_TYPE_SERVICE_LOCATION            (gclue_service_location_get_type())
#define GCLUE_SERVICE_LOCATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_SERVICE_LOCATION, GClueServiceLocation))
#define GCLUE_SERVICE_LOCATION_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_SERVICE_LOCATION, GClueServiceLocation const))
#define GCLUE_SERVICE_LOCATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCLUE_TYPE_SERVICE_LOCATION, GClueServiceLocationClass))
#define GCLUE_IS_SERVICE_LOCATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_SERVICE_LOCATION))
#define GCLUE_IS_SERVICE_LOCATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCLUE_TYPE_SERVICE_LOCATION))
#define GCLUE_SERVICE_LOCATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GCLUE_TYPE_SERVICE_LOCATION, GClueServiceLocationClass))

typedef struct _GClueServiceLocation        GClueServiceLocation;
typedef struct _GClueServiceLocationClass   GClueServiceLocationClass;
typedef struct _GClueServiceLocationPrivate GClueServiceLocationPrivate;

struct _GClueServiceLocation
{
        GClueDBusLocationSkeleton parent;

        /*< private >*/
        GClueServiceLocationPrivate *priv;
};

struct _GClueServiceLocationClass
{
        GClueDBusLocationSkeletonClass parent_class;
};

GType gclue_service_location_get_type (void) G_GNUC_CONST;

GClueServiceLocation * gclue_service_location_new      (GClueClientInfo      *info,
                                                        const char           *path,
                                                        GDBusConnection      *connection,
                                                        GClueLocation        *location,
                                                        GError              **error);
const char *           gclue_service_location_get_path (GClueServiceLocation *location);

G_END_DECLS

#endif /* GCLUE_SERVICE_LOCATION_H */
