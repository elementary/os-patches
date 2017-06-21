/* cloudprovider.h
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 * Copyright (C) 2017 Julius Haertl <jus@bitgrid.net>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CLOUD_PROVIDER_H
#define CLOUD_PROVIDER_H

#include <gio/gio.h>
#include "cloudprovider-generated.h"

G_BEGIN_DECLS

typedef enum {
  CLOUD_PROVIDER_STATUS_INVALID,
  CLOUD_PROVIDER_STATUS_IDLE,
  CLOUD_PROVIDER_STATUS_SYNCING,
  CLOUD_PROVIDER_STATUS_ERROR
} CloudProviderStatus;

#define TYPE_CLOUD_PROVIDER             (cloud_provider_get_type())
#define CLOUD_PROVIDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CLOUD_PROVIDER, CloudProvider))
#define CLOUD_PROVIDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CLOUD_PROVIDER, CloudProviderClass))
#define IS_CLOUD_PROVIDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CLOUD_PROVIDER))
#define IS_CLOUD_PROVIDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CLOUD_PROVIDER))
#define CLOUD_PROVIDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CLOUD_PROVIDER, CloudProviderClass))

typedef struct _CloudProvider CloudProvider;
typedef struct _CloudProviderClass CloudProviderClass;


struct _CloudProviderClass
{
  GObjectClass parent_class;
};

struct _CloudProvider
{
  GObject parent_instance;
};


GType cloud_provider_get_type (void) G_GNUC_CONST;
CloudProvider *cloud_provider_new (const gchar *bus_name,
                                          const gchar *object_path);

gchar* cloud_provider_get_name (CloudProvider *self);
CloudProviderStatus cloud_provider_get_status (CloudProvider *self);
GIcon *cloud_provider_get_icon (CloudProvider *self);
GMenuModel *cloud_provider_get_menu_model (CloudProvider *self);
GActionGroup* cloud_provider_get_action_group (CloudProvider *self);
gchar *cloud_provider_get_path (CloudProvider *self);

G_END_DECLS

#endif /* CLOUD_PROVIDER_H */
