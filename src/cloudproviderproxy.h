/* cloudproviderproxy.h
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

#ifndef CLOUD_PROVIDER_PROXY_H
#define CLOUD_PROVIDER_PROXY_H

#include <gio/gio.h>
#include "cloudprovider-generated.h"

G_BEGIN_DECLS

typedef enum {
  CLOUD_PROVIDER_STATUS_INVALID,
  CLOUD_PROVIDER_STATUS_IDLE,
  CLOUD_PROVIDER_STATUS_SYNCING,
  CLOUD_PROVIDER_STATUS_ERROR
} CloudProviderStatus;

#define TYPE_CLOUD_PROVIDER_PROXY             (cloud_provider_proxy_get_type())
#define CLOUD_PROVIDER_PROXY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CLOUD_PROVIDER_PROXY, CloudProviderProxy))
#define CLOUD_PROVIDER_PROXY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CLOUD_PROVIDER_PROXY, CloudProviderProxyClass))
#define IS_CLOUD_PROVIDER_PROXY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CLOUD_PROVIDER_PROXY))
#define IS_CLOUD_PROVIDER_PROXY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CLOUD_PROVIDER_PROXY))
#define CLOUD_PROVIDER_PROXY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CLOUD_PROVIDER_PROXY, CloudProviderProxyClass))

typedef struct _CloudProviderProxy CloudProviderProxy;
typedef struct _CloudProviderProxyClass CloudProviderProxyClass;


struct _CloudProviderProxyClass
{
  GObjectClass parent_class;
};

struct _CloudProviderProxy
{
  GObject parent_instance;
};


GType cloud_provider_proxy_get_type (void) G_GNUC_CONST;
CloudProviderProxy *cloud_provider_proxy_new (const gchar *bus_name,
                                          const gchar *object_path);

gchar* cloud_provider_proxy_get_name (CloudProviderProxy *self);
CloudProviderStatus cloud_provider_proxy_get_status (CloudProviderProxy *self);
gchar* cloud_provider_proxy_get_status_details (CloudProviderProxy *self);
GIcon *cloud_provider_proxy_get_icon (CloudProviderProxy *self);
GMenuModel *cloud_provider_proxy_get_menu_model (CloudProviderProxy *self);
GActionGroup* cloud_provider_proxy_get_action_group (CloudProviderProxy *self);
gchar *cloud_provider_proxy_get_path (CloudProviderProxy *self);
gchar *cloud_provider_proxy_get_owner (CloudProviderProxy *self);
gboolean cloud_provider_proxy_is_available(CloudProviderProxy *self);
void cloud_provider_proxy_update (CloudProviderProxy *self);
G_END_DECLS


#endif /* CLOUD_PROVIDER_PROXY_H */
