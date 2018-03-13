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

#ifndef GCLUE_3G_H
#define GCLUE_3G_H

#include <glib.h>
#include <gio/gio.h>
#include "gclue-web-source.h"

G_BEGIN_DECLS

GType gclue_3g_get_type (void) G_GNUC_CONST;

#define GCLUE_TYPE_3G                  (gclue_3g_get_type ())
#define GCLUE_3G(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_3G, GClue3G))
#define GCLUE_IS_3G(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_3G))
#define GCLUE_3G_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GCLUE_TYPE_3G, GClue3GClass))
#define GCLUE_IS_3G_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GCLUE_TYPE_3G))
#define GCLUE_3G_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GCLUE_TYPE_3G, GClue3GClass))

/**
 * GClue3G:
 *
 * All the fields in the #GClue3G structure are private and should never be accessed directly.
**/
typedef struct _GClue3G        GClue3G;
typedef struct _GClue3GClass   GClue3GClass;
typedef struct _GClue3GPrivate GClue3GPrivate;

struct _GClue3G {
        /* <private> */
        GClueWebSource parent_instance;
        GClue3GPrivate *priv;
};

/**
 * GClue3GClass:
 *
 * All the fields in the #GClue3GClass structure are private and should never be accessed directly.
**/
struct _GClue3GClass {
        /* <private> */
        GClueWebSourceClass parent_class;
};

GClue3G * gclue_3g_get_singleton (void);

G_END_DECLS

#endif /* GCLUE_3G_H */
