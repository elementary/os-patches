/* vim: set et ts=8 sw=8: */
/* gclue-service-client.h
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

#ifndef GCLUE_SERVICE_CLIENT_H
#define GCLUE_SERVICE_CLIENT_H

#include <glib-object.h>
#include "gclue-client-interface.h"
#include "geoclue-agent-interface.h"
#include "gclue-client-info.h"

G_BEGIN_DECLS

#define GCLUE_TYPE_SERVICE_CLIENT            (gclue_service_client_get_type())
#define GCLUE_SERVICE_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_SERVICE_CLIENT, GClueServiceClient))
#define GCLUE_SERVICE_CLIENT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_SERVICE_CLIENT, GClueServiceClient const))
#define GCLUE_SERVICE_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCLUE_TYPE_SERVICE_CLIENT, GClueServiceClientClass))
#define GCLUE_IS_SERVICE_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_SERVICE_CLIENT))
#define GCLUE_IS_SERVICE_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCLUE_TYPE_SERVICE_CLIENT))
#define GCLUE_SERVICE_CLIENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GCLUE_TYPE_SERVICE_CLIENT, GClueServiceClientClass))

typedef struct _GClueServiceClient        GClueServiceClient;
typedef struct _GClueServiceClientClass   GClueServiceClientClass;
typedef struct _GClueServiceClientPrivate GClueServiceClientPrivate;

struct _GClueServiceClient
{
        GClueDBusClientSkeleton parent;

        /*< private >*/
        GClueServiceClientPrivate *priv;
};

struct _GClueServiceClientClass
{
        GClueDBusClientSkeletonClass parent_class;
};

GType gclue_service_client_get_type (void) G_GNUC_CONST;

GClueServiceClient * gclue_service_client_new             (GClueClientInfo *info,
                                                           const char      *path,
                                                           GDBusConnection *connection,
                                                           GClueAgent      *agent_proxy,
                                                           GError         **error);
const char *         gclue_service_client_get_path        (GClueServiceClient *client);
GClueClientInfo *    gclue_service_client_get_client_info (GClueServiceClient *client);

G_END_DECLS

#endif /* GCLUE_SERVICE_CLIENT_H */
