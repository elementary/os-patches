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

#ifndef GCLUE_WEB_SOURCE_H
#define GCLUE_WEB_SOURCE_H

#include <glib.h>
#include <gio/gio.h>
#include "gclue-location-source.h"
#include <libsoup/soup.h>

G_BEGIN_DECLS

GType gclue_web_source_get_type (void) G_GNUC_CONST;

#define GCLUE_TYPE_WEB_SOURCE                  (gclue_web_source_get_type ())
#define GCLUE_WEB_SOURCE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_WEB_SOURCE, GClueWebSource))
#define GCLUE_IS_WEB_SOURCE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_WEB_SOURCE))
#define GCLUE_WEB_SOURCE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GCLUE_TYPE_WEB_SOURCE, GClueWebSourceClass))
#define GCLUE_IS_WEB_SOURCE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GCLUE_TYPE_WEB_SOURCE))
#define GCLUE_WEB_SOURCE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GCLUE_TYPE_WEB_SOURCE, GClueWebSourceClass))

/**
 * GClueWebSource:
 *
 * All the fields in the #GClueWebSource structure are private and should never be accessed directly.
**/
typedef struct _GClueWebSource        GClueWebSource;
typedef struct _GClueWebSourceClass   GClueWebSourceClass;
typedef struct _GClueWebSourcePrivate GClueWebSourcePrivate;

struct _GClueWebSource {
        /* <private> */
        GClueLocationSource parent_instance;
        GClueWebSourcePrivate *priv;
};

/**
 * GClueWebSourceClass:
 *
 * All the fields in the #GClueWebSourceClass structure are private and should never be accessed directly.
**/
struct _GClueWebSourceClass {
        /* <private> */
        GClueLocationSourceClass parent_class;

        SoupMessage *     (*create_query)        (GClueWebSource *source,
                                                  GError        **error);
        SoupMessage *     (*create_submit_query) (GClueWebSource  *source,
                                                  GClueLocation   *location,
                                                  GError         **error);
        GClueLocation * (*parse_response)        (GClueWebSource *source,
                                                  const char     *response,
                                                  GError        **error);
        GClueAccuracyLevel (*get_available_accuracy_level)
                                                 (GClueWebSource *source,
                                                  gboolean        network_available);
};

void gclue_web_source_refresh           (GClueWebSource      *source);
void gclue_web_source_set_submit_source (GClueWebSource      *source,
                                         GClueLocationSource *submit_source);

G_END_DECLS

#endif /* GCLUE_WEB_SOURCE_H */
