/* cloudprovidermanager.h
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
#ifndef CLOUD_PROVIDER_MANAGER_H
#define CLOUD_PROVIDER_MANAGER_H

#include <gio/gio.h>
#include "cloudproviderproxy.h"
#include "cloudprovidermanager-generated.h"

#define CLOUD_PROVIDER_MANAGER_DBUS_IFACE "org.freedesktop.CloudProviderManager1"
#define CLOUD_PROVIDER_MANAGER_DBUS_NAME  "org.freedesktop.CloudProviderManager"
#define CLOUD_PROVIDER_MANAGER_DBUS_PATH  "/org/freedesktop/CloudProviderManager"

G_BEGIN_DECLS

#define TYPE_CLOUD_PROVIDER_MANAGER             (cloud_provider_manager_get_type())
#define CLOUD_PROVIDER_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CLOUD_PROVIDER_MANAGER, CloudProviderManager))
#define CLOUD_PROVIDER_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CLOUD_PROVIDER_MANAGER, CloudProviderManagerClass))
#define IS_CLOUD_PROVIDER_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CLOUD_PROVIDER_MANAGER))
#define IS_CLOUD_PROVIDER_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CLOUD_PROVIDER_MANAGER))
#define CLOUD_PROVIDER_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CLOUD_PROVIDER_MANAGER, CloudProviderManagerClass))

typedef struct _CloudProviderManager CloudProviderManager;
typedef struct _CloudProviderManagerClass CloudProviderManagerClass;

struct _CloudProviderManagerClass
{
  GObjectClass parent_class;
};

struct _CloudProviderManager
{
  GObject parent_instance;
};

GType          cloud_provider_manager_get_type          (void) G_GNUC_CONST;
CloudProviderManager *cloud_provider_manager_dup_singleton (void);
//static void on_cloud_provider_changed (CloudProvider *cloud_provider, CloudProviderManager *self);
G_END_DECLS

#endif /* CLOUD_PROVIDER_MANAGER_H */
