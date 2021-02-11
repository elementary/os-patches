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

#ifndef GCLUE_MODEM_GPS_H
#define GCLUE_MODEM_GPS_H

#include <glib.h>
#include <gio/gio.h>
#include "gclue-location-source.h"

G_BEGIN_DECLS

GType gclue_modem_gps_get_type (void) G_GNUC_CONST;

#define GCLUE_TYPE_MODEM_GPS                  (gclue_modem_gps_get_type ())
#define GCLUE_MODEM_GPS(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_MODEM_GPS, GClueModemGPS))
#define GCLUE_IS_MODEM_GPS(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_MODEM_GPS))
#define GCLUE_MODEM_GPS_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GCLUE_TYPE_MODEM_GPS, GClueModemGPSClass))
#define GCLUE_IS_MODEM_GPS_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GCLUE_TYPE_MODEM_GPS))
#define GCLUE_MODEM_GPS_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GCLUE_TYPE_MODEM_GPS, GClueModemGPSClass))

/**
 * GClueModemGPS:
 *
 * All the fields in the #GClueModemGPS structure are private and should never be accessed directly.
**/
typedef struct _GClueModemGPS        GClueModemGPS;
typedef struct _GClueModemGPSClass   GClueModemGPSClass;
typedef struct _GClueModemGPSPrivate GClueModemGPSPrivate;

struct _GClueModemGPS {
        /* <private> */
        GClueLocationSource parent_instance;
        GClueModemGPSPrivate *priv;
};

/**
 * GClueModemGPSClass:
 *
 * All the fields in the #GClueModemGPSClass structure are private and should never be accessed directly.
**/
struct _GClueModemGPSClass {
        /* <private> */
        GClueLocationSourceClass parent_class;
};

GClueModemGPS * gclue_modem_gps_get_singleton (void);

G_END_DECLS

#endif /* GCLUE_MODEM_GPS_H */
