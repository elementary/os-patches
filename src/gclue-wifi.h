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

#ifndef GCLUE_WIFI_H
#define GCLUE_WIFI_H

#include <glib.h>
#include <gio/gio.h>
#include "gclue-web-source.h"

G_BEGIN_DECLS

GType gclue_wifi_get_type (void) G_GNUC_CONST;

#define GCLUE_TYPE_WIFI                  (gclue_wifi_get_type ())
#define GCLUE_WIFI(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_WIFI, GClueWifi))
#define GCLUE_IS_WIFI(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_WIFI))
#define GCLUE_WIFI_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GCLUE_TYPE_WIFI, GClueWifiClass))
#define GCLUE_IS_WIFI_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GCLUE_TYPE_WIFI))
#define GCLUE_WIFI_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GCLUE_TYPE_WIFI, GClueWifiClass))

/**
 * GClueWifi:
 *
 * All the fields in the #GClueWifi structure are private and should never be accessed directly.
**/
typedef struct _GClueWifi        GClueWifi;
typedef struct _GClueWifiClass   GClueWifiClass;
typedef struct _GClueWifiPrivate GClueWifiPrivate;

struct _GClueWifi {
        /* <private> */
        GClueWebSource parent_instance;
        GClueWifiPrivate *priv;
};

/**
 * GClueWifiClass:
 *
 * All the fields in the #GClueWifiClass structure are private and should never be accessed directly.
**/
struct _GClueWifiClass {
        /* <private> */
        GClueWebSourceClass parent_class;
};

GClueWifi *        gclue_wifi_get_singleton      (GClueAccuracyLevel level);
GClueAccuracyLevel gclue_wifi_get_accuracy_level (GClueWifi *wifi);

G_END_DECLS

#endif /* GCLUE_WIFI_H */
