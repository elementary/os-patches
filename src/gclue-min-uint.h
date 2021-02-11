/* vim: set et ts=8 sw=8: */
/* gclue-service-client.h
 *
 * Copyright 2018 Collabora Ltd.
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

#ifndef GCLUE_MIN_UINT_H
#define GCLUE_MIN_UINT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCLUE_TYPE_MIN_UINT            (gclue_min_uint_get_type())
#define GCLUE_MIN_UINT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_MIN_UINT, GClueMinUINT))
#define GCLUE_MIN_UINT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_MIN_UINT, GClueMinUINT const))
#define GCLUE_MIN_UINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCLUE_TYPE_MIN_UINT, GClueMinUINTClass))
#define GCLUE_IS_MIN_UINT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_MIN_UINT))
#define GCLUE_IS_MIN_UINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCLUE_TYPE_MIN_UINT))
#define GCLUE_MIN_UINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GCLUE_TYPE_MIN_UINT, GClueMinUINTClass))

typedef struct _GClueMinUINT        GClueMinUINT;
typedef struct _GClueMinUINTClass   GClueMinUINTClass;
typedef struct _GClueMinUINTPrivate GClueMinUINTPrivate;

struct _GClueMinUINT
{
        GObject parent;

        /*< private >*/
        GClueMinUINTPrivate *priv;
};

struct _GClueMinUINTClass
{
        GObjectClass parent_class;
};

GType             gclue_min_uint_get_type       (void) G_GNUC_CONST;

GClueMinUINT *    gclue_min_uint_new            (void);
guint             gclue_min_uint_get_value      (GClueMinUINT    *muint);
void              gclue_min_uint_add_value      (GClueMinUINT    *muint,
                                                 guint            value,
                                                 GObject         *owner);
void              gclue_min_uint_drop_value     (GClueMinUINT    *muint,
                                                 GObject         *owner);
G_END_DECLS

#endif /* GCLUE_MIN_UINT_H */
