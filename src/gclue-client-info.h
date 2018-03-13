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

#ifndef GCLUE_CLIENT_INFO_H
#define GCLUE_CLIENT_INFO_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GCLUE_TYPE_CLIENT_INFO            (gclue_client_info_get_type())
#define GCLUE_CLIENT_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_CLIENT_INFO, GClueClientInfo))
#define GCLUE_CLIENT_INFO_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_CLIENT_INFO, GClueClientInfo const))
#define GCLUE_CLIENT_INFO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCLUE_TYPE_CLIENT_INFO, GClueClientInfoClass))
#define GCLUE_IS_CLIENT_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_CLIENT_INFO))
#define GCLUE_IS_CLIENT_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCLUE_TYPE_CLIENT_INFO))
#define GCLUE_CLIENT_INFO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GCLUE_TYPE_CLIENT_INFO, GClueClientInfoClass))

typedef struct _GClueClientInfo        GClueClientInfo;
typedef struct _GClueClientInfoClass   GClueClientInfoClass;
typedef struct _GClueClientInfoPrivate GClueClientInfoPrivate;

struct _GClueClientInfo
{
        GObject parent;

        /*< private >*/
        GClueClientInfoPrivate *priv;
};

struct _GClueClientInfoClass
{
        GObjectClass parent_class;

        /* signals */
        void (* peer_vanished)  (GClueClientInfo *info);
};

GType             gclue_client_info_get_type       (void) G_GNUC_CONST;

void              gclue_client_info_new_async      (const char         *bus_name,
                                                    GDBusConnection    *connection,
                                                    GCancellable       *cancellable,
                                                    GAsyncReadyCallback callback,
                                                    gpointer            user_data);
GClueClientInfo * gclue_client_info_new_finish     (GAsyncResult       *res,
                                                    GError            **error);
const char *      gclue_client_info_get_bus_name   (GClueClientInfo    *info);
guint32           gclue_client_info_get_user_id    (GClueClientInfo    *info);
gboolean          gclue_client_info_check_bus_name (GClueClientInfo    *info,
                                                    const char         *bus_name);
const char *      gclue_client_info_get_xdg_id     (GClueClientInfo *info);

G_END_DECLS

#endif /* GCLUE_CLIENT_INFO_H */
