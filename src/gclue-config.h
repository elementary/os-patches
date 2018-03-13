/* vim: set et ts=8 sw=8: */
/* gclue-config.h
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
 * Authors: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 */

#ifndef GCLUE_CONFIG_H
#define GCLUE_CONFIG_H

#include <gio/gio.h>
#include "gclue-location.h"
#include "gclue-client-info.h"
#include "gclue-config.h"

G_BEGIN_DECLS

#define GCLUE_TYPE_CONFIG            (gclue_config_get_type())
#define GCLUE_CONFIG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_CONFIG, GClueConfig))
#define GCLUE_CONFIG_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCLUE_TYPE_CONFIG, GClueConfig const))
#define GCLUE_CONFIG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GCLUE_TYPE_CONFIG, GClueConfigClass))
#define GCLUE_IS_CONFIG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCLUE_TYPE_CONFIG))
#define GCLUE_IS_CONFIG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GCLUE_TYPE_CONFIG))
#define GCLUE_CONFIG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GCLUE_TYPE_CONFIG, GClueConfigClass))

typedef enum {
        GCLUE_APP_PERM_ALLOWED,
        GCLUE_APP_PERM_DISALLOWED,
        GCLUE_APP_PERM_ASK_AGENT
} GClueAppPerm;

typedef struct _GClueConfig        GClueConfig;
typedef struct _GClueConfigClass   GClueConfigClass;
typedef struct _GClueConfigPrivate GClueConfigPrivate;

struct _GClueConfig
{
        GObject parent;

        /*< private >*/
        GClueConfigPrivate *priv;
};

struct _GClueConfigClass
{
        GObjectClass parent_class;
};

GType gclue_config_get_type (void) G_GNUC_CONST;

GClueConfig *       gclue_config_get_singleton          (void);
gboolean            gclue_config_is_agent_allowed       (GClueConfig     *config,
                                                         const char      *desktop_id,
                                                         GClueClientInfo *agent_info);
GClueAppPerm        gclue_config_get_app_perm           (GClueConfig     *config,
                                                         const char      *desktop_id,
                                                         GClueClientInfo *app_info);
gboolean            gclue_config_is_system_component    (GClueConfig     *config,
                                                         const char      *desktop_id);
const char *        gclue_config_get_wifi_url           (GClueConfig     *config);
const char *        gclue_config_get_wifi_submit_url    (GClueConfig     *config);
const char *        gclue_config_get_wifi_submit_nick   (GClueConfig     *config);
void                gclue_config_set_wifi_submit_nick   (GClueConfig     *config,
                                                         const char      *nick);
gboolean            gclue_config_get_wifi_submit_data   (GClueConfig     *config);
gboolean            gclue_config_get_enable_nmea_source (GClueConfig     *config);
void                gclue_config_set_wifi_submit_data   (GClueConfig     *config,
                                                         gboolean         submit);

G_END_DECLS

#endif /* GCLUE_CONFIG_H */
