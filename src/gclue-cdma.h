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

#ifndef GCLUE_CDMA_H
#define GCLUE_CDMA_H

#include <glib.h>
#include <gio/gio.h>
#include "gclue-location-source.h"

G_BEGIN_DECLS

GType gclue_cdma_get_type (void) G_GNUC_CONST;

#define GCLUE_TYPE_CDMA                  (gclue_cdma_get_type ())
#define GCLUE_CDMA(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_CDMA, GClueCDMA))
#define GCLUE_IS_CDMA(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_CDMA))
#define GCLUE_CDMA_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GCLUE_TYPE_CDMA, GClueCDMAClass))
#define GCLUE_IS_CDMA_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GCLUE_TYPE_CDMA))
#define GCLUE_CDMA_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GCLUE_TYPE_CDMA, GClueCDMAClass))

/**
 * GClueCDMA:
 *
 * All the fields in the #GClueCDMA structure are private and should never be accessed directly.
**/
typedef struct _GClueCDMA        GClueCDMA;
typedef struct _GClueCDMAClass   GClueCDMAClass;
typedef struct _GClueCDMAPrivate GClueCDMAPrivate;

struct _GClueCDMA {
        /* <private> */
        GClueLocationSource parent_instance;
        GClueCDMAPrivate *priv;
};

/**
 * GClueCDMAClass:
 *
 * All the fields in the #GClueCDMAClass structure are private and should never be accessed directly.
**/
struct _GClueCDMAClass {
        /* <private> */
        GClueLocationSourceClass parent_class;
};

GClueCDMA * gclue_cdma_get_singleton (void);

G_END_DECLS

#endif /* GCLUE_CDMA_H */
