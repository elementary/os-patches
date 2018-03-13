/* vim: set et ts=8 sw=8: */
/*
 * Geoclue convenience library.
 *
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#ifndef GCLUE_SIMPLE_H
#define GCLUE_SIMPLE_H

#include <glib-object.h>
#include <gio/gio.h>
#include "gclue-client.h"
#include "gclue-location.h"
#include "gclue-enum-types.h"

G_BEGIN_DECLS

#define GCLUE_TYPE_SIMPLE            (gclue_simple_get_type())
#define GCLUE_SIMPLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_SIMPLE, GClueSimple))
#define GCLUE_SIMPLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCLUE_TYPE_SIMPLE, GClueSimpleClass))
#define GCLUE_IS_SIMPLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_SIMPLE))
#define GCLUE_IS_SIMPLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCLUE_TYPE_SIMPLE))
#define GCLUE_SIMPLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GCLUE_TYPE_SIMPLE, GClueSimpleClass))

typedef struct _GClueSimple        GClueSimple;
typedef struct _GClueSimpleClass   GClueSimpleClass;
typedef struct _GClueSimplePrivate GClueSimplePrivate;

struct _GClueSimple
{
        GObject parent;

        /*< private >*/
        GClueSimplePrivate *priv;
};

struct _GClueSimpleClass
{
        GObjectClass parent_class;
};

GType           gclue_simple_get_type     (void) G_GNUC_CONST;

void            gclue_simple_new          (const char         *desktop_id,
                                           GClueAccuracyLevel  accuracy_level,
                                           GCancellable       *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer            user_data);
GClueSimple *   gclue_simple_new_finish   (GAsyncResult       *result,
                                           GError            **error);
GClueSimple *   gclue_simple_new_sync     (const char        *desktop_id,
                                           GClueAccuracyLevel accuracy_level,
                                           GCancellable      *cancellable,
                                           GError           **error);
GClueClient *   gclue_simple_get_client   (GClueSimple        *simple);
GClueLocation * gclue_simple_get_location (GClueSimple        *simple);

G_END_DECLS

#endif /* GCLUE_SIMPLE_H */
